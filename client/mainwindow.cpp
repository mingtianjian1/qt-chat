#include "mainwindow.h"

#include "chatclient.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QWidget(parent) {
    m_client = new ChatClient(this);
    buildUi();

    connect(m_client, &ChatClient::connected, this, &MainWindow::onConnected);
    connect(m_client, &ChatClient::disconnected, this, &MainWindow::onDisconnected);
    connect(m_client, &ChatClient::loginResult, this, &MainWindow::onLoginResult);
    connect(m_client, &ChatClient::userList, this, &MainWindow::onUserList);
    connect(m_client, &ChatClient::userJoined, this, &MainWindow::onUserJoined);
    connect(m_client, &ChatClient::userLeft, this, &MainWindow::onUserLeft);
    connect(m_client, &ChatClient::textReceived, this, &MainWindow::onTextReceived);
    connect(m_client, &ChatClient::errorOccurred, this, &MainWindow::onError);
}

void MainWindow::buildUi() {
    setWindowTitle("qt-chat");
    resize(760, 520);

    m_nickname = new QLineEdit("guest", this);
    m_nickname->setMaximumWidth(100);

    m_host = new QLineEdit("127.0.0.1", this);

    m_port = new QSpinBox(this);
    m_port->setRange(1, 65535);
    m_port->setValue(9527);

    m_connectButton = new QPushButton("连接", this);

    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);

    m_message = new QLineEdit(this);
    m_message->setPlaceholderText("一行字，回车发送");

    m_sendButton = new QPushButton("发送", this);

    m_users = new QListWidget(this);
    m_users->setMaximumWidth(170);

    auto *connectRow = new QHBoxLayout;
    connectRow->addWidget(new QLabel("昵称", this));
    connectRow->addWidget(m_nickname);
    connectRow->addWidget(new QLabel("服务器", this));
    connectRow->addWidget(m_host);
    connectRow->addWidget(m_port);
    connectRow->addWidget(m_connectButton);

    auto *sendRow = new QHBoxLayout;
    sendRow->addWidget(m_message);
    sendRow->addWidget(m_sendButton);

    auto *left = new QVBoxLayout;
    left->addLayout(connectRow);
    left->addWidget(m_log);
    left->addLayout(sendRow);

    auto *right = new QVBoxLayout;
    right->addWidget(new QLabel("在线", this));
    right->addWidget(m_users);

    auto *root = new QHBoxLayout(this);
    root->addLayout(left, 1);
    root->addLayout(right);

    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_sendButton, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    connect(m_message, &QLineEdit::returnPressed, m_sendButton, &QPushButton::click);
}

void MainWindow::setConnectedUi(bool connected) {
    m_connectButton->setText(connected ? "断开" : "连接");
    m_nickname->setEnabled(!connected);
    if (!connected) {
        m_users->clear();
    }
}

void MainWindow::onConnectClicked() {
    if (m_client->isConnected()) {
        m_client->disconnectFromServer();
        return;
    }
    m_client->connectToServer(m_host->text().trimmed(),
                              quint16(m_port->value()),
                              m_nickname->text().trimmed());
}

void MainWindow::onSendClicked() {
    if (!m_client->isConnected()) {
        m_log->append("还没连接");
        return;
    }
    if (m_message->text().isEmpty()) {
        return;
    }
    m_client->sendText(m_message->text());
    m_message->clear();
}

void MainWindow::onConnected() {
    m_log->append("已连接 " + m_host->text() + ":" + QString::number(m_port->value()));
    setConnectedUi(true);
}

void MainWindow::onDisconnected() {
    m_log->append("已断开");
    setConnectedUi(false);
}

void MainWindow::onLoginResult(bool ok, qint64, const QString &text) {
    m_log->append(ok ? "登录成功：" + text : "登录失败：" + text);
}

void MainWindow::onUserList(const QStringList &names) {
    m_users->clear();
    m_users->addItems(names);
}

void MainWindow::onUserJoined(const QString &name) {
    if (m_users->findItems(name, Qt::MatchExactly).isEmpty()) {
        m_users->addItem(name);
    }
}

void MainWindow::onUserLeft(const QString &name) {
    const auto items = m_users->findItems(name, Qt::MatchExactly);
    for (QListWidgetItem *item : items) {
        delete m_users->takeItem(m_users->row(item));
    }
}

void MainWindow::onTextReceived(const QString &name, const QString &text) {
    m_log->append(name + ": " + text);
}

void MainWindow::onError(const QString &message) {
    m_log->append("错误: " + message);
}
