#pragma once

#include "chattypes.h"

#include <QObject>
#include <QString>

class QThread;
class NetWorker;

// 界面线程里的门面：方法都是异步的，真正的 socket/文件操作在 NetWorker 线程。
class ChatClient : public QObject {
    Q_OBJECT

public:
    explicit ChatClient(QObject *parent = nullptr);
    ~ChatClient() override;

    void connectToServer(const QString &host, quint16 port, const QString &name);
    void disconnectFromServer();
    void sendText(const QString &text, qint64 to = 0);
    void sendFile(const QString &path, qint64 toId);
    void cancelFile(quint32 fileId);

    bool isConnected() const { return m_connected; }
    bool isActive() const { return m_active; }

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

    // 内部：发往 worker 线程的请求
    void requestConnect(const QString &host, quint16 port, const QString &name);
    void requestDisconnect();
    void requestSendText(const QString &text, qint64 to);
    void requestSendFile(const QString &path, qint64 toId);
    void requestCancelFile(quint32 fileId);

private:
    QThread *m_thread = nullptr;
    NetWorker *m_worker = nullptr;
    bool m_connected = false;
    bool m_active = false;
};
