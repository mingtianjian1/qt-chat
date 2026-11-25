#include "chatserver.h"

#include "clientsession.h"
#include "protocol.h"

#include <QDebug>
#include <QTcpSocket>

ChatServer::ChatServer(QObject *parent) : QTcpServer(parent) {}

void ChatServer::incomingConnection(qintptr handle) {
    auto *socket = new QTcpSocket(this);
    if (!socket->setSocketDescriptor(handle)) {
        delete socket;
        return;
    }

    auto *session = new ClientSession(socket, m_nextId++, this);
    m_sessions.insert(session);

    connect(session, &ClientSession::frameReceived, this, &ChatServer::onFrame);
    connect(session, &ClientSession::closed, this, &ChatServer::onClosed);

    qInfo() << "session" << session->id() << "connected from"
            << socket->peerAddress().toString() << socket->peerPort()
            << "online:" << m_sessions.size();
}

void ChatServer::onFrame(ClientSession *session, int type, const QByteArray &payload) {
    const auto frameType = static_cast<proto::Type>(type);
    qInfo() << "session" << session->id() << "frame" << proto::typeName(frameType)
            << "payload" << payload.size() << "bytes";

    if (frameType == proto::Type::ChatText) {
        broadcast(proto::encode(frameType, payload));   // v0：广播给所有连接（含发送者）
    }
}

void ChatServer::onClosed(ClientSession *session) {
    m_sessions.remove(session);
    qInfo() << "session" << session->id() << "closed, online:" << m_sessions.size();
    session->deleteLater();
}

void ChatServer::broadcast(const QByteArray &frame) {
    for (ClientSession *session : m_sessions) {
        session->sendRaw(frame);
    }
}
