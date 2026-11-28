#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

class QTcpSocket;
class QTimer;
class QFile;

struct ChatUser {
    qint64 id = 0;
    QString name;
};

// 一个进行中的文件传输（发送或接收）
struct FileTask {
    quint32 id = 0;
    bool sending = false;
    QString peerName;
    QString fileName;
    QString savePath;
    qint64 size = 0;
    qint64 transferred = 0;
    quint32 chunks = 0;
    quint32 nextChunk = 0;   // 发送方：下一个要读的块
    quint32 nextSeq = 0;     // 接收方：期望的块序号
    QFile *file = nullptr;
};

// 客户端传输层：连接、登录、心跳、断线重连、拆帧、文件分块。
// 不碰界面，只把结果通过信号抛出去。
class ChatClient : public QObject {
    Q_OBJECT

public:
    explicit ChatClient(QObject *parent = nullptr);
    ~ChatClient() override;

    // 用户意图：连接并保持（断了会自动重连）
    void connectToServer(const QString &host, quint16 port, const QString &name);
    // 用户意图：主动断开（不再重连）
    void disconnectFromServer();

    // to = 0 表示广播，否则发给指定用户 id
    void sendText(const QString &text, qint64 to = 0);
    // 发文件给对方，返回 fileId；失败返回 0
    quint32 sendFile(const QString &path, qint64 toId);
    void cancelFile(quint32 fileId);

    bool isConnected() const;
    bool isActive() const { return m_wantConnected; }

signals:
    void connected();
    void disconnected();
    void reconnecting(int attempt, int delayMs);
    void loginResult(bool ok, qint64 selfId, const QString &text);
    void userList(const QList<ChatUser> &users);
    void userJoined(const ChatUser &user);
    void userLeft(qint64 id, const QString &name);
    void textReceived(const QString &name, const QString &text, bool isPrivate);
    void errorOccurred(const QString &message);

    void fileOffer(quint32 id, const QString &fromName,
                   const QString &fileName, qint64 size);
    void fileProgress(quint32 id, qint64 transferred, qint64 total);
    void fileFinished(quint32 id, bool ok, const QString &message);

private slots:
    void onReadyRead();
    void onPingTimeout();
    void tryReconnect();
    void onBytesWritten();

private:
    void sendLogin(const QString &name);
    void startHeartbeat();
    void stopHeartbeat();
    void scheduleReconnect();

    void handleFileOffer(const QByteArray &payload);
    void handleFileChunk(const QByteArray &payload);
    void handleFileEnd(const QByteArray &payload);
    void pumpFile(quint32 fileId);
    void finishFile(quint32 fileId, bool ok, const QString &message);
    QString uniqueDownloadPath(const QString &fileName) const;

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

    QHash<quint32, FileTask> m_files;
};
