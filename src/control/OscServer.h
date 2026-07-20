#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class QUdpSocket;

class OscServer : public QObject
{
    Q_OBJECT

public:
    explicit OscServer(QObject *parent = nullptr);
    ~OscServer() override;

    bool start(quint16 port);
    void stop();
    bool isRunning() const { return m_running; }
    quint16 port() const { return m_port; }

signals:
    void messageReceived(const QString &address, const QVariantList &args);
    void statusChanged(const QString &status);

private slots:
    void onReadyRead();

private:
    void dispatchPacket(const QByteArray &data);
    static bool parseMessage(const QByteArray &data, QString *address, QVariantList *args);
    static int paddedSize(int size);

    QUdpSocket *m_socket = nullptr;
    quint16 m_port = 7000;
    bool m_running = false;
};
