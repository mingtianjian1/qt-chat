#include "clientsession.h"

#include "protocol.h"

#include <QAbstractSocket>
#include <QTcpSocket>

ClientSession::ClientSession(QTcpSocket *socket, quint32 id, QObject *parent)
    : QObject(parent), m_socket(socket), m_id(id) {
    m_socket->setParent(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientSession::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientSession::onDisconnected);
}

void ClientSession::sendRaw(const QByteArray &frame) {
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(frame);
    }
}

void ClientSession::onReadyRead() {
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
