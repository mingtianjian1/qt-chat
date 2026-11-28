#include "chatclient.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    ChatClient client;

    QWidget window;
    window.setWindowTitle("qt-chat");
    window.resize(760, 520);

    auto *nickname = new QLineEdit("guest", &window);
    nickname->setMaximumWidth(100);
    auto *host = new QLineEdit("127.0.0.1", &window);
    auto *port = new QSpinBox(&window);
    port->setRange(1, 65535);
    port->setValue(9527);
    auto *connectButton = new QPushButton("连接", &window);

    auto *log = new QTextEdit(&window);
    log->setReadOnly(true);

    auto *message = new QLineEdit(&window);
    message->setPlaceholderText("一行字，回车发送");
    auto *sendButton = new QPushButton("发送", &window);

    auto *users = new QListWidget(&window);
    users->setMaximumWidth(170);

    auto *connectRow = new QHBoxLayout;
    connectRow->addWidget(new QLabel("昵称", &window));
    connectRow->addWidget(nickname);
    connectRow->addWidget(new QLabel("服务器", &window));
    connectRow->addWidget(host);
    connectRow->addWidget(port);
    connectRow->addWidget(connectButton);

    auto *sendRow = new QHBoxLayout;
    sendRow->addWidget(message);
    sendRow->addWidget(sendButton);

    auto *left = new QVBoxLayout;
    left->addLayout(connectRow);
    left->addWidget(log);
    left->addLayout(sendRow);

    auto *right = new QVBoxLayout;
    right->addWidget(new QLabel("在线", &window));
    right->addWidget(users);

    auto *root = new QHBoxLayout(&window);
    root->addLayout(left, 1);
    root->addLayout(right);

    QObject::connect(&client, &ChatClient::connected, &window, [&] {
        log->append("已连接 " + host->text() + ":" + QString::number(port->value()));
        connectButton->setText("断开");
        nickname->setEnabled(false);
    });
    QObject::connect(&client, &ChatClient::disconnected, &window, [&] {
        log->append("已断开");
        connectButton->setText("连接");
        nickname->setEnabled(true);
        users->clear();
    });
    QObject::connect(&client, &ChatClient::loginResult, &window,
                     [&](bool ok, qint64, const QString &name) {
                         log->append(ok ? "登录成功：" + name : "登录失败：" + name);
                     });
    QObject::connect(&client, &ChatClient::userList, &window, [&](const QStringList &names) {
        users->clear();
        users->addItems(names);
    });
    QObject::connect(&client, &ChatClient::userJoined, &window, [&](const QString &name) {
        if (users->findItems(name, Qt::MatchExactly).isEmpty()) {
            users->addItem(name);
        }
    });
    QObject::connect(&client, &ChatClient::userLeft, &window, [&](const QString &name) {
        const auto items = users->findItems(name, Qt::MatchExactly);
        for (QListWidgetItem *item : items) {
            delete users->takeItem(users->row(item));
        }
    });
    QObject::connect(&client, &ChatClient::textReceived, &window,
                     [&](const QString &name, const QString &text) {
                         log->append(name + ": " + text);
                     });
    QObject::connect(&client, &ChatClient::errorOccurred, &window,
                     [&](const QString &text) { log->append("错误: " + text); });

    QObject::connect(connectButton, &QPushButton::clicked, &window, [&] {
        if (client.isConnected()) {
            client.disconnectFromServer();
        } else {
            client.connectToServer(host->text().trimmed(), quint16(port->value()),
                                   nickname->text().trimmed());
        }
    });
    QObject::connect(sendButton, &QPushButton::clicked, &window, [&] {
        if (!client.isConnected()) {
            log->append("还没连接");
            return;
        }
        if (message->text().isEmpty()) {
            return;
        }
        client.sendText(message->text());
        message->clear();
    });
    QObject::connect(message, &QLineEdit::returnPressed, sendButton, &QPushButton::click);

    window.show();
    return app.exec();
}
