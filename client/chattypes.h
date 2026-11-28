#pragma once

#include <QList>
#include <QMetaType>
#include <QString>

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

Q_DECLARE_METATYPE(ChatUser)
Q_DECLARE_METATYPE(QList<ChatUser>)
