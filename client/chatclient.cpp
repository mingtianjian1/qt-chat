#include "chatclient.h"

#include "protocol.h"

#include <QAbstractSocket>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>

namespace {

QByteArray toJson(const QJsonObject &object) {
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

}  // namespace

ChatClient::ChatClient(QObject *parent) : QObject(parent) {
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, [this] {
        sendLogin(m_pendingName);                      // 连上就先登录
        emit connected();
    });
    connect(m_socket, &QTcpSocket::disconnected, this, [this] {
        m_selfId = 0;
        emit disconnected();
    });
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatClient::onReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, [this] {
        emit errorOccurred(m_socket->errorString());
    });
}

void ChatClient::connectToServer(const QString &host, quint16 port, const QString &name) {
    m_buffer.clear();
    m_pendingName = name;
    m_socket->connectToHost(host, port);
}

void ChatClient::disconnectFromServer() {
    m_socket->disconnectFromHost();
}

bool ChatClient::isConnected() const {
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void ChatClient::sendLogin(const QString &name) {
    QJsonObject request;
    request["name"] = name;
    m_socket->write(proto::encode(proto::Type::LoginReq, toJson(request)));
}

void ChatClient::sendText(const QString &text) {
    QJsonObject message;
    message["text"] = text;
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
            m_socket->abort();                         // 流错位，重连由上层决定
            return;
        }

        const QJsonObject message = QJsonDocument::fromJson(payload).object();
        switch (type) {
        case proto::Type::LoginAck: {
            const bool ok = message.value("ok").toBool();
            m_selfId = ok ? qint64(message.value("selfId").toInteger()) : 0;
            emit loginResult(ok, m_selfId,
                             ok ? message.value("name").toString()
                                : message.value("reason").toString());
            break;
        }
        case proto::Type::UserList: {
            QStringList names;
            const QJsonArray users = message.value("users").toArray();
            for (const QJsonValue &user : users) {
                names << user.toObject().value("name").toString();
            }
            emit userList(names);
            break;
        }
        case proto::Type::UserJoin:
            emit userJoined(message.value("name").toString());
            break;
        case proto::Type::UserLeave:
            emit userLeft(message.value("name").toString());
            break;
        case proto::Type::ChatText:
            emit textReceived(message.value("fromName").toString(),
                              message.value("text").toString());
            break;
        case proto::Type::Error:
            emit errorOccurred(message.value("msg").toString());
            break;
        default:
            break;
        }
    }
}
