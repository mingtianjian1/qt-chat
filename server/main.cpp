#include "chatserver.h"

#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    ChatServer server;
    const quint16 port = 9527;
    if (!server.listen(QHostAddress::Any, port)) {
        qCritical() << "listen failed:" << server.errorString();
        return 1;
    }
    qInfo() << "chatserver listening on" << server.serverPort();

    return app.exec();
}
