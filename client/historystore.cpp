#include "historystore.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <algorithm>

namespace {

ChatMessage readRow(const QSqlQuery &query) {
    ChatMessage message;
    message.id = query.value(0).toLongLong();
    message.owner = query.value(1).toString();
    message.peer = query.value(2).toString();
    message.sender = query.value(3).toString();
    message.direction = query.value(4).toInt();
    message.ts = query.value(5).toLongLong();
    message.body = query.value(6).toString();
    return message;
}

const char *kColumns = "id, owner, peer, sender, direction, ts, body";

}  // namespace

HistoryStore::HistoryStore(QObject *parent) : QObject(parent) {
    m_connectionName = QStringLiteral("history-%1").arg(quintptr(this));
}

HistoryStore::~HistoryStore() {
    close();
}

bool HistoryStore::open(const QString &filePath, QString *error) {
    if (m_open) {
        return true;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    db.setDatabaseName(filePath);
    if (!db.open()) {
        if (error) {
            *error = db.lastError().text();
        }
        return false;
    }

    QSqlQuery query(db);
    query.exec("PRAGMA journal_mode=WAL");
    query.exec("PRAGMA synchronous=NORMAL");

    if (!query.exec("CREATE TABLE IF NOT EXISTS messages("
                    " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    " owner TEXT NOT NULL DEFAULT '',"
                    " peer TEXT NOT NULL,"
                    " sender TEXT NOT NULL,"
                    " direction INTEGER NOT NULL,"
                    " ts INTEGER NOT NULL,"
                    " body TEXT NOT NULL)")) {
        if (error) {
            *error = query.lastError().text();
        }
        db.close();
        return false;
    }

    // 老库没有 owner 列：补一列，旧记录 owner 为空，等下次登录认领
    bool hasOwner = false;
    QSqlQuery info(db);
    if (info.exec("PRAGMA table_info(messages)")) {
        while (info.next()) {
            if (info.value(1).toString() == QLatin1String("owner")) {
                hasOwner = true;
            }
        }
    }
    if (!hasOwner) {
        query.exec("ALTER TABLE messages ADD COLUMN owner TEXT NOT NULL DEFAULT ''");
    }

    query.exec("CREATE INDEX IF NOT EXISTS idx_messages_owner_id "
               "ON messages(owner, id DESC)");

    m_open = true;
    return true;
}

void HistoryStore::close() {
    if (!m_open) {
        return;
    }
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isOpen()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(m_connectionName);
    m_open = false;
}

bool HistoryStore::append(const QString &owner, const QString &peer, const QString &sender,
                          int direction, qint64 ts, const QString &body) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.isOpen()) {
        return false;
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO messages(owner, peer, sender, direction, ts, body) "
                  "VALUES(?, ?, ?, ?, ?, ?)");
    query.addBindValue(owner);
    query.addBindValue(peer);
    query.addBindValue(sender);
    query.addBindValue(direction);
    query.addBindValue(ts);
    query.addBindValue(body);
    return query.exec();
}

QList<ChatMessage> HistoryStore::latest(const QString &owner, int limit) {
    QList<ChatMessage> messages;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.isOpen()) {
        return messages;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT %1 FROM messages WHERE owner = ? "
                                 "ORDER BY id DESC LIMIT ?")
                      .arg(QLatin1String(kColumns)));
    query.addBindValue(owner);
    query.addBindValue(limit);
    if (!query.exec()) {
        return messages;
    }
    while (query.next()) {
        messages.append(readRow(query));
    }
    std::reverse(messages.begin(), messages.end());
    return messages;
}

QList<ChatMessage> HistoryStore::before(const QString &owner, qint64 beforeId, int limit) {
    QList<ChatMessage> messages;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.isOpen()) {
        return messages;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT %1 FROM messages WHERE owner = ? AND id < ? "
                                 "ORDER BY id DESC LIMIT ?")
                      .arg(QLatin1String(kColumns)));
    query.addBindValue(owner);
    query.addBindValue(beforeId);
    query.addBindValue(limit);
    if (!query.exec()) {
        return messages;
    }
    while (query.next()) {
        messages.append(readRow(query));
    }
    std::reverse(messages.begin(), messages.end());
    return messages;
}

bool HistoryStore::claimLegacy(const QString &owner) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.isOpen()) {
        return false;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE messages SET owner = ? WHERE owner = ''");
    query.addBindValue(owner);
    return query.exec();
}
