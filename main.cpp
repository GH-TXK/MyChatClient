#include "mainwindow.h"
#include "ChatClient.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    ChatClient client;
    w.show();
    return a.exec();
}
