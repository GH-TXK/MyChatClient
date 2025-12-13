
#include "ChatClient.h"
#include <QDataStream>



ChatClient::ChatClient(QObject *parent)
    : QObject(parent)
{
    connect(&socket, &QTcpSocket::connected, this, &ChatClient::connected);
    connect(&socket, &QTcpSocket::readyRead, this, [this] {
        buffer += socket.readAll();
        while (buffer.size() >= 4) {
            quint32 len = qFromBigEndian<quint32>(
                reinterpret_cast<const uchar *>(buffer.constData()));
            if (buffer.size() - 4 < static_cast<int>(len))
                break;
            QByteArray payload = buffer.mid(4, len);
            buffer.remove(0, 4 + len);
            emit messageReceived(QString::fromUtf8(payload));
        }
    });
    connect(&socket,
            QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this,
            [this](auto) { emit errorText(socket.errorString()); });
}



void ChatClient::connectTo(const QString &host, quint16 port)
{
    socket.connectToHost(host, port);
}



void ChatClient::sendText(const QString &text)
{
    QByteArray payload = text.toUtf8();
    QByteArray frame;
    frame.resize(4 + payload.size());
    qToBigEndian<quint32>(payload.size(), reinterpret_cast<uchar *>(frame.data()));
    memcpy(frame.data() + 4, payload.constData(), payload.size());
    socket.write(frame);
}

