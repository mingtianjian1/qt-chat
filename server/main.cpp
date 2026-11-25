#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

// v0 骨架：先把"能监听、能接受连接、能看到字节"跑出来。
// 下一步（TODO）在这里接 ClientSession：每连接一个读缓冲，循环 proto::decode，
// 按 Type 分发；收到 ChatText 就广播给所有连接。
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QTcpServer server;
    const quint16 port = 9527;
    if (!server.listen(QHostAddress::Any, port)) {
        qCritical() << "listen failed:" << server.errorString();
        return 1;
    }
    qInfo() << "chatserver listening on" << server.serverPort();

    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server] {
        while (QTcpSocket *socket = server.nextPendingConnection()) {
            qInfo() << "client connected:"
                    << socket->peerAddress().toString() << socket->peerPort();

            // TODO(v0): 交给 ClientSession 管理；这里只做最粗的观测。
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket] {
                qInfo() << "raw bytes:" << socket->readAll().size();
            });
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });

    return app.exec();
}
