#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

class ChatClient;
class HistoryStore;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTextEdit;
struct ChatMessage;

// 聊天窗口：只负责界面与交互，收发逻辑在 ChatClient，历史在 HistoryStore。
class MainWindow : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onConnectClicked();
    void onSendClicked();
    void onLoadEarlierClicked();

    void onConnected();
    void onDisconnected();
    void onReconnecting(int attempt, int delayMs);
    void onLoginResult(bool ok, qint64 selfId, const QString &text);
    void onUserList(const QStringList &names);
    void onUserJoined(const QString &name);
    void onUserLeft(const QString &name);
    void onTextReceived(const QString &name, const QString &text);
    void onError(const QString &message);

private:
    void buildUi();
    void setConnectedUi(bool connected);

    void openHistory();
    void loadHistory();
    void recordMessage(int direction, const QString &sender, const QString &body);
    QString formatLine(const ChatMessage &message) const;

    ChatClient *m_client = nullptr;
    HistoryStore *m_history = nullptr;
    bool m_historyReady = false;

    QLineEdit *m_nickname = nullptr;
    QLineEdit *m_host = nullptr;
    QSpinBox *m_port = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_loadEarlier = nullptr;
    QTextEdit *m_log = nullptr;
    QLineEdit *m_message = nullptr;
    QPushButton *m_sendButton = nullptr;
    QListWidget *m_users = nullptr;

    QString m_selfName;
    qint64 m_oldestId = 0;
};
