#pragma once

#include <QImage>
#include <QString>
#include <QByteArray>

enum class NdiBandwidth {
    Highest = 0,
    Lowest = 1
};

class NdiSender
{
public:
    NdiSender();
    ~NdiSender();

    bool isAvailable() const;
    void setFps(int fps);
    void setSendAlpha(bool alpha);
    void setBandwidth(NdiBandwidth bandwidth);

    bool start(const QString &name, int width, int height);
    void stop();
    bool send(const QImage &frame);
    bool isRunning() const { return m_running; }

private:
    bool m_running = false;
    QString m_name;
    QByteArray m_nameUtf8; // lifetime for NDI C API
    int m_width = 0;
    int m_height = 0;
    int m_fps = 30;
    bool m_sendAlpha = true;
    NdiBandwidth m_bandwidth = NdiBandwidth::Highest;
    void *m_instance = nullptr;
};
