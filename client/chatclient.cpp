#include "chatclient.h"

#include "protocol.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTimer>

#include <algorithm>

namespace {

QByteArray toJson(const QJsonObject &object) {
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

constexpr int kPingIntervalMs = 10'000;     // 每 10 秒发一次 Ping
constexpr int kPongTimeoutMs = 25'000;      // 25 秒没收到 Pong 就当断线
constexpr int kReconnectBaseMs = 1'000;
constexpr int kReconnectMaxMs = 15'000;

}  // namespace

ChatClient::ChatClient(QObject *parent) : QObject(parent) {
    m_socket = new QTcpSocket(this);
    m_pingTimer = new QTimer(this);
    m_pingTimer->setInterval(kPingIntervalMs);
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);

    connect(m_socket, &QTcpSocket::connected, this, [this] {
        m_attempt = 0;
        m_lastPongMs = QDateTime::currentMSecsSinceEpoch();
        startHeartbeat();
        sendLogin(m_pendingName);                      // 每次（重）连上都要重新登录
        emit connected();
    });
    connect(m_socket, &QTcpSocket::disconnected, this, [this] {
        m_selfId = 0;
        stopHeartbeat();
        emit disconnected();
        if (m_wantConnected && !m_loginRejected) {
            scheduleReconnect();
        }
    });
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatClient::onReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, [this] {
        emit errorOccurred(m_socket->errorString());
        // 连不上时不会再收到 disconnected，这里补一次重连调度
        if (m_wantConnected && !m_loginRejected
            && m_socket->state() == QAbstractSocket::UnconnectedState) {
            scheduleReconnect();
        }
    });
    connect(m_pingTimer, &QTimer::timeout, this, &ChatClient::onPingTimeout);
    connect(m_reconnectTimer, &QTimer::timeout, this, &ChatClient::tryReconnect);
}

void ChatClient::connectToServer(const QString &host, quint16 port, const QString &name) {
    m_host = host;
    m_port = port;
    m_pendingName = name;
    m_wantConnected = true;
    m_loginRejected = false;
    m_attempt = 0;
    m_buffer.clear();
    m_reconnectTimer->stop();
    m_socket->connectToHost(host, port);
}

void ChatClient::disconnectFromServer() {
    m_wantConnected = false;
    m_reconnectTimer->stop();
    stopHeartbeat();
    m_socket->disconnectFromHost();
}

bool ChatClient::isConnected() const {
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void ChatClient::startHeartbeat() {
    m_pingTimer->start();
}

void ChatClient::stopHeartbeat() {
    m_pingTimer->stop();
}

void ChatClient::onPingTimeout() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastPongMs > kPongTimeoutMs) {
        m_socket->abort();                             // 对端没反应，按断线处理
        return;
    }
    m_socket->write(proto::encode(proto::Type::Ping));
}

void ChatClient::scheduleReconnect() {
    if (m_reconnectTimer->isActive()) {
        return;
    }
    const int delay = std::min(kReconnectBaseMs * (1 << std::min(m_attempt, 4)),
                               kReconnectMaxMs);
    ++m_attempt;
    emit reconnecting(m_attempt, delay);
    m_reconnectTimer->start(delay);
}

void ChatClient::tryReconnect() {
    if (!m_wantConnected) {
        return;
    }
    m_buffer.clear();
    m_socket->connectToHost(m_host, m_port);
}

void ChatClient::sendLogin(const QString &name) {
    QJsonObject request;
    request["name"] = name;
    m_socket->write(proto::encode(proto::Type::LoginReq, toJson(request)));
}

void ChatClient::sendText(const QString &text, qint64 to) {
    QJsonObject message;
    message["text"] = text;
    message["to"] = to;                                // 0 = 广播
    m_socket->write(proto::encode(proto::Type::ChatText, toJson(message)));
}

void ChatClient::onReadyRead() {
    m_buffer.append(m_socket->readAll());

    for (;;) {
        proto::Type type;
        QByteArray payload;
        const proto::DecodeStatus status = proto::decode(m_buffer, &type, &payload);

        if (status == proto::DecodeStatus::NeedMore) {
            return;
        }
        if (status != proto::DecodeStatus::Ok) {
            m_socket->abort();                         // 流错位，按断线处理
            return;
        }

        if (type == proto::Type::Pong) {
            m_lastPongMs = QDateTime::currentMSecsSinceEpoch();
            continue;
        }

        const QJsonObject message = QJsonDocument::fromJson(payload).object();
        switch (type) {
        case proto::Type::LoginAck: {
            const bool ok = message.value("ok").toBool();
            if (ok) {
                m_selfId = qint64(message.value("selfId").toInteger());
            } else {
                m_selfId = 0;
                m_loginRejected = true;
                m_wantConnected = false;               // 名字被占，重连也没用
            }
            emit loginResult(ok, m_selfId,
                             ok ? message.value("name").toString()
                                : message.value("reason").toString());
            break;
        }
        case proto::Type::UserList: {
            QList<ChatUser> users;
            const QJsonArray array = message.value("users").toArray();
            for (const QJsonValue &value : array) {
                const QJsonObject object = value.toObject();
                users.append({qint64(object.value("id").toInteger()),
                              object.value("name").toString()});
            }
            emit userList(users);
            break;
        }
        case proto::Type::UserJoin:
            emit userJoined({qint64(message.value("id").toInteger()),
                             message.value("name").toString()});
            break;
        case proto::Type::UserLeave:
            emit userLeft(qint64(message.value("id").toInteger()),
                          message.value("name").toString());
            break;
        case proto::Type::ChatText:
            emit textReceived(message.value("fromName").toString(),
                              message.value("text").toString(),
                              message.value("to").toInteger() != 0);
            break;
        case proto::Type::Error:
            emit errorOccurred(message.value("msg").toString());
            break;
        default:
            break;
        }
    }
}
