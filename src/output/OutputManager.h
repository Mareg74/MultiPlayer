#pragma once

#include "output/NdiSender.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <memory>

class SpoutSender;

class OutputManager : public QObject
{
    Q_OBJECT

public:
    explicit OutputManager(QObject *parent = nullptr);
    ~OutputManager() override;

    void setNdiEnabled(bool enabled);
    void setSpoutEnabled(bool enabled);
    void setNdiName(const QString &name);
    void setSpoutName(const QString &name);
    void setResolution(int width, int height);
    void setFps(int fps);
    void setNdiSendAlpha(bool alpha);
    void setNdiBandwidth(NdiBandwidth bandwidth);

    bool ndiEnabled() const { return m_ndiEnabled; }
    bool spoutEnabled() const { return m_spoutEnabled; }
    QString ndiName() const { return m_ndiName; }
    QString spoutName() const { return m_spoutName; }
    int fps() const { return m_fps; }
    bool ndiSendAlpha() const { return m_ndiAlpha; }
    NdiBandwidth ndiBandwidth() const { return m_ndiBandwidth; }

    bool start();
    void stop();
    bool isRunning() const { return m_running; }
    bool sendFrame(const QImage &frame);

    bool ndiAvailable() const;
    bool spoutAvailable() const;

signals:
    void statusChanged();

private:
    void ensureSenders();

    bool m_running = false;
    bool m_ndiEnabled = true;
    bool m_spoutEnabled = true;
    QString m_ndiName = QStringLiteral("MultiPlayer");
    QString m_spoutName = QStringLiteral("MultiPlayer");
    int m_width = 1920;
    int m_height = 1080;
    int m_fps = 30;
    bool m_ndiAlpha = true;
    NdiBandwidth m_ndiBandwidth = NdiBandwidth::Highest;

    std::unique_ptr<NdiSender> m_ndi;
    std::unique_ptr<SpoutSender> m_spout;
};
