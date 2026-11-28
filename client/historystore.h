#pragma once

#include <QList>
#include <QObject>
#include <QString>

struct ChatMessage {
    qint64 id = 0;
    QString peer;        // 广播消息固定为 "all"
    QString sender;      // 发送者昵称；自己发的记 "我"
    int direction = 0;   // 0 = 收到，1 = 发出
    qint64 ts = 0;       // unix 毫秒
    QString body;
};

// 客户端本地聊天记录：SQLite 单表，按 id 分页取。
class HistoryStore : public QObject {
    Q_OBJECT

public:
    explicit HistoryStore(QObject *parent = nullptr);
    ~HistoryStore() override;

    bool open(const QString &filePath, QString *error = nullptr);
    void close();

    bool append(const QString &peer, const QString &sender, int direction,
                qint64 ts, const QString &body);

    // 最近的 limit 条，返回按时间升序
    QList<ChatMessage> latest(int limit);
    // id 小于 beforeId 的 limit 条，返回按时间升序（用于"加载更早"）
    QList<ChatMessage> before(qint64 beforeId, int limit);

    int count();

private:
    QString m_connectionName;
    bool m_open = false;
};
