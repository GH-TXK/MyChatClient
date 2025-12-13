// #ifndef CHATCLIENT_H
// #define CHATCLIENT_H

// #endif // CHATCLIENT_H
#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QtEndian>

class ChatClient : public QObject
{
    Q_OBJECT
public:
    explicit ChatClient(QObject *parent = nullptr);
    void connectTo(const QString &host, quint16 port);
    void sendText(const QString &text);
signals:
    void connected();
    void messageReceived(const QString &text);
    void errorText(const QString &msg);

private:
    QTcpSocket socket;
    QByteArray buffer;


};
