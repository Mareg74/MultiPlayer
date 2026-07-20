#include "control/ControlRouter.h"
#include "media/PlayerModule.h"

#include <QMetaType>
#include <QRegularExpression>
#include <QtGlobal>

ControlRouter::ControlRouter(QObject *parent)
    : QObject(parent)
{
}

void ControlRouter::setModules(QVector<std::shared_ptr<PlayerModule>> *modules)
{
    m_modules = modules;
}

void ControlRouter::setOutputToggle(std::function<void(bool)> fn)
{
    m_outputToggle = std::move(fn);
}

void ControlRouter::setOutputQuery(std::function<bool()> fn)
{
    m_outputQuery = std::move(fn);
}

void ControlRouter::setSelectModule(std::function<void(int)> fn)
{
    m_selectModule = std::move(fn);
}

void ControlRouter::setTransformNotify(std::function<void()> fn)
{
    m_transformNotify = std::move(fn);
}

void ControlRouter::setPlayAll(std::function<void()> fn)
{
    m_playAll = std::move(fn);
}

void ControlRouter::setPauseAll(std::function<void()> fn)
{
    m_pauseAll = std::move(fn);
}

void ControlRouter::setStopAll(std::function<void()> fn)
{
    m_stopAll = std::move(fn);
}

void ControlRouter::setToggleAll(std::function<void()> fn)
{
    m_toggleAll = std::move(fn);
}

bool ControlRouter::parseModuleAddress(const QString &address, int *moduleIndex,
                                       QString *command) const
{
    // /mp/module/0/play  or /mp/module/1/opacity
    static const QRegularExpression re(
        QStringLiteral("^/mp/module/(\\d+)/([a-zA-Z_]+)$"));
    const auto m = re.match(address);
    if (!m.hasMatch())
        return false;
    *moduleIndex = m.captured(1).toInt();
    *command = m.captured(2).toLower();
    return true;
}

void ControlRouter::handleOsc(const QString &address, const QVariantList &args)
{
    emit logMessage(QStringLiteral("OSC %1 (%2 args)").arg(address).arg(args.size()));

    if (address == QStringLiteral("/mp/output")) {
        bool on = true;
        if (!args.isEmpty()) {
            const QVariant &v = args.first();
            if (v.typeId() == QMetaType::Bool)
                on = v.toBool();
            else
                on = v.toDouble() >= 0.5;
        } else if (m_outputQuery) {
            on = !m_outputQuery();
        }
        if (m_outputToggle)
            m_outputToggle(on);
        return;
    }

    if (address == QStringLiteral("/mp/select") && !args.isEmpty()) {
        if (m_selectModule)
            m_selectModule(args.first().toInt());
        return;
    }

    if (address == QStringLiteral("/mp/play") || address == QStringLiteral("/mp/play_all")) {
        if (m_playAll)
            m_playAll();
        return;
    }
    if (address == QStringLiteral("/mp/pause") || address == QStringLiteral("/mp/pause_all")) {
        if (m_pauseAll)
            m_pauseAll();
        return;
    }
    if (address == QStringLiteral("/mp/stop") || address == QStringLiteral("/mp/stop_all")) {
        if (m_stopAll)
            m_stopAll();
        return;
    }
    if (address == QStringLiteral("/mp/toggle") || address == QStringLiteral("/mp/toggle_all")) {
        if (m_toggleAll)
            m_toggleAll();
        return;
    }

    int index = -1;
    QString cmd;
    if (parseModuleAddress(address, &index, &cmd)) {
        applyModuleCommand(index, cmd, args);
        return;
    }
}

void ControlRouter::handleMidiAction(const QString &action, float value01)
{
    emit logMessage(QStringLiteral("MIDI %1 = %2").arg(action).arg(value01, 0, 'f', 2));
    applyNamedAction(action, value01);
}

void ControlRouter::applyNamedAction(const QString &action, float value01)
{
    if (action == QStringLiteral("output")) {
        const bool on = value01 >= 0.5f;
        if (m_outputToggle)
            m_outputToggle(on);
        return;
    }
    if (action == QStringLiteral("output_toggle")) {
        bool on = true;
        if (m_outputQuery)
            on = !m_outputQuery();
        if (m_outputToggle)
            m_outputToggle(on);
        return;
    }
    if (action == QStringLiteral("play_all")) {
        if (value01 >= 0.5f && m_playAll)
            m_playAll();
        return;
    }
    if (action == QStringLiteral("pause_all")) {
        if (value01 >= 0.5f && m_pauseAll)
            m_pauseAll();
        return;
    }
    if (action == QStringLiteral("stop_all")) {
        if (value01 >= 0.5f && m_stopAll)
            m_stopAll();
        return;
    }
    if (action == QStringLiteral("toggle_all")) {
        if (value01 >= 0.5f && m_toggleAll)
            m_toggleAll();
        return;
    }

    // module.N.command
    const QStringList parts = action.split('.');
    if (parts.size() >= 3 && parts[0] == QStringLiteral("module")) {
        const int index = parts[1].toInt();
        const QString cmd = parts[2];
        QVariantList args;
        if (cmd == QStringLiteral("opacity") || cmd == QStringLiteral("scale")
            || cmd == QStringLiteral("seek"))
            args.append(static_cast<double>(value01));
        else if (cmd == QStringLiteral("play") || cmd == QStringLiteral("pause")
                 || cmd == QStringLiteral("stop") || cmd == QStringLiteral("toggle")) {
            if (value01 < 0.5f && cmd != QStringLiteral("toggle"))
                return; // ignore note/cc release for one-shots except toggle
        }
        applyModuleCommand(index, cmd, args);
        if (parts.size() >= 3 && m_selectModule)
            m_selectModule(index);
    }
}

void ControlRouter::applyModuleCommand(int index, const QString &command,
                                       const QVariantList &args)
{
    if (!m_modules || index < 0 || index >= m_modules->size() || !m_modules->at(index))
        return;

    auto &mod = *(*m_modules)[index];
    auto *dec = mod.decoder();

    if (command == QStringLiteral("play")) {
        mod.play();
    } else if (command == QStringLiteral("pause")) {
        mod.pause();
    } else if (command == QStringLiteral("stop")) {
        mod.stop();
    } else if (command == QStringLiteral("toggle")) {
        mod.togglePlayPause();
    } else if (command == QStringLiteral("load") && !args.isEmpty()) {
        mod.load(args.first().toString());
    } else if (command == QStringLiteral("seek") && !args.isEmpty() && dec->isOpen()) {
        const double n = qBound(0.0, args.first().toDouble(), 1.0);
        dec->seekMs(static_cast<qint64>(n * dec->durationMs()));
    } else if (command == QStringLiteral("opacity") && !args.isEmpty()) {
        mod.transform().opacity = static_cast<float>(qBound(0.0, args.first().toDouble(), 1.0));
        if (m_transformNotify)
            m_transformNotify();
    } else if (command == QStringLiteral("scale") && !args.isEmpty()) {
        mod.transform().scale = static_cast<float>(qMax(0.01, args.first().toDouble()));
        mod.transform().scaleY = mod.transform().scale;
        if (m_transformNotify)
            m_transformNotify();
    } else if (command == QStringLiteral("pos") && args.size() >= 2) {
        mod.transform().posX = static_cast<float>(args[0].toDouble());
        mod.transform().posY = static_cast<float>(args[1].toDouble());
        if (m_transformNotify)
            m_transformNotify();
    } else if (command == QStringLiteral("crop") && args.size() >= 4) {
        auto &t = mod.transform();
        t.cropLeft = static_cast<float>(qBound(0.0, args[0].toDouble(), 0.49));
        t.cropTop = static_cast<float>(qBound(0.0, args[1].toDouble(), 0.49));
        t.cropRight = static_cast<float>(qBound(0.0, args[2].toDouble(), 0.49));
        t.cropBottom = static_cast<float>(qBound(0.0, args[3].toDouble(), 0.49));
        if (m_transformNotify)
            m_transformNotify();
    } else if (command == QStringLiteral("visible") && !args.isEmpty()) {
        mod.transform().visible = args.first().toDouble() >= 0.5;
        if (m_transformNotify)
            m_transformNotify();
    }
}
