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
    message.peer = query.value(1).toString();
    message.sender = query.value(2).toString();
    message.direction = query.value(3).toInt();
    message.ts = query.value(4).toLongLong();
    message.body = query.value(5).toString();
    return message;
}

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
    query.exec("CREATE INDEX IF NOT EXISTS idx_messages_peer_id "
               "ON messages(peer, id DESC)");

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

bool HistoryStore::append(const QString &peer, const QString &sender, int direction,
                          qint64 ts, const QString &body) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.isOpen()) {
        return false;
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO messages(peer, sender, direction, ts, body) "
                  "VALUES(?, ?, ?, ?, ?)");
    query.addBindValue(peer);
    query.addBindValue(sender);
    query.addBindValue(direction);
    query.addBindValue(ts);
    query.addBindValue(body);
    return query.exec();
}

QList<ChatMessage> HistoryStore::latest(int limit) {
    QList<ChatMessage> messages;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.isOpen()) {
        return messages;
    }

    QSqlQuery query(db);
    query.prepare("SELECT id, peer, sender, direction, ts, body FROM messages "
                  "ORDER BY id DESC LIMIT ?");
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

QList<ChatMessage> HistoryStore::before(qint64 beforeId, int limit) {
    QList<ChatMessage> messages;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.isOpen()) {
        return messages;
    }

    QSqlQuery query(db);
    query.prepare("SELECT id, peer, sender, direction, ts, body FROM messages "
                  "WHERE id < ? ORDER BY id DESC LIMIT ?");
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

int HistoryStore::count() {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.isOpen()) {
        return 0;
    }
    QSqlQuery query(db);
    if (!query.exec("SELECT COUNT(*) FROM messages") || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}
