#pragma once

#include <QStringList>
#include <QWidget>

class ChatClient;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTextEdit;

// 聊天窗口：只负责界面与交互，收发逻辑都在 ChatClient 里。
class MainWindow : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onConnectClicked();
    void onSendClicked();

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

    ChatClient *m_client = nullptr;

    QLineEdit *m_nickname = nullptr;
    QLineEdit *m_host = nullptr;
    QSpinBox *m_port = nullptr;
    QPushButton *m_connectButton = nullptr;
    QTextEdit *m_log = nullptr;
    QLineEdit *m_message = nullptr;
    QPushButton *m_sendButton = nullptr;
    QListWidget *m_users = nullptr;
};
