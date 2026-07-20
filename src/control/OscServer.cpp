#include "control/OscServer.h"

#include <QHostAddress>
#include <QUdpSocket>
#include <QtEndian>
#include <cstring>

OscServer::OscServer(QObject *parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
{
    connect(m_socket, &QUdpSocket::readyRead, this, &OscServer::onReadyRead);
}

OscServer::~OscServer()
{
    stop();
}

bool OscServer::start(quint16 port)
{
    stop();
    m_port = port;
    if (!m_socket->bind(QHostAddress::AnyIPv4, port,
                        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit statusChanged(tr("OSC: échec bind port %1").arg(port));
        m_running = false;
        return false;
    }
    m_running = true;
    emit statusChanged(tr("OSC: écoute UDP %1").arg(port));
    return true;
}

void OscServer::stop()
{
    if (m_socket->state() == QAbstractSocket::BoundState)
        m_socket->close();
    m_running = false;
    emit statusChanged(tr("OSC: arrêté"));
}

void OscServer::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QByteArray data;
        data.resize(static_cast<int>(m_socket->pendingDatagramSize()));
        m_socket->readDatagram(data.data(), data.size());
        dispatchPacket(data);
    }
}

void OscServer::dispatchPacket(const QByteArray &data)
{
    if (data.startsWith("#bundle")) {
        int pos = 16; // "#bundle\0" (8) + timetag (8)
        while (pos + 4 <= data.size()) {
            const qint32 size =
                qFromBigEndian<qint32>(reinterpret_cast<const uchar *>(data.constData() + pos));
            pos += 4;
            if (size <= 0 || pos + size > data.size())
                break;
            dispatchPacket(data.mid(pos, size));
            pos += size;
        }
        return;
    }

    QString address;
    QVariantList args;
    if (parseMessage(data, &address, &args))
        emit messageReceived(address, args);
}

int OscServer::paddedSize(int size)
{
    return (size + 4) & ~0x3;
}

bool OscServer::parseMessage(const QByteArray &data, QString *address, QVariantList *args)
{
    if (!address || !args || data.size() < 4)
        return false;
    args->clear();

    int pos = 0;
    const int addrEnd = data.indexOf('\0', pos);
    if (addrEnd < 0)
        return false;
    *address = QString::fromUtf8(data.constData() + pos, addrEnd - pos);
    pos = paddedSize(addrEnd + 1);
    if (pos >= data.size())
        return true;
    if (data.at(pos) != ',')
        return true;

    const int typeEnd = data.indexOf('\0', pos);
    if (typeEnd < 0)
        return false;
    const QByteArray types = data.mid(pos + 1, typeEnd - pos - 1);
    pos = paddedSize(typeEnd + 1);

    for (char t : types) {
        if (pos > data.size())
            break;
        switch (t) {
        case 'i': {
            if (pos + 4 > data.size())
                return false;
            args->append(qFromBigEndian<qint32>(
                reinterpret_cast<const uchar *>(data.constData() + pos)));
            pos += 4;
            break;
        }
        case 'f': {
            if (pos + 4 > data.size())
                return false;
            quint32 bits = qFromBigEndian<quint32>(
                reinterpret_cast<const uchar *>(data.constData() + pos));
            float f = 0.f;
            std::memcpy(&f, &bits, sizeof(float));
            args->append(static_cast<double>(f));
            pos += 4;
            break;
        }
        case 's':
        case 'S': {
            const int end = data.indexOf('\0', pos);
            if (end < 0)
                return false;
            args->append(QString::fromUtf8(data.constData() + pos, end - pos));
            pos = paddedSize(end + 1);
            break;
        }
        case 'T':
            args->append(true);
            break;
        case 'F':
            args->append(false);
            break;
        case 'N':
            args->append(QVariant());
            break;
        default:
            break;
        }
    }
    return true;
}
