#include "chatclient.h"

#include "protocol.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>

ChatClient::ChatClient(QObject *parent) : QObject(parent) {
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, &ChatClient::connected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ChatClient::disconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatClient::onReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, [this] {
        emit errorOccurred(m_socket->errorString());
    });
}

void ChatClient::connectToServer(const QString &host, quint16 port) {
    m_buffer.clear();
    m_socket->connectToHost(host, port);
}

void ChatClient::disconnectFromServer() {
    m_socket->disconnectFromHost();
}

bool ChatClient::isConnected() const {
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void ChatClient::sendText(const QString &name, const QString &text) {
    QJsonObject message;
    message["name"] = name;
    message["text"] = text;
    message["ts"] = QDateTime::currentMSecsSinceEpoch();

    const QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
    m_socket->write(proto::encode(proto::Type::ChatText, payload));
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
            m_socket->abort();                       // 流错位，重连由上层决定
            return;
        }

        if (type == proto::Type::ChatText) {
            const QJsonObject message = QJsonDocument::fromJson(payload).object();
            emit textReceived(message.value("name").toString(),
                              message.value("text").toString());
        }
    }
}
