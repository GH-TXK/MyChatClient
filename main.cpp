#include "mainwindow.h"
#include "ChatClient.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    ChatClient client;
    client.connectTo("192.168.142.128",12345);
    w.show();
    return a.exec();
}
