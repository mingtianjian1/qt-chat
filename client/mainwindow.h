#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

class ChatClient;
class HistoryStore;
class QLabel;
class QLineEdit;
class QListWidget;
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
    void setStatus(const QString &text, const QString &color);

    void openHistory();
    void loadHistory(const QString &owner);
    void recordMessage(int direction, const QString &sender, const QString &body);

    QWidget *createMessageWidget(const ChatMessage &message) const;
    QWidget *createNoticeWidget(const QString &text) const;
    void appendWidget(QWidget *widget);
    void prependWidget(QWidget *widget);
    void addNotice(const QString &text);
    void addUserItem(const QString &name);

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
    QPushButton *m_sendButton = nullptr;
    QLabel *m_status = nullptr;

    QString m_selfName;
    qint64 m_oldestId = 0;
};
