#include "chatclient.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
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
    window.resize(620, 480);

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

    auto *root = new QVBoxLayout(&window);
    root->addLayout(connectRow);
    root->addWidget(log);
    root->addLayout(sendRow);

    QObject::connect(&client, &ChatClient::connected, &window, [&] {
        log->append("已连接 " + host->text() + ":" + QString::number(port->value()));
        connectButton->setText("断开");
    });
    QObject::connect(&client, &ChatClient::disconnected, &window, [&] {
        log->append("已断开");
        connectButton->setText("连接");
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
            client.connectToServer(host->text().trimmed(), quint16(port->value()));
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
        client.sendText(nickname->text(), message->text());
        message->clear();
    });
    QObject::connect(message, &QLineEdit::returnPressed, sendButton, &QPushButton::click);

    window.show();
    return app.exec();
}
