// #ifndef CHATCLIENT_H
// #define CHATCLIENT_H

// #endif // CHATCLIENT_H
#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QtEndian>
#include <QFileDialog>
#include <QMessageBox>
#include <QBuffer>

class ChatClient : public QObject
{
    Q_OBJECT
public:
    explicit ChatClient(QObject *parent = nullptr);
    void connectTo(const QString &host, quint16 port);
    void sendText(const QString &text);
    void sendFile(const QString &filePath);
public:
    QByteArray getPendingFileData() const { return pendingFileData; }
    QString getPendingFileName() const { return pendingFileName; }
    qint64 getPendingFileSize() const { return pendingFileSize; }
signals:
    void connected();
    void messageReceived(const QString &text);
    void fileReceived(const QString &filePath);
    void errorText(const QString &msg);
    void progressUpdated(qint64 sentBytes, qint64 totalBytes);
    void fileSent();
    void fileOffer(const QString &senderName, const QString &fileName, qint64 fileSize);
    void fileProgress(qint64 receivedBytes, qint64 totalBytes);

private:
    QTcpSocket socket;
    QByteArray buffer;
    QString pendingFileName;
    qint64 pendingFileSize;
    QByteArray pendingFileData;
    // 新增：接收文件状态
    QString currentFileName;
    qint64 currentFileSize = 0;
    qint64 receivedBytes = 0;
    QFile outFile;
    bool receivingFile = false;
    QString pendingFilePath;   // 待发送文件路径（发送端）
    bool pendingSendActive = false;

};
