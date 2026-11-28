#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

class QTcpSocket;
class QTimer;

// 客户端传输层：连接、登录、心跳、断线重连、拆帧。
// 不碰界面，只把结果通过信号抛出去。
class ChatClient : public QObject {
    Q_OBJECT

public:
    explicit ChatClient(QObject *parent = nullptr);

    // 用户意图：连接并保持（断了会自动重连）
    void connectToServer(const QString &host, quint16 port, const QString &name);
    // 用户意图：主动断开（不再重连）
    void disconnectFromServer();

    void sendText(const QString &text);
    bool isConnected() const;
    bool isActive() const { return m_wantConnected; }

signals:
    void connected();
    void disconnected();
    void reconnecting(int attempt, int delayMs);
    void loginResult(bool ok, qint64 selfId, const QString &text);
    void userList(const QStringList &names);
    void userJoined(const QString &name);
    void userLeft(const QString &name);
    void textReceived(const QString &name, const QString &text);
    void errorOccurred(const QString &message);

private slots:
    void onReadyRead();
    void onPingTimeout();
    void tryReconnect();

private:
    void sendLogin(const QString &name);
    void startHeartbeat();
    void stopHeartbeat();
    void scheduleReconnect();

    QTcpSocket *m_socket = nullptr;
    QTimer *m_pingTimer = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    QByteArray m_buffer;
    QString m_host;
    QString m_pendingName;
    quint16 m_port = 0;
    qint64 m_selfId = 0;
    qint64 m_lastPongMs = 0;
    int m_attempt = 0;
    bool m_wantConnected = false;
    bool m_loginRejected = false;
};
