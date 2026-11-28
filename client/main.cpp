#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("qt-chat");
    QCoreApplication::setApplicationName("qt-chat");

    MainWindow window;
    window.show();

    return app.exec();
}
