#pragma once

#include <QImage>
#include <QString>
#include <QByteArray>

class SpoutSender
{
public:
    SpoutSender();
    ~SpoutSender();

    bool isAvailable() const;
    bool start(const QString &name, int width, int height);
    void stop();
    bool send(const QImage &frame);
    bool isRunning() const { return m_running; }

private:
    bool m_running = false;
    QString m_name;
    QByteArray m_nameUtf8;
    int m_width = 0;
    int m_height = 0;
    void *m_spout = nullptr; // SPOUTLIBRARY*
};
