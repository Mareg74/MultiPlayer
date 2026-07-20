#include "map/OutputMap.h"

#include <QPainter>
#include <QTransform>
#include <QtMath>

void OutputSlice::syncCornersFromRect()
{
    corners[0] = outputRect.topLeft();
    corners[1] = outputRect.topRight();
    corners[2] = outputRect.bottomRight();
    corners[3] = outputRect.bottomLeft();
}

void OutputSlice::syncRectFromCorners()
{
    qreal minX = corners[0].x(), maxX = corners[0].x();
    qreal minY = corners[0].y(), maxY = corners[0].y();
    for (int i = 1; i < 4; ++i) {
        minX = qMin(minX, corners[i].x());
        maxX = qMax(maxX, corners[i].x());
        minY = qMin(minY, corners[i].y());
        maxY = qMax(maxY, corners[i].y());
    }
    outputRect = QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

void OutputMap::clear()
{
    m_slices.clear();
}

void OutputMap::setOutputSize(int width, int height)
{
    m_outputSize = QSize(qMax(16, width), qMax(16, height));
}

void OutputMap::addSlice(const OutputSlice &slice)
{
    OutputSlice s = slice;
    if (!s.useWarp)
        s.syncCornersFromRect();
    m_slices.push_back(s);
}

void OutputMap::resetFullscreen(int canvasW, int canvasH)
{
    setOutputSize(canvasW, canvasH);
    m_slices.clear();
    m_resolumeImported = false;
    OutputSlice s;
    s.name = QStringLiteral("Fullscreen");
    s.inputRect = QRectF(0, 0, canvasW, canvasH);
    s.outputRect = QRectF(0, 0, m_outputSize.width(), m_outputSize.height());
    s.syncCornersFromRect();
    s.useWarp = false;
    m_slices.push_back(s);
}

void OutputMap::matchCanvas(int canvasW, int canvasH)
{
    canvasW = qMax(16, canvasW);
    canvasH = qMax(16, canvasH);

    const bool simpleFullscreen =
        m_slices.isEmpty()
        || (m_slices.size() == 1 && !m_slices[0].useWarp && m_slices[0].softLeft <= 0.f
            && m_slices[0].softRight <= 0.f && m_slices[0].softTop <= 0.f
            && m_slices[0].softBottom <= 0.f);

    if (simpleFullscreen) {
        resetFullscreen(canvasW, canvasH);
        return;
    }

    const QSize oldOut = m_outputSize;
    const qreal sxIn = oldOut.width() > 0 ? static_cast<qreal>(canvasW) / oldOut.width() : 1.0;
    const qreal syIn = oldOut.height() > 0 ? static_cast<qreal>(canvasH) / oldOut.height() : 1.0;
    setOutputSize(canvasW, canvasH);
    const qreal sxOut =
        oldOut.width() > 0 ? static_cast<qreal>(m_outputSize.width()) / oldOut.width() : 1.0;
    const qreal syOut =
        oldOut.height() > 0 ? static_cast<qreal>(m_outputSize.height()) / oldOut.height() : 1.0;

    for (OutputSlice &s : m_slices) {
        s.inputRect =
            QRectF(s.inputRect.x() * sxIn, s.inputRect.y() * syIn, s.inputRect.width() * sxIn,
                   s.inputRect.height() * syIn);
        for (int i = 0; i < 4; ++i) {
            s.corners[i].rx() *= sxOut;
            s.corners[i].ry() *= syOut;
        }
        s.syncRectFromCorners();
    }
}

bool OutputMap::isPassthrough(QSize canvasSize) const
{
    if (canvasSize.width() < 1 || canvasSize.height() < 1)
        return false;
    if (m_outputSize != canvasSize)
        return false;

    if (m_slices.isEmpty())
        return true;

    if (m_slices.size() != 1)
        return false;

    const OutputSlice &s = m_slices[0];
    if (!s.enabled || s.useWarp)
        return false;
    if (s.softLeft > 0.f || s.softRight > 0.f || s.softTop > 0.f || s.softBottom > 0.f)
        return false;

    const QRect in = s.inputRect.toAlignedRect();
    const QRect out = s.outputRect.toAlignedRect();
    return in.contains(QRect(0, 0, canvasSize.width(), canvasSize.height()))
           && out.size() == canvasSize && qAbs(out.left()) < 2 && qAbs(out.top()) < 2;
}

QImage OutputMap::extractWithSoftEdge(const QImage &composed, const OutputSlice &s)
{
    QImage patch = composed.copy(s.inputRect.toAlignedRect());
    if (patch.isNull())
        return {};

    patch = patch.convertToFormat(QImage::Format_RGBA8888);
    const bool anySoft = s.softLeft > 0.f || s.softRight > 0.f || s.softTop > 0.f
                         || s.softBottom > 0.f;
    if (!anySoft)
        return patch;

    const int w = patch.width();
    const int h = patch.height();
    const int featherL = qBound(0, static_cast<int>(s.softLeft * w), w / 2);
    const int featherR = qBound(0, static_cast<int>(s.softRight * w), w / 2);
    const int featherT = qBound(0, static_cast<int>(s.softTop * h), h / 2);
    const int featherB = qBound(0, static_cast<int>(s.softBottom * h), h / 2);

    for (int y = 0; y < h; ++y) {
        auto *line = reinterpret_cast<QRgb *>(patch.scanLine(y));
        float edgeY = 1.f;
        if (featherT > 0 && y < featherT)
            edgeY = static_cast<float>(y) / static_cast<float>(featherT);
        else if (featherB > 0 && y >= h - featherB)
            edgeY = static_cast<float>(h - 1 - y) / static_cast<float>(featherB);

        for (int x = 0; x < w; ++x) {
            float edgeX = 1.f;
            if (featherL > 0 && x < featherL)
                edgeX = static_cast<float>(x) / static_cast<float>(featherL);
            else if (featherR > 0 && x >= w - featherR)
                edgeX = static_cast<float>(w - 1 - x) / static_cast<float>(featherR);

            const float aMul = qBound(0.f, edgeX * edgeY, 1.f);
            if (aMul >= 0.999f)
                continue;
            QRgb px = line[x];
            const int a = qRound(qAlpha(px) * aMul);
            line[x] = qRgba(qRed(px), qGreen(px), qBlue(px), a);
        }
    }
    return patch;
}

void OutputMap::drawWarped(QPainter *p, const QImage &src, const OutputSlice &s)
{
    if (src.isNull())
        return;

    QPolygonF srcQuad;
    srcQuad << QPointF(0, 0) << QPointF(src.width(), 0) << QPointF(src.width(), src.height())
            << QPointF(0, src.height());

    QPolygonF dstQuad;
    dstQuad << s.corners[0] << s.corners[1] << s.corners[2] << s.corners[3];

    QTransform xform;
    if (!QTransform::quadToQuad(srcQuad, dstQuad, xform)) {
        // Fallback axis-aligned
        p->drawImage(s.outputRect, src);
        return;
    }

    p->save();
    p->setTransform(xform);
    p->drawImage(QPointF(0, 0), src);
    p->restore();
}

QImage OutputMap::apply(const QImage &composed) const
{
    if (composed.isNull())
        return {};

    if (isPassthrough(composed.size()))
        return composed;

    if (m_slices.isEmpty())
        return composed.scaled(m_outputSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QImage out(m_outputSize, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    for (const OutputSlice &s : m_slices) {
        if (!s.enabled || s.inputRect.isEmpty())
            continue;

        const QImage patch = extractWithSoftEdge(composed, s);
        if (patch.isNull())
            continue;

        if (s.useWarp) {
            drawWarped(&p, patch, s);
        } else {
            p.drawImage(s.outputRect, patch);
        }
    }
    p.end();
    return out.convertToFormat(QImage::Format_ARGB32);
}

QJsonObject OutputMap::toJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("width"), m_outputSize.width());
    root.insert(QStringLiteral("height"), m_outputSize.height());
    QJsonArray arr;
    for (const OutputSlice &s : m_slices) {
        QJsonObject o;
        o.insert(QStringLiteral("name"), s.name);
        o.insert(QStringLiteral("enabled"), s.enabled);
        o.insert(QStringLiteral("useWarp"), s.useWarp);
        o.insert(QStringLiteral("inX"), s.inputRect.x());
        o.insert(QStringLiteral("inY"), s.inputRect.y());
        o.insert(QStringLiteral("inW"), s.inputRect.width());
        o.insert(QStringLiteral("inH"), s.inputRect.height());
        o.insert(QStringLiteral("outX"), s.outputRect.x());
        o.insert(QStringLiteral("outY"), s.outputRect.y());
        o.insert(QStringLiteral("outW"), s.outputRect.width());
        o.insert(QStringLiteral("outH"), s.outputRect.height());
        o.insert(QStringLiteral("softL"), s.softLeft);
        o.insert(QStringLiteral("softR"), s.softRight);
        o.insert(QStringLiteral("softT"), s.softTop);
        o.insert(QStringLiteral("softB"), s.softBottom);
        QJsonArray corners;
        for (int i = 0; i < 4; ++i) {
            QJsonObject c;
            c.insert(QStringLiteral("x"), s.corners[i].x());
            c.insert(QStringLiteral("y"), s.corners[i].y());
            corners.append(c);
        }
        o.insert(QStringLiteral("corners"), corners);
        arr.append(o);
    }
    root.insert(QStringLiteral("slices"), arr);
    root.insert(QStringLiteral("resolumeImported"), m_resolumeImported);
    return root;
}

void OutputMap::fromJson(const QJsonObject &obj)
{
    m_outputSize = QSize(obj.value(QStringLiteral("width")).toInt(1920),
                         obj.value(QStringLiteral("height")).toInt(1080));
    m_resolumeImported = obj.value(QStringLiteral("resolumeImported")).toBool(false);
    m_slices.clear();
    const QJsonArray arr = obj.value(QStringLiteral("slices")).toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        OutputSlice s;
        s.name = o.value(QStringLiteral("name")).toString(QStringLiteral("Slice"));
        s.enabled = o.value(QStringLiteral("enabled")).toBool(true);
        s.useWarp = o.value(QStringLiteral("useWarp")).toBool(false);
        s.inputRect = QRectF(o.value(QStringLiteral("inX")).toDouble(),
                             o.value(QStringLiteral("inY")).toDouble(),
                             o.value(QStringLiteral("inW")).toDouble(),
                             o.value(QStringLiteral("inH")).toDouble());
        s.outputRect = QRectF(o.value(QStringLiteral("outX")).toDouble(),
                              o.value(QStringLiteral("outY")).toDouble(),
                              o.value(QStringLiteral("outW")).toDouble(),
                              o.value(QStringLiteral("outH")).toDouble());
        s.softLeft = static_cast<float>(o.value(QStringLiteral("softL")).toDouble());
        s.softRight = static_cast<float>(o.value(QStringLiteral("softR")).toDouble());
        s.softTop = static_cast<float>(o.value(QStringLiteral("softT")).toDouble());
        s.softBottom = static_cast<float>(o.value(QStringLiteral("softB")).toDouble());
        const QJsonArray corners = o.value(QStringLiteral("corners")).toArray();
        if (corners.size() == 4) {
            for (int i = 0; i < 4; ++i) {
                const QJsonObject c = corners[i].toObject();
                s.corners[i] = QPointF(c.value(QStringLiteral("x")).toDouble(),
                                       c.value(QStringLiteral("y")).toDouble());
            }
        } else {
            s.syncCornersFromRect();
        }
        m_slices.push_back(s);
    }
    // Older projects may have multi-slice maps without the flag.
    if (!m_resolumeImported && m_slices.size() > 1)
        m_resolumeImported = true;
}
