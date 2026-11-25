#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

// v0 骨架：界面先出来，网络两处留成 TODO。
// 下一步：加 ChatClient（QTcpSocket + 读缓冲 + proto::decode），
// 连接/断开/收到帧都通过信号回给这个窗口，界面层不碰 QByteArray。
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("qt-chat");
    window.resize(560, 460);

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

    QObject::connect(connectButton, &QPushButton::clicked, &window, [=] {
        log->append("TODO(v0): 用 QTcpSocket 连接 " + host->text()
                    + ":" + QString::number(port->value()));
    });
    QObject::connect(sendButton, &QPushButton::clicked, &window, [=] {
        if (message->text().isEmpty()) {
            return;
        }
        log->append("TODO(v0): proto::encode(ChatText) 后 write " + message->text());
        message->clear();
    });
    QObject::connect(message, &QLineEdit::returnPressed, sendButton, &QPushButton::click);

    window.show();
    return app.exec();
}
