#pragma once

#include "chatclient.h"

#include <QHash>
#include <QString>
#include <QWidget>

class HistoryStore;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTextEdit;
struct ChatMessage;

// 聊天窗口：左侧在线列表 + 右侧消息气泡 + 底部输入框。
// 收发逻辑在 ChatClient，历史在 HistoryStore。
class MainWindow : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onConnectClicked();
    void onSendClicked();
    void onSendFileClicked();
    void onLoadEarlierClicked();
    void onUserSelectionChanged();

    void onConnected();
    void onDisconnected();
    void onReconnecting(int attempt, int delayMs);
    void onLoginResult(bool ok, qint64 selfId, const QString &text);
    void onUserList(const QList<ChatUser> &users);
    void onUserJoined(const ChatUser &user);
    void onUserLeft(qint64 id, const QString &name);
    void onTextReceived(const QString &name, const QString &text, bool isPrivate);
    void onError(const QString &message);

    void onFileOffer(quint32 id, const QString &fromName,
                     const QString &fileName, qint64 size);
    void onFileProgress(quint32 id, qint64 transferred, qint64 total);
    void onFileFinished(quint32 id, bool ok, const QString &message);

private:
    struct FileWidgets {
        QProgressBar *bar = nullptr;
        QLabel *status = nullptr;
        QPushButton *cancel = nullptr;
    };

    void buildUi();
    void setConnectedUi(bool connected);
    void setStatus(const QString &text, const QString &color);

    void openHistory();
    void loadHistory(const QString &owner);
    void recordMessage(const QString &peer, int direction,
                       const QString &sender, const QString &body);

    QWidget *createMessageWidget(const ChatMessage &message) const;
    QWidget *createNoticeWidget(const QString &text) const;
    QWidget *createFileWidget(quint32 id, const QString &title,
                              qint64 total, bool outgoing);
    void appendWidget(QWidget *widget);
    void addNotice(const QString &text);
    void addUserItem(const ChatUser &user);
    void updateTargetLabel();

    ChatClient *m_client = nullptr;
    HistoryStore *m_history = nullptr;
    bool m_historyReady = false;

    QLineEdit *m_nickname = nullptr;
    QLineEdit *m_host = nullptr;
    QSpinBox *m_port = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_loadEarlier = nullptr;
    QListWidget *m_messages = nullptr;
    QListWidget *m_users = nullptr;
    QTextEdit *m_input = nullptr;
    QPushButton *m_sendFileButton = nullptr;
    QPushButton *m_sendButton = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_targetLabel = nullptr;

    QString m_selfName;
    qint64 m_selfId = 0;
    qint64 m_oldestId = 0;

    QHash<quint32, FileWidgets> m_fileWidgets;
};
