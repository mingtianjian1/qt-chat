#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

class QTcpSocket;

// 客户端传输层：连接、登录、拆帧，把帧还原成界面能直接用的信号。
// 不碰界面，也不解析协议以外的业务。
class ChatClient : public QObject {
    Q_OBJECT

public:
    explicit ChatClient(QObject *parent = nullptr);

    void connectToServer(const QString &host, quint16 port, const QString &name);
    void disconnectFromServer();
    void sendText(const QString &text);
    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void loginResult(bool ok, qint64 selfId, const QString &name);
    void userList(const QStringList &names);
    void userJoined(const QString &name);
    void userLeft(const QString &name);
    void textReceived(const QString &name, const QString &text);
    void errorOccurred(const QString &message);

private slots:
    void onReadyRead();

private:
    void sendLogin(const QString &name);

    QTcpSocket *m_socket = nullptr;
    QByteArray m_buffer;
    QString m_pendingName;
    qint64 m_selfId = 0;
};
