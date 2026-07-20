#include "output/OutputManager.h"
#include "output/NdiSender.h"
#include "output/SpoutSender.h"

#include <QDebug>

OutputManager::OutputManager(QObject *parent)
    : QObject(parent)
    , m_ndi(std::make_unique<NdiSender>())
    , m_spout(std::make_unique<SpoutSender>())
{
}

OutputManager::~OutputManager()
{
    stop();
}

void OutputManager::setNdiEnabled(bool enabled)
{
    m_ndiEnabled = enabled;
    if (m_running)
        ensureSenders();
    emit statusChanged();
}

void OutputManager::setSpoutEnabled(bool enabled)
{
    m_spoutEnabled = enabled;
    if (m_running)
        ensureSenders();
    emit statusChanged();
}

void OutputManager::setNdiName(const QString &name)
{
    m_ndiName = name.isEmpty() ? QStringLiteral("MultiPlayer") : name;
    if (m_running && m_ndiEnabled) {
        m_ndi->stop();
        m_ndi->setFps(m_fps);
        m_ndi->setSendAlpha(m_ndiAlpha);
        m_ndi->setBandwidth(m_ndiBandwidth);
        m_ndi->start(m_ndiName, m_width, m_height);
    }
}

void OutputManager::setResolution(int width, int height)
{
    const bool sizeChanged = (m_width != width || m_height != height);
    m_width = width;
    m_height = height;
    if (m_running && sizeChanged) {
        // Size change requires recreating Spout/NDI senders.
        if (m_spout)
            m_spout->stop();
        if (m_ndi)
            m_ndi->stop();
        ensureSenders();
    }
}

void OutputManager::setSpoutName(const QString &name)
{
    m_spoutName = name.isEmpty() ? QStringLiteral("MultiPlayer") : name;
    if (m_running && m_spoutEnabled) {
        m_spout->stop();
        if (!m_spout->start(m_spoutName, m_width, m_height))
            qWarning("OutputManager: Spout rename/start failed");
    }
}

void OutputManager::setFps(int fps)
{
    m_fps = qBound(1, fps, 120);
    if (m_ndi)
        m_ndi->setFps(m_fps);
}

void OutputManager::setNdiSendAlpha(bool alpha)
{
    m_ndiAlpha = alpha;
    if (m_ndi)
        m_ndi->setSendAlpha(alpha);
}

void OutputManager::setNdiBandwidth(NdiBandwidth bandwidth)
{
    m_ndiBandwidth = bandwidth;
    if (m_ndi)
        m_ndi->setBandwidth(bandwidth);
}

bool OutputManager::start()
{
    m_running = true;
    ensureSenders();
    emit statusChanged();
    return true;
}

void OutputManager::stop()
{
    m_running = false;
    if (m_ndi)
        m_ndi->stop();
    if (m_spout)
        m_spout->stop();
    emit statusChanged();
}

bool OutputManager::sendFrame(const QImage &frame)
{
    if (!m_running || frame.isNull())
        return false;

    bool any = false;
    if (m_ndiEnabled && m_ndi && m_ndi->isRunning())
        any = m_ndi->send(frame) || any;
    if (m_spoutEnabled && m_spout && m_spout->isRunning())
        any = m_spout->send(frame) || any;

    if (!ndiAvailable() && m_ndiEnabled)
        any = true;
    if (!spoutAvailable() && m_spoutEnabled)
        any = true;

    return any;
}

bool OutputManager::ndiAvailable() const
{
    return m_ndi && m_ndi->isAvailable();
}

bool OutputManager::spoutAvailable() const
{
    return m_spout && m_spout->isAvailable();
}

void OutputManager::ensureSenders()
{
    if (m_ndiEnabled) {
        m_ndi->setFps(m_fps);
        m_ndi->setSendAlpha(m_ndiAlpha);
        m_ndi->setBandwidth(m_ndiBandwidth);
        if (!m_ndi->isRunning()) {
            if (!m_ndi->start(m_ndiName, m_width, m_height))
                qWarning("OutputManager: NDI start failed");
        } else {
            m_ndi->stop();
            if (!m_ndi->start(m_ndiName, m_width, m_height))
                qWarning("OutputManager: NDI restart failed");
        }
    } else {
        m_ndi->stop();
    }

    if (m_spoutEnabled) {
        // Avoid tearing down Spout every ensureSenders() call — receivers drop the source.
        if (!m_spout->isRunning()) {
            if (!m_spout->start(m_spoutName, m_width, m_height))
                qWarning("OutputManager: Spout start failed (name=%s %dx%d)",
                         qPrintable(m_spoutName), m_width, m_height);
        }
    } else {
        m_spout->stop();
    }
}
