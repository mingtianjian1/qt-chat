#include "chatclient.h"

#include "worker.h"

#include <QThread>

ChatClient::ChatClient(QObject *parent) : QObject(parent) {
    qRegisterMetaType<ChatUser>("ChatUser");
    qRegisterMetaType<QList<ChatUser>>("QList<ChatUser>");

    m_thread = new QThread(this);
    m_worker = new NetWorker;
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &NetWorker::start);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(this, &ChatClient::requestConnect,
            m_worker, &NetWorker::connectToServer, Qt::QueuedConnection);
    connect(this, &ChatClient::requestDisconnect,
            m_worker, &NetWorker::disconnectFromServer, Qt::QueuedConnection);
    connect(this, &ChatClient::requestSendText,
            m_worker, &NetWorker::sendText, Qt::QueuedConnection);
    connect(this, &ChatClient::requestSendFile,
            m_worker, &NetWorker::sendFile, Qt::QueuedConnection);
    connect(this, &ChatClient::requestCancelFile,
            m_worker, &NetWorker::cancelFile, Qt::QueuedConnection);

    connect(m_worker, &NetWorker::connected, this, [this] {
        m_connected = true;
        emit connected();
    });
    connect(m_worker, &NetWorker::disconnected, this, [this] {
        m_connected = false;
        emit disconnected();
    });
    connect(m_worker, &NetWorker::reconnecting, this, &ChatClient::reconnecting);
    connect(m_worker, &NetWorker::loginResult, this,
            [this](bool ok, qint64 selfId, const QString &text) {
                if (!ok) {
                    m_active = false;                  // 登录被拒，不再重连
                }
                emit loginResult(ok, selfId, text);
            });
    connect(m_worker, &NetWorker::userList, this, &ChatClient::userList);
    connect(m_worker, &NetWorker::userJoined, this, &ChatClient::userJoined);
    connect(m_worker, &NetWorker::userLeft, this, &ChatClient::userLeft);
    connect(m_worker, &NetWorker::textReceived, this, &ChatClient::textReceived);
    connect(m_worker, &NetWorker::errorOccurred, this, &ChatClient::errorOccurred);
    connect(m_worker, &NetWorker::fileStarted, this, &ChatClient::fileStarted);
    connect(m_worker, &NetWorker::fileOffer, this, &ChatClient::fileOffer);
    connect(m_worker, &NetWorker::fileProgress, this, &ChatClient::fileProgress);
    connect(m_worker, &NetWorker::fileFinished, this, &ChatClient::fileFinished);

    m_thread->start();
}

ChatClient::~ChatClient() {
    m_thread->quit();
    m_thread->wait();
}

void ChatClient::connectToServer(const QString &host, quint16 port, const QString &name) {
    m_active = true;
    emit requestConnect(host, port, name);
}

void ChatClient::disconnectFromServer() {
    m_active = false;
    emit requestDisconnect();
}

void ChatClient::sendText(const QString &text, qint64 to) {
    emit requestSendText(text, to);
}

void ChatClient::sendFile(const QString &path, qint64 toId) {
    emit requestSendFile(path, toId);
}

void ChatClient::cancelFile(quint32 fileId) {
    emit requestCancelFile(fileId);
}
