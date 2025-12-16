#include "ChatClient.h"
#include <QDataStream>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>

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

            char type = payload.at(0);
            QByteArray data = payload.mid(1);

            if (type == 0x01) {
                // 文本消息
                emit messageReceived(QString::fromUtf8(data));
            } else if (type == 0x02) {
                // 文件头
                QDataStream in(data);
                in.setVersion(QDataStream::Qt_6_10);
                in >> currentFileName >> currentFileSize;

                emit messageReceived(QString("有人发送文件: %1 (%2 字节)")
                                         .arg(currentFileName).arg(currentFileSize));

                QMessageBox::StandardButton reply =
                    QMessageBox::question(nullptr, tr("文件接收"),
                                          tr("有人发送文件 %1 (%2 字节)，是否接收？")
                                              .arg(currentFileName).arg(currentFileSize),
                                          QMessageBox::Yes | QMessageBox::No);

                if (reply == QMessageBox::Yes) {
                    QString dir = QFileDialog::getExistingDirectory(nullptr, tr("选择保存目录"));
                    if (dir.isEmpty()) return;

                    QString savePath = dir + "/" + currentFileName;
                    outFile.setFileName(savePath);
                    if (outFile.open(QIODevice::WriteOnly)) {
                        receivingFile = true;
                        receivedBytes = 0;
                        emit fileProgress(receivedBytes, currentFileSize);
                    }
                } else {
                    receivingFile = false;
                }
            } else if (type == 0x03) {
                // 文件分块
                if (receivingFile && outFile.isOpen()) {
                    outFile.write(data);
                    receivedBytes += data.size();
                    emit fileProgress(receivedBytes, currentFileSize);

                    if (receivedBytes >= currentFileSize) {
                        outFile.close();
                        receivingFile = false;
                        emit fileReceived(outFile.fileName());
                    }
                }
            }
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
    QByteArray payload;
    payload.append(char(0x01));
    payload.append(text.toUtf8());

    QByteArray frame;
    frame.resize(4 + payload.size());
    qToBigEndian<quint32>(payload.size(), reinterpret_cast<uchar *>(frame.data()));
    memcpy(frame.data() + 4, payload.constData(), payload.size());
    socket.write(frame);
}

void ChatClient::sendFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return;

    QFileInfo info(file);

    // 文件头
    QByteArray payload;
    payload.append(char(0x02));
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_10);
    out << info.fileName() << file.size();

    QByteArray frame;
    frame.resize(4 + payload.size());
    qToBigEndian<quint32>(payload.size(), reinterpret_cast<uchar *>(frame.data()));
    memcpy(frame.data() + 4, payload.constData(), payload.size());
    socket.write(frame);

    // 文件分块
    qint64 totalBytes = file.size();
    qint64 sentBytes = 0;

    while (!file.atEnd()) {
        QByteArray chunk = file.read(4096);
        QByteArray block;
        block.append(char(0x03));
        block.append(chunk);

        QByteArray frame;
        frame.resize(4 + block.size());
        qToBigEndian<quint32>(block.size(), reinterpret_cast<uchar *>(frame.data()));
        memcpy(frame.data() + 4, block.constData(), block.size());
        socket.write(frame);

        sentBytes += chunk.size();
        emit progressUpdated(sentBytes, totalBytes);
    }

    file.close();
    emit fileSent();
}
