#pragma once

#include <QSet>
#include <QTcpServer>

class QTimer;
class ClientSession;

// 无状态中转：维护在线表，处理登录与广播，连接断开时清理。
// 不存历史、不做离线消息、不落库。
class ChatServer : public QTcpServer {
    Q_OBJECT

public:
    explicit ChatServer(QObject *parent = nullptr);

protected:
    void incomingConnection(qintptr handle) override;

private slots:
    void onFrame(ClientSession *session, int type, const QByteArray &payload);
    void onClosed(ClientSession *session);
    void checkIdleSessions();

private:
    void handleLogin(ClientSession *session, const QByteArray &payload);
    void handleChat(ClientSession *session, const QByteArray &payload);

    void broadcast(const QByteArray &frame);
    void broadcastExcept(const QByteArray &frame, ClientSession *except);
    ClientSession *findById(qint64 id) const;
    bool isNameTaken(const QString &name, ClientSession *except) const;
    int loggedInCount() const;

    QSet<ClientSession *> m_sessions;
    quint32 m_nextId = 1;
    QTimer *m_idleTimer = nullptr;
};
