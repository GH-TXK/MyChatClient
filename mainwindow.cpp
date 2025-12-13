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
        // if (!input->toPlainText.isEmpty()) {
        //     client->sendText(input->text());
        //     appendLine("我: " + input->text(), true);
        //     input->clear();
        // }
        const QString text = ui->textEdit->toPlainText().trimmed();
        if (!text.isEmpty()) {
            client->sendText(text);
            appendLine("我: " + text, true);
            ui->textEdit->clear();
        }
    });

    client->connectTo("192.168.142.128", 12345);
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

