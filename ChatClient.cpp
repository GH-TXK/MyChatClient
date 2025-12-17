#include "ChatClient.h"
#include <QDataStream>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>

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
                emit messageReceived(QString::fromUtf8(data));

            } else if (type == 0x02) {
                // 文件头
                QDataStream in(data);
                in.setVersion(QDataStream::Qt_6_10);
                in >> currentFileName >> currentFileSize;

                // CHANGED: 重置接收状态，避免历史状态残留
                receivingFile = false;
                receivedBytes = 0;

                emit messageReceived(QString("有人发送文件: %1 (%2 字节)")
                                         .arg(currentFileName).arg(currentFileSize));

                QMessageBox::StandardButton reply =
                    QMessageBox::question(nullptr, tr("文件接收"),
                                          tr("有人发送文件 %1 (%2 字节)，是否接收？")
                                              .arg(currentFileName).arg(currentFileSize),
                                          QMessageBox::Yes | QMessageBox::No);

                if (reply == QMessageBox::Yes) {
                    QString dir = QFileDialog::getExistingDirectory(nullptr, tr("选择保存目录"));
                    if (dir.isEmpty()) {
                        // CHANGED: 用户取消目录选择 -> 发送拒绝 0x05
                        QByteArray reject;
                        reject.append(char(0x05));
                        QByteArray frame;
                        frame.resize(4 + reject.size());
                        qToBigEndian<quint32>(reject.size(), reinterpret_cast<uchar *>(frame.data()));
                        memcpy(frame.data() + 4, reject.constData(), reject.size());
                        socket.write(frame);

                        receivingFile = false;
                        return;
                    }

                    QString savePath = dir + "/" + currentFileName;
                    outFile.setFileName(savePath);
                    if (outFile.open(QIODevice::WriteOnly)) {
                        receivingFile = true;
                        receivedBytes = 0;
                        emit fileProgress(receivedBytes, currentFileSize);

                        // =========================
                        // CHANGED: 成功准备好后再发送确认帧 0x04
                        // =========================
                        QByteArray confirm;
                        confirm.append(char(0x04));
                        QByteArray cframe;
                        cframe.resize(4 + confirm.size());
                        qToBigEndian<quint32>(confirm.size(), reinterpret_cast<uchar *>(cframe.data()));
                        memcpy(cframe.data() + 4, confirm.constData(), confirm.size());
                        socket.write(cframe);

                    } else {
                        // CHANGED: 打开失败 -> 发送拒绝 0x05
                        QByteArray reject;
                        reject.append(char(0x05));
                        QByteArray frame;
                        frame.resize(4 + reject.size());
                        qToBigEndian<quint32>(reject.size(), reinterpret_cast<uchar *>(frame.data()));
                        memcpy(frame.data() + 4, reject.constData(), reject.size());
                        socket.write(frame);

                        receivingFile = false;
                        emit messageReceived(QString("无法打开保存文件: %1").arg(savePath));
                    }
                } else {
                    // 用户选择否 -> 发送拒绝 0x05
                    QByteArray reject;
                    reject.append(char(0x05));
                    QByteArray frame;
                    frame.resize(4 + reject.size());
                    qToBigEndian<quint32>(reject.size(), reinterpret_cast<uchar *>(frame.data()));
                    memcpy(frame.data() + 4, reject.constData(), reject.size());
                    socket.write(frame);

                    receivingFile = false;
                }
            } else if (type == 0x03) {
                // 文件分块
                if (receivingFile && outFile.isOpen()) {
                    qint64 w = outFile.write(data);
                    if (w > 0) {
                        receivedBytes += w;
                        emit fileProgress(receivedBytes, currentFileSize);
                    } else if (w < 0) {
                        qWarning() << "Write failed:" << outFile.errorString();
                    }

                    if (receivedBytes >= currentFileSize) {
                        outFile.close();
                        receivingFile = false;
                        emit fileReceived(outFile.fileName());
                    }
                }
            } else if (type == 0x06) {
                // =========================
                // CHANGED: 收到服务端允许开始发送分块
                // =========================
                if (pendingSendActive && !pendingFilePath.isEmpty()) {
                    QFile file(pendingFilePath);
                    if (!file.open(QIODevice::ReadOnly)) {
                        emit errorText(tr("无法打开待发送文件: %1").arg(pendingFilePath));
                    } else {
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
                    pendingSendActive = false;
                    pendingFilePath.clear();
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

    QByteArray body;
    QDataStream out(&body, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_10);
    out << info.fileName() << file.size();

    payload.append(body);

    QByteArray frame;
    frame.resize(4 + payload.size());
    qToBigEndian<quint32>(payload.size(), reinterpret_cast<uchar *>(frame.data()));
    memcpy(frame.data() + 4, payload.constData(), payload.size());
    socket.write(frame);

    // 不立即发送分块，等待服务端发来 0x06
    pendingFilePath = filePath;
    pendingSendActive = true;

    file.close();
}
