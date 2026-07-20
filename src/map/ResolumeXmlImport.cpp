#include "map/ResolumeXmlImport.h"

#include <QFile>
#include <QXmlStreamReader>
#include <QtMath>
#include <algorithm>
#include <array>
#include <vector>

namespace {

bool parseRectAttrs(const QXmlStreamAttributes &attrs, QRectF *out)
{
    auto has = [&](const char *n) { return attrs.hasAttribute(QLatin1String(n)); };
    auto num = [&](const char *n) { return attrs.value(QLatin1String(n)).toDouble(); };

    if (has("l") && has("t") && has("r") && has("b")) {
        *out = QRectF(QPointF(num("l"), num("t")), QPointF(num("r"), num("b"))).normalized();
        return out->isValid();
    }
    if (has("x") && has("y") && has("width") && has("height")) {
        *out = QRectF(num("x"), num("y"), num("width"), num("height"));
        return out->isValid();
    }
    if (has("left") && has("top") && has("right") && has("bottom")) {
        *out = QRectF(QPointF(num("left"), num("top")), QPointF(num("right"), num("bottom")))
                   .normalized();
        return out->isValid();
    }
    return false;
}

double paramValue(const QXmlStreamAttributes &attrs)
{
    if (attrs.hasAttribute(QStringLiteral("value")))
        return attrs.value(QStringLiteral("value")).toDouble();
    if (attrs.hasAttribute(QStringLiteral("default")))
        return attrs.value(QStringLiteral("default")).toDouble();
    if (attrs.hasAttribute(QStringLiteral("Value")))
        return attrs.value(QStringLiteral("Value")).toDouble();
    return 0.0;
}

bool parsePointAttrs(const QXmlStreamAttributes &attrs, QPointF *out)
{
    auto has = [&](const QString &n) { return attrs.hasAttribute(n); };
    auto num = [&](const QString &n) { return attrs.value(n).toDouble(); };
    if (has(QStringLiteral("x")) && has(QStringLiteral("y"))) {
        *out = QPointF(num(QStringLiteral("x")), num(QStringLiteral("y")));
        return true;
    }
    if (has(QStringLiteral("X")) && has(QStringLiteral("Y"))) {
        *out = QPointF(num(QStringLiteral("X")), num(QStringLiteral("Y")));
        return true;
    }
    return false;
}

int cornerIndexFromName(const QString &raw)
{
    const QString n = raw.toLower();
    if (n.contains(QStringLiteral("topleft")) || n.contains(QStringLiteral("top-left"))
        || n == QStringLiteral("tl") || n.contains(QStringLiteral("ul")))
        return 0;
    if (n.contains(QStringLiteral("topright")) || n.contains(QStringLiteral("top-right"))
        || n == QStringLiteral("tr") || n.contains(QStringLiteral("ur")))
        return 1;
    if (n.contains(QStringLiteral("bottomright")) || n.contains(QStringLiteral("bottom-right"))
        || n == QStringLiteral("br") || n.contains(QStringLiteral("lr")))
        return 2;
    if (n.contains(QStringLiteral("bottomleft")) || n.contains(QStringLiteral("bottom-left"))
        || n == QStringLiteral("bl") || n.contains(QStringLiteral("ll")))
        return 3;
    return -1;
}

bool cornersDifferFromRect(const OutputSlice &s)
{
    const QPointF expected[4] = {s.outputRect.topLeft(), s.outputRect.topRight(),
                                 s.outputRect.bottomRight(), s.outputRect.bottomLeft()};
    for (int i = 0; i < 4; ++i) {
        if (qAbs(s.corners[i].x() - expected[i].x()) > 0.5
            || qAbs(s.corners[i].y() - expected[i].y()) > 0.5)
            return true;
    }
    return false;
}

// Snap float noise from Resolume XML (e.g. -6.1e-05 → 0, 2304.002 → 2304).
qreal snapCoord(qreal v, qreal eps = 0.05)
{
    if (qAbs(v) < eps)
        return 0.0;
    const qreal nearest = qRound(v);
    if (qAbs(v - nearest) < eps)
        return nearest;
    return v;
}

QPointF snapPoint(QPointF p)
{
    return QPointF(snapCoord(p.x()), snapCoord(p.y()));
}

QRectF snapRect(QRectF r)
{
    const QPointF tl = snapPoint(r.topLeft());
    const QPointF br = snapPoint(r.bottomRight());
    return QRectF(tl, br).normalized();
}

// Resolume Arena stores InputRect/OutputRect as 4 vertices (TL, TR, BR, BL).
QRectF boundingFromVerts(const std::vector<QPointF> &verts)
{
    if (verts.empty())
        return {};
    qreal minX = verts[0].x(), maxX = verts[0].x();
    qreal minY = verts[0].y(), maxY = verts[0].y();
    for (const QPointF &p : verts) {
        minX = qMin(minX, p.x());
        maxX = qMax(maxX, p.x());
        minY = qMin(minY, p.y());
        maxY = qMax(maxY, p.y());
    }
    return snapRect(QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized());
}

void assignCornersFromVerts(OutputSlice &s, const std::vector<QPointF> &verts)
{
    if (verts.size() >= 4) {
        // Arena order: TL, TR, BR, BL
        s.corners[0] = snapPoint(verts[0]);
        s.corners[1] = snapPoint(verts[1]);
        s.corners[2] = snapPoint(verts[2]);
        s.corners[3] = snapPoint(verts[3]);
    } else if (!s.outputRect.isEmpty()) {
        s.syncCornersFromRect();
    }
}

} // namespace

bool ResolumeXmlImport::importFile(const QString &path, OutputMap *map, QString *error)
{
    if (!map) {
        if (error)
            *error = QStringLiteral("map null");
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Impossible d'ouvrir le XML");
        return false;
    }

    QXmlStreamReader xml(&file);
    QVector<OutputSlice> slices;
    OutputSlice current;
    bool inSlice = false;
    bool inInput = false;
    bool inOutput = false;
    bool inInputRect = false;
    bool inOutputRect = false;
    bool inHomographyDst = false;
    bool inSoftEdgeParams = false;
    double screenW = map->outputSize().width();
    double screenH = map->outputSize().height();
    bool haveDeviceSize = false;

    double inX = 0, inY = 0, inW = 0, inH = 0;
    double outX = 0, outY = 0, outW = 0, outH = 0;
    bool haveIn = false, haveOut = false;
    std::array<bool, 4> haveCorner{{false, false, false, false}};
    int sequentialCorner = 0;
    std::vector<QPointF> inputVerts;
    std::vector<QPointF> outputVerts;
    std::vector<QPointF> homographyDst;

    auto flushSlice = [&]() {
        if (!inSlice)
            return;

        if (!inputVerts.empty()) {
            current.inputRect = boundingFromVerts(inputVerts);
            haveIn = true;
        } else if (haveIn) {
            current.inputRect = QRectF(inX, inY, inW, inH);
        }

        const bool hadOutputVerts = !outputVerts.empty();
        if (hadOutputVerts) {
            current.outputRect = boundingFromVerts(outputVerts);
            assignCornersFromVerts(current, outputVerts);
            haveOut = true;
            haveCorner = {{true, true, true, true}};
        } else if (haveOut) {
            current.outputRect = QRectF(outX, outY, outW, outH);
        }

        // Homography dst = warped output corners (Arena). Prefer over axis-aligned OutputRect.
        if (homographyDst.size() >= 4) {
            assignCornersFromVerts(current, homographyDst);
            haveCorner = {{true, true, true, true}};
            if (current.outputRect.isEmpty())
                current.outputRect = boundingFromVerts(homographyDst);
            haveOut = true;
        }

        if (current.inputRect.isEmpty() && !current.outputRect.isEmpty())
            current.inputRect = current.outputRect;
        if (current.outputRect.isEmpty() && !current.inputRect.isEmpty())
            current.outputRect = current.inputRect;

        const bool anyCorner =
            haveCorner[0] || haveCorner[1] || haveCorner[2] || haveCorner[3];
        if (!current.outputRect.isEmpty()) {
            if (!anyCorner) {
                current.syncCornersFromRect();
                current.useWarp = false;
            } else {
                const QPointF fallback[4] = {
                    current.outputRect.topLeft(), current.outputRect.topRight(),
                    current.outputRect.bottomRight(), current.outputRect.bottomLeft()};
                for (int i = 0; i < 4; ++i) {
                    if (!haveCorner[static_cast<size_t>(i)])
                        current.corners[i] = fallback[i];
                }
                // Keep OutputRect bounds when we already parsed vertices; only rebuild
                // the AABB from corners for legacy Point/Corner imports.
                if (!hadOutputVerts && homographyDst.size() < 4)
                    current.syncRectFromCorners();
                current.useWarp = cornersDifferFromRect(current);
            }
        }

        if (!current.inputRect.isEmpty() && !current.outputRect.isEmpty()) {
            if (current.name.isEmpty())
                current.name = QStringLiteral("Slice %1").arg(slices.size() + 1);
            if (!current.useWarp)
                current.syncCornersFromRect();
            slices.push_back(current);
        }
        current = OutputSlice{};
        haveIn = haveOut = false;
        haveCorner = {{false, false, false, false}};
        sequentialCorner = 0;
        inX = inY = inW = inH = 0;
        outX = outY = outW = outH = 0;
        inputVerts.clear();
        outputVerts.clear();
        homographyDst.clear();
        inInputRect = inOutputRect = inHomographyDst = false;
    };

    auto setCorner = [&](int idx, const QPointF &pt) {
        if (idx < 0 || idx > 3)
            return;
        current.corners[idx] = pt;
        haveCorner[static_cast<size_t>(idx)] = true;
        haveOut = true;
    };

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString();
            const auto attrs = xml.attributes();

            // Arena 7 composition / virtual device size
            if (name.compare(QStringLiteral("CurrentCompositionTextureSize"),
                             Qt::CaseInsensitive)
                == 0) {
                if (attrs.hasAttribute(QStringLiteral("width")))
                    screenW = attrs.value(QStringLiteral("width")).toDouble();
                if (attrs.hasAttribute(QStringLiteral("height")))
                    screenH = attrs.value(QStringLiteral("height")).toDouble();
            } else if (name.compare(QStringLiteral("OutputDeviceVirtual"), Qt::CaseInsensitive)
                           == 0
                       || name.compare(QStringLiteral("OutputDevice"), Qt::CaseInsensitive)
                              == 0) {
                if (attrs.hasAttribute(QStringLiteral("width"))
                    && attrs.hasAttribute(QStringLiteral("height"))) {
                    screenW = attrs.value(QStringLiteral("width")).toDouble();
                    screenH = attrs.value(QStringLiteral("height")).toDouble();
                    haveDeviceSize = true;
                }
            } else if (name.compare(QStringLiteral("Screen"), Qt::CaseInsensitive) == 0
                       || name.compare(QStringLiteral("Output"), Qt::CaseInsensitive) == 0) {
                if (attrs.hasAttribute(QStringLiteral("width")))
                    screenW = attrs.value(QStringLiteral("width")).toDouble();
                if (attrs.hasAttribute(QStringLiteral("height")))
                    screenH = attrs.value(QStringLiteral("height")).toDouble();
            }

            if (name.compare(QStringLiteral("Slice"), Qt::CaseInsensitive) == 0) {
                flushSlice();
                inSlice = true;
                current.name = attrs.value(QStringLiteral("name")).toString();
                if (current.name.isEmpty())
                    current.name = attrs.value(QStringLiteral("Name")).toString();
            } else if (inSlice
                       && name.compare(QStringLiteral("Params"), Qt::CaseInsensitive) == 0) {
                const QString pname = attrs.value(QStringLiteral("name")).toString();
                inInput = (pname.compare(QStringLiteral("Input"), Qt::CaseInsensitive) == 0);
                inOutput = (pname.compare(QStringLiteral("Output"), Qt::CaseInsensitive) == 0);
                inSoftEdgeParams =
                    (pname.compare(QStringLiteral("Soft Edge"), Qt::CaseInsensitive) == 0);
            } else if (inSlice
                       && name.compare(QStringLiteral("Input"), Qt::CaseInsensitive) == 0) {
                inInput = true;
                inOutput = false;
            } else if (inSlice
                       && name.compare(QStringLiteral("Output"), Qt::CaseInsensitive) == 0) {
                inOutput = true;
                inInput = false;
            } else if (inSlice
                       && name.compare(QStringLiteral("InputRect"), Qt::CaseInsensitive)
                              == 0) {
                inInputRect = true;
                inOutputRect = false;
                inputVerts.clear();
                QRectF r;
                if (parseRectAttrs(attrs, &r)) {
                    current.inputRect = r;
                    haveIn = true;
                }
            } else if (inSlice
                       && name.compare(QStringLiteral("OutputRect"), Qt::CaseInsensitive)
                              == 0) {
                inOutputRect = true;
                inInputRect = false;
                outputVerts.clear();
                QRectF r;
                if (parseRectAttrs(attrs, &r)) {
                    current.outputRect = r;
                    haveOut = true;
                }
            } else if (inSlice && name.compare(QStringLiteral("dst"), Qt::CaseInsensitive) == 0) {
                inHomographyDst = true;
                homographyDst.clear();
            } else if (inSlice
                       && (name.compare(QStringLiteral("Rect"), Qt::CaseInsensitive) == 0
                           || name.compare(QStringLiteral("Rectangle"), Qt::CaseInsensitive)
                                  == 0)) {
                QRectF r;
                if (parseRectAttrs(attrs, &r)) {
                    if (inOutput) {
                        current.outputRect = r;
                        haveOut = true;
                    } else {
                        current.inputRect = r;
                        haveIn = true;
                    }
                }
            } else if (inSlice && name.compare(QStringLiteral("v"), Qt::CaseInsensitive) == 0) {
                // Arena: only InputRect / OutputRect / Homography dst (ignore BezierWarper mesh)
                QPointF pt;
                if (parsePointAttrs(attrs, &pt)) {
                    if (inInputRect)
                        inputVerts.push_back(pt);
                    else if (inOutputRect)
                        outputVerts.push_back(pt);
                    else if (inHomographyDst)
                        homographyDst.push_back(pt);
                }
            } else if (inSlice
                       && (name.compare(QStringLiteral("Point"), Qt::CaseInsensitive) == 0
                           || name.compare(QStringLiteral("Vertex"), Qt::CaseInsensitive) == 0
                           || name.compare(QStringLiteral("Corner"), Qt::CaseInsensitive)
                                  == 0)) {
                QPointF pt;
                if (parsePointAttrs(attrs, &pt)) {
                    int idx = -1;
                    if (attrs.hasAttribute(QStringLiteral("index")))
                        idx = attrs.value(QStringLiteral("index")).toInt();
                    else if (attrs.hasAttribute(QStringLiteral("Index")))
                        idx = attrs.value(QStringLiteral("Index")).toInt();
                    else if (attrs.hasAttribute(QStringLiteral("id")))
                        idx = attrs.value(QStringLiteral("id")).toInt();
                    else {
                        const QString pn = attrs.value(QStringLiteral("name")).toString();
                        if (pn.isEmpty())
                            idx = cornerIndexFromName(
                                attrs.value(QStringLiteral("Name")).toString());
                        else
                            idx = cornerIndexFromName(pn);
                    }
                    if (idx < 0 && sequentialCorner < 4)
                        idx = sequentialCorner++;
                    if (idx >= 0 && idx < 4)
                        setCorner(idx, pt);
                }
            } else if (name.compare(QStringLiteral("Param"), Qt::CaseInsensitive) == 0
                       || name.compare(QStringLiteral("ParamRange"), Qt::CaseInsensitive) == 0
                       || name.compare(QStringLiteral("ParamDouble"), Qt::CaseInsensitive) == 0
                       || name.compare(QStringLiteral("ParamFloat"), Qt::CaseInsensitive)
                              == 0) {
                const QString pname = attrs.value(QStringLiteral("name")).toString();
                const QString plower = pname.toLower();
                const double val = paramValue(attrs);

                // Screen Width/Height (Arena Screen Params) when no OutputDevice*
                if (!haveDeviceSize) {
                    if (pname.compare(QStringLiteral("Width"), Qt::CaseInsensitive) == 0
                        && val > 1.0)
                        screenW = val;
                    if (pname.compare(QStringLiteral("Height"), Qt::CaseInsensitive) == 0
                        && val > 1.0)
                        screenH = val;
                }

                if (!inSlice)
                    continue;

                // Slice display name (Arena: Params/Common/Param Name)
                if (pname.compare(QStringLiteral("Name"), Qt::CaseInsensitive) == 0
                    && attrs.hasAttribute(QStringLiteral("value"))) {
                    const QString v = attrs.value(QStringLiteral("value")).toString();
                    if (!v.isEmpty()
                        && (current.name.isEmpty()
                            || current.name.startsWith(QStringLiteral("Slice "))))
                        current.name = v;
                }

                // Soft-edge / blend (fraction of slice)
                if (inSoftEdgeParams || plower.contains(QStringLiteral("soft"))
                    || plower.contains(QStringLiteral("feather"))
                    || plower.contains(QStringLiteral("blend"))) {
                    const float f = static_cast<float>(qBound(0.0, val, 0.49));
                    if (plower.contains(QStringLiteral("left")) || plower.endsWith(QStringLiteral(" l")))
                        current.softLeft = f;
                    else if (plower.contains(QStringLiteral("right"))
                             || plower.endsWith(QStringLiteral(" r")))
                        current.softRight = f;
                    else if (plower.contains(QStringLiteral("top"))
                             || plower.endsWith(QStringLiteral(" t")))
                        current.softTop = f;
                    else if (plower.contains(QStringLiteral("bottom"))
                             || plower.endsWith(QStringLiteral(" b")))
                        current.softBottom = f;
                }

                if (plower.contains(QStringLiteral("input")) || inInput) {
                    if (plower.contains(QStringLiteral("left")) || plower == QStringLiteral("x")
                        || plower.endsWith(QStringLiteral(" l"))) {
                        inX = val;
                        haveIn = true;
                    } else if (plower.contains(QStringLiteral("top")) || plower == QStringLiteral("y")
                               || plower.endsWith(QStringLiteral(" t"))) {
                        inY = val;
                        haveIn = true;
                    } else if (plower.contains(QStringLiteral("width"))
                               || plower.endsWith(QStringLiteral(" w"))) {
                        inW = val;
                        haveIn = true;
                    } else if (plower.contains(QStringLiteral("height"))
                               || plower.endsWith(QStringLiteral(" h"))) {
                        inH = val;
                        haveIn = true;
                    }
                }
                if (plower.contains(QStringLiteral("output")) || inOutput) {
                    if (plower.contains(QStringLiteral("left")) || plower == QStringLiteral("x")) {
                        outX = val;
                        haveOut = true;
                    } else if (plower.contains(QStringLiteral("top"))
                               || plower == QStringLiteral("y")) {
                        outY = val;
                        haveOut = true;
                    } else if (plower.contains(QStringLiteral("width"))) {
                        outW = val;
                        haveOut = true;
                    } else if (plower.contains(QStringLiteral("height"))) {
                        outH = val;
                        haveOut = true;
                    }
                }
            }
        } else if (xml.isEndElement()) {
            const QString name = xml.name().toString();
            if (name.compare(QStringLiteral("Slice"), Qt::CaseInsensitive) == 0) {
                flushSlice();
                inSlice = false;
                inInput = inOutput = false;
                inSoftEdgeParams = false;
            } else if (name.compare(QStringLiteral("Input"), Qt::CaseInsensitive) == 0) {
                inInput = false;
            } else if (name.compare(QStringLiteral("Output"), Qt::CaseInsensitive) == 0) {
                inOutput = false;
            } else if (name.compare(QStringLiteral("Params"), Qt::CaseInsensitive) == 0) {
                inInput = inOutput = false;
                inSoftEdgeParams = false;
            } else if (name.compare(QStringLiteral("InputRect"), Qt::CaseInsensitive) == 0) {
                inInputRect = false;
            } else if (name.compare(QStringLiteral("OutputRect"), Qt::CaseInsensitive) == 0) {
                inOutputRect = false;
            } else if (name.compare(QStringLiteral("dst"), Qt::CaseInsensitive) == 0) {
                inHomographyDst = false;
            }
        }
    }

    if (xml.hasError()) {
        if (error)
            *error = xml.errorString();
        return false;
    }

    if (slices.isEmpty()) {
        if (error)
            *error = QStringLiteral(
                "Aucune slice rectangulaire trouvée (format Resolume non reconnu).");
        return false;
    }

    // Arena XML stores layers bottom→top (z-order); UI tree is top→bottom → reverse.
    std::reverse(slices.begin(), slices.end());

    // Prefer virtual device size when present (actual output), else composition size.
    map->setOutputSize(static_cast<int>(qRound(screenW)), static_cast<int>(qRound(screenH)));
    map->clear();
    for (const OutputSlice &s : slices)
        map->addSlice(s);
    map->setResolumeImported(true);
    return true;
}
