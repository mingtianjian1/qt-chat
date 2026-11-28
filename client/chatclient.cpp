#include "chatclient.h"

#include "protocol.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QTimer>
#include <QtEndian>

#include <algorithm>

namespace {

QByteArray toJson(const QJsonObject &object) {
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

constexpr int kPingIntervalMs = 10'000;     // 每 10 秒发一次 Ping
constexpr int kPongTimeoutMs = 25'000;      // 25 秒没收到 Pong 就当断线
constexpr int kReconnectBaseMs = 1'000;
constexpr int kReconnectMaxMs = 15'000;
constexpr int kMaxPendingBytes = 2 * 1024 * 1024;   // 写缓冲超过 2MB 就等 bytesWritten

}  // namespace

ChatClient::ChatClient(QObject *parent) : QObject(parent) {
    m_socket = new QTcpSocket(this);
    m_pingTimer = new QTimer(this);
    m_pingTimer->setInterval(kPingIntervalMs);
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);

    connect(m_socket, &QTcpSocket::connected, this, [this] {
        m_attempt = 0;
        m_lastPongMs = QDateTime::currentMSecsSinceEpoch();
        startHeartbeat();
        sendLogin(m_pendingName);                      // 每次（重）连上都要重新登录
        emit connected();
    });
    connect(m_socket, &QTcpSocket::disconnected, this, [this] {
        m_selfId = 0;
        stopHeartbeat();
        for (quint32 id : m_files.keys()) {
            finishFile(id, false, "连接断开");
        }
        emit disconnected();
        if (m_wantConnected && !m_loginRejected) {
            scheduleReconnect();
        }
    });
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatClient::onReadyRead);
    connect(m_socket, &QTcpSocket::bytesWritten, this, &ChatClient::onBytesWritten);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, [this] {
        emit errorOccurred(m_socket->errorString());
        // 连不上时不会再收到 disconnected，这里补一次重连调度
        if (m_wantConnected && !m_loginRejected
            && m_socket->state() == QAbstractSocket::UnconnectedState) {
            scheduleReconnect();
        }
    });
    connect(m_pingTimer, &QTimer::timeout, this, &ChatClient::onPingTimeout);
    connect(m_reconnectTimer, &QTimer::timeout, this, &ChatClient::tryReconnect);
}

ChatClient::~ChatClient() {
    for (FileTask &task : m_files) {
        if (task.file) {
            task.file->close();
            delete task.file;
        }
    }
    m_files.clear();
}

void ChatClient::connectToServer(const QString &host, quint16 port, const QString &name) {
    m_host = host;
    m_port = port;
    m_pendingName = name;
    m_wantConnected = true;
    m_loginRejected = false;
    m_attempt = 0;
    m_buffer.clear();
    m_reconnectTimer->stop();
    m_socket->connectToHost(host, port);
}

void ChatClient::disconnectFromServer() {
    m_wantConnected = false;
    m_reconnectTimer->stop();
    stopHeartbeat();
    m_socket->disconnectFromHost();
}

bool ChatClient::isConnected() const {
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void ChatClient::startHeartbeat() {
    m_pingTimer->start();
}

void ChatClient::stopHeartbeat() {
    m_pingTimer->stop();
}

void ChatClient::onPingTimeout() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastPongMs > kPongTimeoutMs) {
        m_socket->abort();                             // 对端没反应，按断线处理
        return;
    }
    m_socket->write(proto::encode(proto::Type::Ping));
}

void ChatClient::scheduleReconnect() {
    if (m_reconnectTimer->isActive()) {
        return;
    }
    const int delay = std::min(kReconnectBaseMs * (1 << std::min(m_attempt, 4)),
                               kReconnectMaxMs);
    ++m_attempt;
    emit reconnecting(m_attempt, delay);
    m_reconnectTimer->start(delay);
}

void ChatClient::tryReconnect() {
    if (!m_wantConnected) {
        return;
    }
    m_buffer.clear();
    m_socket->connectToHost(m_host, m_port);
}

void ChatClient::sendLogin(const QString &name) {
    QJsonObject request;
    request["name"] = name;
    m_socket->write(proto::encode(proto::Type::LoginReq, toJson(request)));
}

void ChatClient::sendText(const QString &text, qint64 to) {
    QJsonObject message;
    message["text"] = text;
    message["to"] = to;                                // 0 = 广播
    m_socket->write(proto::encode(proto::Type::ChatText, toJson(message)));
}

quint32 ChatClient::sendFile(const QString &path, qint64 toId) {
    if (!isConnected()) {
        emit errorOccurred("还没连接");
        return 0;
    }

    auto *file = new QFile(path);
    if (!file->open(QIODevice::ReadOnly)) {
        emit errorOccurred("打不开文件：" + path);
        delete file;
        return 0;
    }

    FileTask task;
    task.id = QRandomGenerator::global()->generate();
    if (task.id == 0) {
        task.id = 1;
    }
    task.sending = true;
    task.fileName = QFileInfo(path).fileName();
    task.size = file->size();
    task.chunks = quint32((task.size + proto::kChunkSize - 1) / proto::kChunkSize);
    task.file = file;
    m_files.insert(task.id, task);

    QJsonObject offer;
    offer["fileId"] = qint64(task.id);
    offer["to"] = toId;
    offer["name"] = task.fileName;
    offer["size"] = task.size;
    offer["chunks"] = qint64(task.chunks);
    m_socket->write(proto::encode(proto::Type::FileOffer, toJson(offer)));
    return task.id;
}

void ChatClient::cancelFile(quint32 fileId) {
    auto it = m_files.find(fileId);
    if (it == m_files.end()) {
        return;
    }

    QJsonObject end;
    end["fileId"] = qint64(fileId);
    end["ok"] = false;
    m_socket->write(proto::encode(proto::Type::FileEnd, toJson(end)));

    if (!it->sending && !it->savePath.isEmpty()) {
        it->file->close();
        QFile::remove(it->savePath);
    }
    finishFile(fileId, false, "已取消");
}

void ChatClient::onBytesWritten() {
    for (quint32 id : m_files.keys()) {
        auto it = m_files.find(id);
        if (it != m_files.end() && it->sending) {
            pumpFile(id);
        }
    }
}

void ChatClient::pumpFile(quint32 fileId) {
    auto it = m_files.find(fileId);
    if (it == m_files.end() || !it->sending) {
        return;
    }

    while (it->nextChunk < it->chunks
           && m_socket->bytesToWrite() < kMaxPendingBytes) {
        it->file->seek(qint64(it->nextChunk) * proto::kChunkSize);
        const QByteArray data = it->file->read(proto::kChunkSize);
        if (data.isEmpty()) {
            break;
        }

        QByteArray payload;
        payload.reserve(8 + data.size());
        quint32 be = qToBigEndian(fileId);
        payload.append(reinterpret_cast<const char *>(&be), 4);
        be = qToBigEndian(it->nextChunk);
        payload.append(reinterpret_cast<const char *>(&be), 4);
        payload.append(data);

        m_socket->write(proto::encode(proto::Type::FileChunk, payload));
        ++it->nextChunk;
        it->transferred += data.size();
        emit fileProgress(fileId, it->transferred, it->size);
    }

    if (it->nextChunk >= it->chunks) {
        QJsonObject end;
        end["fileId"] = qint64(fileId);
        end["ok"] = true;
        m_socket->write(proto::encode(proto::Type::FileEnd, toJson(end)));
        finishFile(fileId, true, "已发送");
    }
}

void ChatClient::handleFileOffer(const QByteArray &payload) {
    const QJsonObject offer = QJsonDocument::fromJson(payload).object();

    FileTask task;
    task.id = quint32(offer.value("fileId").toInteger());
    task.sending = false;
    task.peerName = offer.value("fromName").toString();
    task.fileName = offer.value("name").toString();
    task.size = offer.value("size").toInteger();
    task.chunks = quint32(offer.value("chunks").toInteger());
    task.savePath = uniqueDownloadPath(task.fileName);
    task.file = new QFile(task.savePath);

    if (!task.file->open(QIODevice::WriteOnly)) {
        QJsonObject reject;
        reject["fileId"] = qint64(task.id);
        m_socket->write(proto::encode(proto::Type::FileReject, toJson(reject)));
        delete task.file;
        emit errorOccurred("无法写入文件：" + task.savePath);
        return;
    }

    m_files.insert(task.id, task);
    emit fileOffer(task.id, task.peerName, task.fileName, task.size);

    QJsonObject accept;
    accept["fileId"] = qint64(task.id);
    m_socket->write(proto::encode(proto::Type::FileAccept, toJson(accept)));
}

void ChatClient::handleFileChunk(const QByteArray &payload) {
    if (payload.size() < 8) {
        return;
    }

    const quint32 fileId = qFromBigEndian<quint32>(payload.constData());
    const quint32 seq = qFromBigEndian<quint32>(payload.constData() + 4);

    auto it = m_files.find(fileId);
    if (it == m_files.end() || it->sending) {
        return;
    }

    if (seq != it->nextSeq) {
        QJsonObject end;
        end["fileId"] = qint64(fileId);
        end["ok"] = false;
        m_socket->write(proto::encode(proto::Type::FileEnd, toJson(end)));
        finishFile(fileId, false, "块序号不正确");
        return;
    }

    const QByteArray data = payload.mid(8);
    it->file->write(data);
    ++it->nextSeq;
    it->transferred += data.size();
    emit fileProgress(fileId, it->transferred, it->size);
}

void ChatClient::handleFileEnd(const QByteArray &payload) {
    const QJsonObject end = QJsonDocument::fromJson(payload).object();
    const quint32 fileId = quint32(end.value("fileId").toInteger());
    const bool ok = end.value("ok").toBool();

    auto it = m_files.find(fileId);
    if (it == m_files.end()) {
        return;
    }

    if (it->sending) {
        finishFile(fileId, false, "对方取消");
        return;
    }

    it->file->close();
    if (ok) {
        finishFile(fileId, true, it->savePath);
    } else {
        QFile::remove(it->savePath);
        finishFile(fileId, false, "对方取消");
    }
}

void ChatClient::finishFile(quint32 fileId, bool ok, const QString &message) {
    auto it = m_files.find(fileId);
    if (it == m_files.end()) {
        return;
    }
    if (it->file) {
        it->file->close();
        delete it->file;
    }
    m_files.erase(it);
    emit fileFinished(fileId, ok, message);
}

QString ChatClient::uniqueDownloadPath(const QString &fileName) const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath();
    }
    QDir().mkpath(dir);

    const QFileInfo info(fileName);
    QString candidate = dir + QLatin1Char('/') + info.fileName();
    int index = 1;
    while (QFile::exists(candidate)) {
        candidate = QStringLiteral("%1/%2 (%3).%4")
                        .arg(dir, info.completeBaseName())
                        .arg(index++)
                        .arg(info.suffix());
    }
    return candidate;
}

void ChatClient::onReadyRead() {
    m_buffer.append(m_socket->readAll());

    for (;;) {
        proto::Type type;
        QByteArray payload;
        const proto::DecodeStatus status = proto::decode(m_buffer, &type, &payload);

        if (status == proto::DecodeStatus::NeedMore) {
            return;
        }
        if (status != proto::DecodeStatus::Ok) {
            m_socket->abort();                         // 流错位，按断线处理
            return;
        }

        if (type == proto::Type::Pong) {
            m_lastPongMs = QDateTime::currentMSecsSinceEpoch();
            continue;
        }
        if (type == proto::Type::FileOffer) {
            handleFileOffer(payload);
            continue;
        }
        if (type == proto::Type::FileChunk) {
            handleFileChunk(payload);
            continue;
        }
        if (type == proto::Type::FileEnd) {
            handleFileEnd(payload);
            continue;
        }

        const QJsonObject message = QJsonDocument::fromJson(payload).object();
        switch (type) {
        case proto::Type::LoginAck: {
            const bool ok = message.value("ok").toBool();
            if (ok) {
                m_selfId = qint64(message.value("selfId").toInteger());
            } else {
                m_selfId = 0;
                m_loginRejected = true;
                m_wantConnected = false;               // 名字被占，重连也没用
            }
            emit loginResult(ok, m_selfId,
                             ok ? message.value("name").toString()
                                : message.value("reason").toString());
            break;
        }
        case proto::Type::UserList: {
            QList<ChatUser> users;
            const QJsonArray array = message.value("users").toArray();
            for (const QJsonValue &value : array) {
                const QJsonObject object = value.toObject();
                users.append({qint64(object.value("id").toInteger()),
                              object.value("name").toString()});
            }
            emit userList(users);
            break;
        }
        case proto::Type::UserJoin:
            emit userJoined({qint64(message.value("id").toInteger()),
                             message.value("name").toString()});
            break;
        case proto::Type::UserLeave:
            emit userLeft(qint64(message.value("id").toInteger()),
                          message.value("name").toString());
            break;
        case proto::Type::ChatText:
            emit textReceived(message.value("fromName").toString(),
                              message.value("text").toString(),
                              message.value("to").toInteger() != 0);
            break;
        case proto::Type::FileAccept:
            pumpFile(quint32(message.value("fileId").toInteger()));
            break;
        case proto::Type::FileReject:
            finishFile(quint32(message.value("fileId").toInteger()), false, "对方拒绝");
            break;
        case proto::Type::Error:
            emit errorOccurred(message.value("msg").toString());
            break;
        default:
            break;
        }
    }
}
