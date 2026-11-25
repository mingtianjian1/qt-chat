#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QTcpSocket;

// 客户端传输层：连接、拆帧、把 ChatText 还原成"谁说了什么"。
// 不碰界面，也不解析协议以外的业务。
class ChatClient : public QObject {
    Q_OBJECT

public:
    explicit ChatClient(QObject *parent = nullptr);

    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    void sendText(const QString &name, const QString &text);
    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void textReceived(const QString &name, const QString &text);
    void errorOccurred(const QString &message);

private slots:
    void onReadyRead();

private:
    QTcpSocket *m_socket = nullptr;
    QByteArray m_buffer;
};
