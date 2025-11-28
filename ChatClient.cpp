
#include "ChatClient.h"
#include <QDataStream>



ChatClient::ChatClient(QObject *parent)
    : QObject(parent)
{

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

