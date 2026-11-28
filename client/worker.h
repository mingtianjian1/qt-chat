#pragma once

#include "chattypes.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

class QTcpSocket;
class QTimer;

// 跑在独立线程里：socket、心跳、重连、文件读写都归它。
// 对外只通过信号报告结果，界面线程不碰 socket 和文件。
class NetWorker : public QObject {
    Q_OBJECT

public:
    explicit NetWorker(QObject *parent = nullptr);
    ~NetWorker() override;

public slots:
    // 在 worker 线程里创建 socket 和定时器
    void start();

    void connectToServer(const QString &host, quint16 port, const QString &name);
    void disconnectFromServer();
    void sendText(const QString &text, qint64 to);
    void sendFile(const QString &path, qint64 toId);
    void cancelFile(quint32 fileId);

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

    void fileStarted(quint32 id, const QString &fileName, qint64 size);
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
    void finishFile(quint32 fileId, bool ok, QString message);
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
