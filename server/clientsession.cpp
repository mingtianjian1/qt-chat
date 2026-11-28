#include "clientsession.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QTcpSocket>

ClientSession::ClientSession(QTcpSocket *socket, quint32 id, QObject *parent)
    : QObject(parent), m_socket(socket), m_id(id) {
    m_socket->setParent(this);
    touch();
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientSession::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientSession::onDisconnected);
}

void ClientSession::touch() {
    m_lastSeenMs = QDateTime::currentMSecsSinceEpoch();
}

void ClientSession::sendRaw(const QByteArray &frame) {
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(frame);
    }
}

void ClientSession::sendFrame(proto::Type type, const QByteArray &payload) {
    sendRaw(proto::encode(type, payload));
}

void ClientSession::onReadyRead() {
    touch();                                          // 收到任何字节都算活着
    m_buffer.append(m_socket->readAll());

    for (;;) {
        proto::Type type;
        QByteArray payload;
        const proto::DecodeStatus status = proto::decode(m_buffer, &type, &payload);

        if (status == proto::DecodeStatus::NeedMore) {
            return;                                   // 半包，等下一次 readyRead
        }
        if (status != proto::DecodeStatus::Ok) {
            m_socket->disconnectFromHost();           // 流已错位或长度超限
            return;
        }
        emit frameReceived(this, int(type), payload);
    }
}

void ClientSession::onDisconnected() {
    emit closed(this);
}
