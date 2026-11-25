#pragma once

#include <QSet>
#include <QTcpServer>

class ClientSession;

// 无状态中转：维护在线连接，收到 ChatText 原样广播。
// 不做登录、不存历史、不落库。
class ChatServer : public QTcpServer {
    Q_OBJECT

public:
    explicit ChatServer(QObject *parent = nullptr);

protected:
    void incomingConnection(qintptr handle) override;

private slots:
    void onFrame(ClientSession *session, int type, const QByteArray &payload);
    void onClosed(ClientSession *session);

private:
    void broadcast(const QByteArray &frame);

    QSet<ClientSession *> m_sessions;
    quint32 m_nextId = 1;
};
