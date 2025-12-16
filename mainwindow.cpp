#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , client(new ChatClient(this))
{
    ui->setupUi(this);

    connect(client, &ChatClient::connected, this, [this] { appendLine("✅ 已连接服务器"); });
    connect(client, &ChatClient::messageReceived, this, [this](const QString &t) { appendLine(t); });
    connect(client, &ChatClient::errorText, this, [this](const QString &e) {
        appendLine("❌ 错误: " + e);
    });
    connect(ui->toolButton, &QToolButton::clicked, this, [this] {
        const QString text = ui->textEdit->toPlainText().trimmed();
        if (!text.isEmpty()) {
            client->sendText(text);
            appendLine("我: " + text, true);
            ui->textEdit->clear();
        }
    });
    connect(client, &ChatClient::progressUpdated, this, [this](qint64 sent, qint64 total){
        int percent = static_cast<int>((double)sent / total * 100);
        ui->progressBar->setValue(percent);
    });

    connect(client, &ChatClient::fileSent, this, [this]{
        ui->progressBar->setVisible(false);
        ui->statusbar->clearMessage(); // 清除旧消息
        ui->statusbar->showMessage(tr("文件发送完成"), 3000); // 显示3秒后自动消失
    });

    connect(client, &ChatClient::fileOffer, this,
            [this](const QString &senderName, const QString &fileName, qint64 fileSize) {
                // 在聊天窗口显示提示
                appendLine(QString("有人发送文件: %1 (%2 字节)，是否接收？")
                               .arg(fileName).arg(fileSize), false);

                // 弹窗询问是否接收
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this, tr("文件接收"),
                                              tr("有人发送文件 %1 (%2 字节)，是否接收？")
                                                  .arg(fileName).arg(fileSize),
                                              QMessageBox::Yes | QMessageBox::No);

                if (reply == QMessageBox::Yes) {
                    QString dir = QFileDialog::getExistingDirectory(this, tr("选择保存目录"));
                    if (dir.isEmpty()) return;

                    QString savePath = dir + "/" + fileName;
                    QFile outFile(savePath);
                    if (outFile.open(QIODevice::WriteOnly)) {
                        qint64 received = 0;
                        qint64 total = client->getPendingFileSize();

                        ui->progressBar->setVisible(true);
                        ui->progressBar->setValue(0);

                        // 分块写入并更新进度
                        QByteArray fileData = client->getPendingFileData();
                        outFile.write(fileData);
                        received += fileData.size();
                        int percent = static_cast<int>((double)received / total * 100);
                        ui->progressBar->setValue(percent);

                        outFile.close();
                        ui->progressBar->setVisible(false);

                        appendLine(QString("已保存文件到: %1").arg(savePath), false);
                        emit client->fileReceived(savePath);
                    }
                } else {
                    appendLine(QString("拒绝接收文件: %1").arg(fileName), false);
                }
            });

    // 更新进度条
    connect(client, &ChatClient::fileProgress, this, [this](qint64 received, qint64 total){
        ui->progressBar->setVisible(true);
        int percent = static_cast<int>((double)received / total * 100);
        ui->progressBar->setValue(percent);
    });

    // 接收完成后隐藏进度条并提示
    connect(client, &ChatClient::fileReceived, this, [this](const QString &filePath){
        ui->progressBar->setVisible(false);
        appendLine(QString("已保存文件到: %1").arg(filePath), false);
    });

    // client->connectTo("192.168.142.128", 12345);
    client->connectTo("127.0.0.1", 12345);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_toolButton_clicked()
{
    qDebug()<< "clicked";
}

void MainWindow::appendLine(const QString &line, bool isSelf)
{
    if (isSelf)
        ui->textBrowser->append("<p style='color:blue;'><b>" + line + "</b></p>");
    else
        ui->textBrowser->append("<p style='color:green;'>" + line + "</p>");
}


void MainWindow::on_pushButton_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("选择要发送的文件"),
        QString(),
        tr("所有文件 (*.*)")
        );

    if (filePath.isEmpty())
        return;

    ui->progressBar->setVisible(true);
    ui->progressBar->setValue(0);

    client->sendFile(filePath);

    // 在聊天窗口显示提示
    appendLine(QString("我发送了文件: %1").arg(QFileInfo(filePath).fileName()), true);

    // ui->statusbar->showMessage(tr("正在发送文件: %1").arg(filePath));
}
