#include "chatserver.h"

#include "clientsession.h"
#include "protocol.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QTcpSocket>
#include <QTimer>
#include <QtEndian>

namespace {

QByteArray toJson(const QJsonObject &object) {
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

constexpr int kIdleTimeoutMs = 30'000;      // 30 秒没收到任何字节就认为断了
constexpr int kIdleCheckIntervalMs = 5'000;

}  // namespace

ChatServer::ChatServer(QObject *parent) : QTcpServer(parent) {
    m_idleTimer = new QTimer(this);
    m_idleTimer->setInterval(kIdleCheckIntervalMs);
    connect(m_idleTimer, &QTimer::timeout, this, &ChatServer::checkIdleSessions);
    m_idleTimer->start();
}

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
            << "online:" << loggedInCount();
}

void ChatServer::onFrame(ClientSession *session, int type, const QByteArray &payload) {
    const auto frameType = static_cast<proto::Type>(type);
    switch (frameType) {
    case proto::Type::LoginReq:
        handleLogin(session, payload);
        break;
    case proto::Type::ChatText:
        handleChat(session, payload);
        break;
    case proto::Type::FileOffer:
    case proto::Type::FileAccept:
    case proto::Type::FileReject:
    case proto::Type::FileChunk:
    case proto::Type::FileEnd:
        handleFile(session, frameType, payload);
        break;
    case proto::Type::Ping:
        session->sendFrame(proto::Type::Pong);
        break;
    default:
        qInfo() << "session" << session->id() << "ignored frame" << proto::typeName(frameType);
        break;
    }
}

void ChatServer::handleLogin(ClientSession *session, const QByteArray &payload) {
    if (session->isLoggedIn()) {
        return;                                        // 重复登录直接忽略
    }

    QString name = QJsonDocument::fromJson(payload).object().value("name").toString().trimmed();
    if (name.isEmpty()) {
        name = QStringLiteral("user%1").arg(session->id());
    }

    if (isNameTaken(name, session)) {
        QJsonObject rejected;
        rejected["ok"] = false;
        rejected["code"] = "name_taken";
        rejected["reason"] = "昵称已被占用";
        session->sendFrame(proto::Type::LoginAck, toJson(rejected));
        qInfo() << "session" << session->id() << "login rejected, name taken:" << name;
        session->socket()->disconnectFromHost();       // 客户端换一个名字重连
        return;
    }

    session->setName(name);
    session->setLoggedIn(true);

    QJsonObject ack;
    ack["ok"] = true;
    ack["selfId"] = qint64(session->id());
    ack["name"] = name;
    session->sendFrame(proto::Type::LoginAck, toJson(ack));

    QJsonArray users;
    for (ClientSession *other : m_sessions) {
        if (other->isLoggedIn()) {
            QJsonObject user;
            user["id"] = qint64(other->id());
            user["name"] = other->name();
            users.append(user);
        }
    }
    QJsonObject list;
    list["users"] = users;
    session->sendFrame(proto::Type::UserList, toJson(list));

    QJsonObject join;
    join["id"] = qint64(session->id());
    join["name"] = name;
    broadcastExcept(proto::encode(proto::Type::UserJoin, toJson(join)), session);

    qInfo() << "session" << session->id() << "login as" << name
            << "online:" << loggedInCount();
}

void ChatServer::handleChat(ClientSession *session, const QByteArray &payload) {
    if (!session->isLoggedIn()) {
        QJsonObject error;
        error["code"] = "not_logged_in";
        error["msg"] = "send LoginReq first";
        session->sendFrame(proto::Type::Error, toJson(error));
        return;
    }

    const QJsonObject incoming = QJsonDocument::fromJson(payload).object();
    const QString text = incoming.value("text").toString();
    if (text.isEmpty()) {
        return;
    }
    const qint64 to = qint64(incoming.value("to").toInteger());   // 0 = 广播

    QJsonObject message;
    message["from"] = qint64(session->id());
    message["fromName"] = session->name();
    message["to"] = to;
    message["ts"] = QDateTime::currentMSecsSinceEpoch();
    message["text"] = text;

    if (to == 0) {
        qInfo() << "session" << session->id() << "chat" << text.size() << "chars (broadcast)";
        broadcast(proto::encode(proto::Type::ChatText, toJson(message)));
        return;
    }

    // 私聊：只投递给目标，不回显给发送方（发送方已经本地显示）
    ClientSession *target = findById(to);
    if (!target || !target->isLoggedIn()) {
        QJsonObject error;
        error["code"] = "user_offline";
        error["msg"] = "对方不在线";
        session->sendFrame(proto::Type::Error, toJson(error));
        return;
    }

    message["toName"] = target->name();
    target->sendRaw(proto::encode(proto::Type::ChatText, toJson(message)));
    qInfo() << "session" << session->id() << "chat" << text.size()
            << "chars (private to" << target->name() << ")";
}

void ChatServer::handleFile(ClientSession *session, proto::Type type, const QByteArray &payload) {
    if (!session->isLoggedIn()) {
        return;
    }

    // 第一条是 FileOffer：建立 fileId -> (from, to) 的路由，然后转发给目标
    if (type == proto::Type::FileOffer) {
        const QJsonObject offer = QJsonDocument::fromJson(payload).object();
        const quint32 fileId = quint32(offer.value("fileId").toInteger());
        const qint64 to = qint64(offer.value("to").toInteger());
        ClientSession *target = findById(to);

        if (fileId == 0 || !target || !target->isLoggedIn()) {
            QJsonObject error;
            error["code"] = "user_offline";
            error["msg"] = "对方不在线";
            session->sendFrame(proto::Type::Error, toJson(error));
            return;
        }

        m_fileRoutes.insert(fileId, {session->id(), target->id()});

        QJsonObject forward = offer;
        forward["from"] = qint64(session->id());
        forward["fromName"] = session->name();
        forward["toName"] = target->name();
        target->sendRaw(proto::encode(proto::Type::FileOffer, toJson(forward)));

        qInfo() << "file" << fileId << "offer" << session->name() << "->" << target->name();
        return;
    }

    // 其余帧按 fileId 找路由，转发给另一端；双向都走这里（Accept/Reject/End 或数据块）
    quint32 fileId = 0;
    if (type == proto::Type::FileChunk) {
        if (payload.size() < 8) {
            return;
        }
        fileId = qFromBigEndian<quint32>(payload.constData());
    } else {
        fileId = quint32(QJsonDocument::fromJson(payload).object().value("fileId").toInteger());
    }

    const auto it = m_fileRoutes.constFind(fileId);
    if (it == m_fileRoutes.constEnd()) {
        return;
    }

    const quint32 peerId = (session->id() == it.value().first) ? it.value().second
                                                               : it.value().first;
    if (ClientSession *peer = findById(peerId)) {
        peer->sendRaw(proto::encode(type, payload));
    }

    if (type == proto::Type::FileEnd) {
        m_fileRoutes.remove(fileId);
    }
}

ClientSession *ChatServer::findById(qint64 id) const {
    for (ClientSession *session : m_sessions) {
        if (qint64(session->id()) == id) {
            return session;
        }
    }
    return nullptr;
}

void ChatServer::onClosed(ClientSession *session) {
    if (session->isLoggedIn()) {
        QJsonObject leave;
        leave["id"] = qint64(session->id());
        leave["name"] = session->name();
        broadcastExcept(proto::encode(proto::Type::UserLeave, toJson(leave)), session);
    }

    // 这个连接上还没传完的文件：通知对端中止，并清掉路由
    for (auto it = m_fileRoutes.begin(); it != m_fileRoutes.end();) {
        if (it.value().first == session->id() || it.value().second == session->id()) {
            const quint32 peerId = (it.value().first == session->id()) ? it.value().second
                                                                       : it.value().first;
            if (ClientSession *peer = findById(peerId)) {
                QJsonObject end;
                end["fileId"] = qint64(it.key());
                end["ok"] = false;
                peer->sendFrame(proto::Type::FileEnd, toJson(end));
            }
            it = m_fileRoutes.erase(it);
        } else {
            ++it;
        }
    }

    m_sessions.remove(session);
    qInfo() << "session" << session->id() << "closed, online:" << loggedInCount();
    session->deleteLater();
}

void ChatServer::broadcast(const QByteArray &frame) {
    for (ClientSession *session : m_sessions) {
        session->sendRaw(frame);
    }
}

void ChatServer::broadcastExcept(const QByteArray &frame, ClientSession *except) {
    for (ClientSession *session : m_sessions) {
        if (session != except) {
            session->sendRaw(frame);
        }
    }
}

void ChatServer::checkIdleSessions() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    QList<ClientSession *> dead;
    for (ClientSession *session : m_sessions) {
        if (now - session->lastSeenMs() > kIdleTimeoutMs) {
            dead.append(session);
        }
    }

    // abort 会同步触发 closed -> onClosed，所以先收集再处理，避免遍历中修改 m_sessions
    for (ClientSession *session : dead) {
        qInfo() << "session" << session->id() << "idle timeout, dropping";
        session->socket()->abort();
    }
}

bool ChatServer::isNameTaken(const QString &name, ClientSession *except) const {
    for (ClientSession *session : m_sessions) {
        if (session != except && session->isLoggedIn() && session->name() == name) {
            return true;
        }
    }
    return false;
}

int ChatServer::loggedInCount() const {
    int count = 0;
    for (ClientSession *session : m_sessions) {
        if (session->isLoggedIn()) {
            ++count;
        }
    }
    return count;
}
