#include "project/ProjectFile.h"
#include "compose/OutputCanvas.h"
#include "map/OutputMap.h"
#include "media/MediaBank.h"
#include "media/PlayerModule.h"
#include "output/OutputManager.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtMath>

bool ProjectFile::save(const QString &path,
                       const QVector<std::shared_ptr<PlayerModule>> &modules,
                       const MediaBank *bank,
                       const OutputCanvas *canvas,
                       const OutputManager *output,
                       const OutputMap *outputMap,
                       int fps,
                       const InputGuideState *guide,
                       const AlphaMaskState *alphaMask)
{
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("app"), QStringLiteral(MP_APP_ID));
    root.insert(QStringLiteral("fps"), fps);

    if (canvas) {
        QJsonObject c;
        c.insert(QStringLiteral("width"), canvas->size().width());
        c.insert(QStringLiteral("height"), canvas->size().height());
        root.insert(QStringLiteral("canvas"), c);
    }

    if (bank)
        root.insert(QStringLiteral("bankFolder"), bank->folder());

    if (output) {
        QJsonObject o;
        o.insert(QStringLiteral("ndiEnabled"), output->ndiEnabled());
        o.insert(QStringLiteral("spoutEnabled"), output->spoutEnabled());
        o.insert(QStringLiteral("ndiName"), output->ndiName());
        o.insert(QStringLiteral("spoutName"), output->spoutName());
        o.insert(QStringLiteral("ndiAlpha"), output->ndiSendAlpha());
        o.insert(QStringLiteral("ndiBandwidth"), static_cast<int>(output->ndiBandwidth()));
        root.insert(QStringLiteral("output"), o);
    }

    if (outputMap)
        root.insert(QStringLiteral("outputMap"), outputMap->toJson());

    if (guide && !guide->path.isEmpty()) {
        QJsonObject g;
        g.insert(QStringLiteral("path"), guide->path);
        g.insert(QStringLiteral("opacity"), static_cast<double>(guide->opacity));
        g.insert(QStringLiteral("visible"), guide->visible);
        g.insert(QStringLiteral("foreground"), guide->foreground);
        root.insert(QStringLiteral("inputGuide"), g);
    }

    if (alphaMask && !alphaMask->path.isEmpty()) {
        QJsonObject a;
        a.insert(QStringLiteral("path"), alphaMask->path);
        a.insert(QStringLiteral("opacity"), static_cast<double>(alphaMask->opacity));
        a.insert(QStringLiteral("visible"), alphaMask->visible);
        a.insert(QStringLiteral("inverted"), alphaMask->inverted);
        root.insert(QStringLiteral("alphaMask"), a);
    }

    QJsonArray mods;
    for (const auto &mod : modules) {
        if (!mod)
            continue;
        QJsonObject m;
        m.insert(QStringLiteral("index"), mod->index());
        m.insert(QStringLiteral("name"), mod->name());
        m.insert(QStringLiteral("path"), mod->decoder()->path());
        m.insert(QStringLiteral("loop"), mod->decoder()->isLooping());
        const auto &t = mod->transform();
        QJsonObject tr;
        tr.insert(QStringLiteral("cropLeft"), t.cropLeft);
        tr.insert(QStringLiteral("cropTop"), t.cropTop);
        tr.insert(QStringLiteral("cropRight"), t.cropRight);
        tr.insert(QStringLiteral("cropBottom"), t.cropBottom);
        tr.insert(QStringLiteral("posX"), t.posX);
        tr.insert(QStringLiteral("posY"), t.posY);
        tr.insert(QStringLiteral("scale"), t.scale);
        tr.insert(QStringLiteral("scaleY"), t.scaleY);
        tr.insert(QStringLiteral("opacity"), t.opacity);
        tr.insert(QStringLiteral("visible"), t.visible);
        m.insert(QStringLiteral("transform"), tr);
        m.insert(QStringLiteral("linkedSlice"), mod->linkedSliceName());
        m.insert(QStringLiteral("sliceAlign"), static_cast<int>(mod->sliceAlignMode()));
        m.insert(QStringLiteral("maskToSlice"), mod->maskToSlice());
        mods.append(m);
    }
    root.insert(QStringLiteral("modules"), mods);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool ProjectFile::load(const QString &path,
                       QVector<std::shared_ptr<PlayerModule>> &modules,
                       MediaBank *bank,
                       OutputCanvas *canvas,
                       OutputManager *output,
                       OutputMap *outputMap,
                       int *fps,
                       InputGuideState *guide,
                       AlphaMaskState *alphaMask)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;
    const QJsonObject root = doc.object();

    if (fps)
        *fps = root.value(QStringLiteral("fps")).toInt(30);

    if (canvas) {
        const QJsonObject c = root.value(QStringLiteral("canvas")).toObject();
        canvas->setSize(c.value(QStringLiteral("width")).toInt(1920),
                        c.value(QStringLiteral("height")).toInt(1080));
    }

    if (bank) {
        const QString folder = root.value(QStringLiteral("bankFolder")).toString();
        if (!folder.isEmpty())
            bank->setFolder(folder);
    }

    if (output) {
        const QJsonObject o = root.value(QStringLiteral("output")).toObject();
        output->setNdiEnabled(o.value(QStringLiteral("ndiEnabled")).toBool(true));
        output->setSpoutEnabled(o.value(QStringLiteral("spoutEnabled")).toBool(true));
        output->setNdiName(o.value(QStringLiteral("ndiName")).toString(QStringLiteral("MultiPlayer")));
        output->setSpoutName(
            o.value(QStringLiteral("spoutName")).toString(QStringLiteral("MultiPlayer")));
        output->setNdiSendAlpha(o.value(QStringLiteral("ndiAlpha")).toBool(true));
        output->setNdiBandwidth(
            static_cast<NdiBandwidth>(o.value(QStringLiteral("ndiBandwidth")).toInt(0)));
        if (canvas)
            output->setResolution(canvas->size().width(), canvas->size().height());
    }

    if (outputMap && root.contains(QStringLiteral("outputMap")))
        outputMap->fromJson(root.value(QStringLiteral("outputMap")).toObject());

    if (guide) {
        *guide = InputGuideState{};
        if (root.contains(QStringLiteral("inputGuide"))) {
            const QJsonObject g = root.value(QStringLiteral("inputGuide")).toObject();
            guide->path = g.value(QStringLiteral("path")).toString();
            guide->opacity = static_cast<float>(g.value(QStringLiteral("opacity")).toDouble(0.45));
            guide->visible = g.value(QStringLiteral("visible")).toBool(true);
            guide->foreground = g.value(QStringLiteral("foreground")).toBool(false);
        }
    }

    if (alphaMask) {
        *alphaMask = AlphaMaskState{};
        if (root.contains(QStringLiteral("alphaMask"))) {
            const QJsonObject a = root.value(QStringLiteral("alphaMask")).toObject();
            alphaMask->path = a.value(QStringLiteral("path")).toString();
            alphaMask->opacity = static_cast<float>(a.value(QStringLiteral("opacity")).toDouble(1.0));
            alphaMask->visible = a.value(QStringLiteral("visible")).toBool(true);
            alphaMask->inverted = a.value(QStringLiteral("inverted")).toBool(false);
        }
    }

    const QJsonArray mods = root.value(QStringLiteral("modules")).toArray();
    int needed = 0;
    for (const QJsonValue &v : mods) {
        const int index = v.toObject().value(QStringLiteral("index")).toInt(-1);
        if (index >= 0)
            needed = qMax(needed, index + 1);
    }
    if (needed < 1)
        needed = modules.isEmpty() ? MP_MODULE_DEFAULT : modules.size();
    needed = qBound(1, needed, MP_MODULE_MAX);
    while (modules.size() > needed)
        modules.removeLast();
    while (modules.size() < needed)
        modules.push_back(std::make_shared<PlayerModule>(modules.size()));

    for (const QJsonValue &v : mods) {
        const QJsonObject m = v.toObject();
        const int index = m.value(QStringLiteral("index")).toInt(-1);
        if (index < 0 || index >= modules.size() || !modules[index])
            continue;
        auto &mod = modules[index];
        mod->setIndex(index);
        const QString savedName = m.value(QStringLiteral("name")).toString();
        if (!savedName.isEmpty())
            mod->setName(savedName);
        else
            mod->setName(PlayerModule::defaultNameForIndex(index));
        const QString mediaPath = m.value(QStringLiteral("path")).toString();
        if (!mediaPath.isEmpty())
            mod->load(mediaPath);
        mod->decoder()->setLoop(m.value(QStringLiteral("loop")).toBool(true));
        const QJsonObject tr = m.value(QStringLiteral("transform")).toObject();
        auto &t = mod->transform();
        t.cropLeft = static_cast<float>(tr.value(QStringLiteral("cropLeft")).toDouble());
        t.cropTop = static_cast<float>(tr.value(QStringLiteral("cropTop")).toDouble());
        t.cropRight = static_cast<float>(tr.value(QStringLiteral("cropRight")).toDouble());
        t.cropBottom = static_cast<float>(tr.value(QStringLiteral("cropBottom")).toDouble());
        t.posX = static_cast<float>(tr.value(QStringLiteral("posX")).toDouble(t.posX));
        t.posY = static_cast<float>(tr.value(QStringLiteral("posY")).toDouble(t.posY));
        t.scale = static_cast<float>(tr.value(QStringLiteral("scale")).toDouble(1.0));
        t.scaleY = static_cast<float>(
            tr.value(QStringLiteral("scaleY")).toDouble(static_cast<double>(t.scale)));
        t.opacity = static_cast<float>(tr.value(QStringLiteral("opacity")).toDouble(1.0));
        t.visible = tr.value(QStringLiteral("visible")).toBool(true);
        mod->setLinkedSliceName(m.value(QStringLiteral("linkedSlice")).toString());
        const int align = m.value(QStringLiteral("sliceAlign")).toInt(0);
        mod->setSliceAlignMode(static_cast<SliceAlignMode>(qBound(0, align, 2)));
        mod->setMaskToSlice(m.value(QStringLiteral("maskToSlice")).toBool(true));
    }
    return true;
}
