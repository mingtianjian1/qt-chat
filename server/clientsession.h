#pragma once

#include <QByteArray>
#include <QObject>

class QTcpSocket;

// 一个 TCP 连接一个实例：负责读缓冲、拆帧、断线通知。
// 只做传输层，不关心业务；业务分发给 ChatServer。
class ClientSession : public QObject {
    Q_OBJECT

public:
    ClientSession(QTcpSocket *socket, quint32 id, QObject *parent = nullptr);

    quint32 id() const { return m_id; }
    QTcpSocket *socket() const { return m_socket; }

    void sendRaw(const QByteArray &frame);

signals:
    void frameReceived(ClientSession *session, int type, const QByteArray &payload);
    void closed(ClientSession *session);

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    QTcpSocket *m_socket = nullptr;
    QByteArray m_buffer;
    quint32 m_id = 0;
};
