#include "mainwindow.h"

#include "chatclient.h"
#include "historystore.h"

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

constexpr int kPageSize = 200;
constexpr int kBubbleTextWidth = 380;   // 气泡内文字最大宽度
constexpr int kBubblePadX = 12;
constexpr int kBubblePadY = 8;

QString avatarColor(const QString &name) {
    static const QStringList colors = {"#5b8ff9", "#f6a623", "#52c41a",
                                       "#13c2c2", "#9254de", "#f56c6c"};
    return colors.at(int(qHash(name) % uint(colors.size())));
}

QString clockText(qint64 ts) {
    return QDateTime::fromMSecsSinceEpoch(ts).toString("HH:mm");
}

}  // namespace

MainWindow::MainWindow(QWidget *parent) : QWidget(parent) {
    m_client = new ChatClient(this);
    m_history = new HistoryStore(this);
    buildUi();
    openHistory();

    QSettings settings;
    m_nickname->setText(settings.value("lastName", "guest").toString());

    connect(m_client, &ChatClient::connected, this, &MainWindow::onConnected);
    connect(m_client, &ChatClient::disconnected, this, &MainWindow::onDisconnected);
    connect(m_client, &ChatClient::reconnecting, this, &MainWindow::onReconnecting);
    connect(m_client, &ChatClient::loginResult, this, &MainWindow::onLoginResult);
    connect(m_client, &ChatClient::userList, this, &MainWindow::onUserList);
    connect(m_client, &ChatClient::userJoined, this, &MainWindow::onUserJoined);
    connect(m_client, &ChatClient::userLeft, this, &MainWindow::onUserLeft);
    connect(m_client, &ChatClient::textReceived, this, &MainWindow::onTextReceived);
    connect(m_client, &ChatClient::errorOccurred, this, &MainWindow::onError);
    connect(m_client, &ChatClient::fileOffer, this, &MainWindow::onFileOffer);
    connect(m_client, &ChatClient::fileProgress, this, &MainWindow::onFileProgress);
    connect(m_client, &ChatClient::fileFinished, this, &MainWindow::onFileFinished);
}

void MainWindow::buildUi() {
    setWindowTitle("qt-chat");
    resize(960, 640);
    setMinimumSize(760, 520);

    // ---------- 左侧：在线列表 ----------
    auto *leftPanel = new QWidget(this);
    leftPanel->setFixedWidth(210);
    leftPanel->setStyleSheet("background:#f7f7f7;border-right:1px solid #e3e3e3;");

    auto *onlineTitle = new QLabel("在线", leftPanel);
    onlineTitle->setStyleSheet("font-weight:bold;color:#444;font-size:14px;");

    m_users = new QListWidget(leftPanel);
    m_users->setStyleSheet(
        "QListWidget{background:transparent;border:none;}"
        "QListWidget::item{height:34px;padding-left:6px;color:#333;}"
        "QListWidget::item:selected{background:#e9e9e9;color:#000;}");

    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 14, 12, 12);
    leftLayout->setSpacing(8);
    leftLayout->addWidget(onlineTitle);
    leftLayout->addWidget(m_users);

    // ---------- 右侧：顶部连接栏 ----------
    m_nickname = new QLineEdit("guest", this);
    m_nickname->setMaximumWidth(110);
    m_host = new QLineEdit("127.0.0.1", this);
    m_port = new QSpinBox(this);
    m_port->setRange(1, 65535);
    m_port->setValue(9527);
    m_port->setMaximumWidth(90);
    m_connectButton = new QPushButton("连接", this);
    m_status = new QLabel("● 未连接", this);
    m_status->setStyleSheet("color:#999;font-size:12px;");

    auto *topBar = new QWidget(this);
    topBar->setStyleSheet("background:#f7f7f7;border-bottom:1px solid #e3e3e3;");
    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(12, 8, 12, 8);
    topLayout->setSpacing(8);
    topLayout->addWidget(new QLabel("昵称", topBar));
    topLayout->addWidget(m_nickname);
    topLayout->addWidget(new QLabel("服务器", topBar));
    topLayout->addWidget(m_host);
    topLayout->addWidget(m_port);
    topLayout->addWidget(m_connectButton);
    topLayout->addStretch(1);
    topLayout->addWidget(m_status);

    // ---------- 右侧：消息区 ----------
    m_loadEarlier = new QPushButton("加载更早", this);
    m_loadEarlier->setVisible(false);
    m_loadEarlier->setStyleSheet("QPushButton{border:none;color:#576b95;background:transparent;}"
                                 "QPushButton:hover{color:#1a4fbf;}");

    m_messages = new QListWidget(this);
    m_messages->setSelectionMode(QAbstractItemView::NoSelection);
    m_messages->setFocusPolicy(Qt::NoFocus);
    m_messages->setStyleSheet("QListWidget{background:#f5f5f5;border:none;}");

    // ---------- 右侧：输入区 ----------
    m_input = new QTextEdit(this);
    m_input->setPlaceholderText("输入消息，Enter 发送，Shift+Enter 换行");
    m_input->setFixedHeight(88);
    m_input->installEventFilter(this);
    m_input->setStyleSheet("QTextEdit{background:#ffffff;border:1px solid #e0e0e0;"
                           "border-radius:6px;padding:6px;}");

    m_sendFileButton = new QPushButton("文件", this);
    m_sendFileButton->setFixedSize(64, 34);
    m_sendFileButton->setStyleSheet("QPushButton{background:#f2f2f2;border:1px solid #e0e0e0;"
                                    "border-radius:4px;color:#333;font-size:13px;}"
                                    "QPushButton:hover{background:#e8e8e8;}");

    m_sendButton = new QPushButton("发送", this);
    m_sendButton->setFixedSize(88, 34);
    m_sendButton->setStyleSheet("QPushButton{background:#07c160;color:white;border:none;"
                                "border-radius:4px;font-size:13px;}"
                                "QPushButton:hover{background:#06ad56;}");

    m_targetLabel = new QLabel("发送给：所有人", this);
    m_targetLabel->setStyleSheet("color:#666;font-size:12px;");

    auto *inputRow = new QHBoxLayout;
    inputRow->setContentsMargins(0, 0, 0, 0);
    inputRow->setSpacing(8);
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(m_sendFileButton, 0, Qt::AlignBottom);
    inputRow->addWidget(m_sendButton, 0, Qt::AlignBottom);

    auto *inputArea = new QWidget(this);
    inputArea->setStyleSheet("background:#f7f7f7;border-top:1px solid #e3e3e3;");
    auto *inputLayout = new QVBoxLayout(inputArea);
    inputLayout->setContentsMargins(12, 8, 12, 10);
    inputLayout->setSpacing(6);
    inputLayout->addWidget(m_targetLabel);
    inputLayout->addLayout(inputRow);

    auto *rightLayout = new QVBoxLayout;
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    rightLayout->addWidget(topBar);
    rightLayout->addWidget(m_loadEarlier, 0, Qt::AlignHCenter);
    rightLayout->addWidget(m_messages, 1);
    rightLayout->addWidget(inputArea);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(leftPanel);
    root->addLayout(rightLayout, 1);

    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_sendButton, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    connect(m_sendFileButton, &QPushButton::clicked, this, &MainWindow::onSendFileClicked);
    connect(m_loadEarlier, &QPushButton::clicked, this, &MainWindow::onLoadEarlierClicked);
    connect(m_users, &QListWidget::itemSelectionChanged,
            this, &MainWindow::onUserSelectionChanged);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        const bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
        if (enter && !(key->modifiers() & Qt::ShiftModifier)) {
            onSendClicked();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QWidget *MainWindow::createMessageWidget(const ChatMessage &message) const {
    const bool self = message.direction == 1;
    const QString name = self ? m_selfName : message.sender;

    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 5, 12, 5);
    layout->setSpacing(8);

    auto *avatar = new QLabel(name.left(1).toUpper(), row);
    avatar->setFixedSize(34, 34);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet(QStringLiteral("background:%1;color:white;border-radius:17px;"
                                         "font-weight:bold;font-size:14px;")
                              .arg(avatarColor(name)));

    QString metaText;
    if (message.peer != QLatin1String("all")) {
        metaText = self ? QStringLiteral("我 → %1（私聊）").arg(message.peer)
                        : QStringLiteral("%1（私聊）").arg(name);
    } else {
        metaText = name;
    }
    auto *meta = new QLabel(metaText + "   " + clockText(message.ts), row);
    meta->setStyleSheet("color:#999;font-size:11px;");
    meta->setAlignment(self ? Qt::AlignRight : Qt::AlignLeft);

    auto *bubble = new QLabel(message.body, row);
    bubble->setWordWrap(true);
    bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bubble->setStyleSheet(self
        ? "background:#95ec69;border-radius:8px;padding:8px 12px;color:#111;"
        : "background:#ffffff;border:1px solid #e6e6e6;border-radius:8px;"
          "padding:8px 12px;color:#111;");

    const QFontMetrics fm(bubble->font());
    const int textWidth = fm.boundingRect(QRect(0, 0, kBubbleTextWidth, 100000),
                                          Qt::TextWordWrap, message.body).width();
    // 多留几像素：QSS 的 padding/border 会占掉内容宽度，正好等于文字宽度时会裁掉最后一两个字
    const int contentWidth = qBound(40, textWidth + 6, kBubbleTextWidth);
    const int width = contentWidth + 2 * kBubblePadX + 2;
    const int height = fm.boundingRect(QRect(0, 0, contentWidth, 100000),
                                       Qt::TextWordWrap, message.body).height()
                       + 2 * kBubblePadY + 2;
    bubble->setFixedSize(width, height);

    auto *column = new QVBoxLayout;
    column->setSpacing(3);
    column->addWidget(meta);
    column->addWidget(bubble, 0, self ? Qt::AlignRight : Qt::AlignLeft);

    if (self) {
        layout->addStretch(1);
        layout->addLayout(column);
        layout->addWidget(avatar, 0, Qt::AlignTop);
    } else {
        layout->addWidget(avatar, 0, Qt::AlignTop);
        layout->addLayout(column);
        layout->addStretch(1);
    }
    return row;
}

QWidget *MainWindow::createNoticeWidget(const QString &text) const {
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 4, 12, 4);

    auto *label = new QLabel(text, row);
    label->setStyleSheet("color:#999;font-size:11px;background:#e9e9e9;"
                         "border-radius:6px;padding:3px 10px;");

    layout->addStretch(1);
    layout->addWidget(label);
    layout->addStretch(1);
    return row;
}

void MainWindow::appendWidget(QWidget *widget) {
    auto *item = new QListWidgetItem;
    item->setSizeHint(widget->sizeHint());
    m_messages->addItem(item);
    m_messages->setItemWidget(item, widget);
    m_messages->scrollToBottom();
}

void MainWindow::addNotice(const QString &text) {
    appendWidget(createNoticeWidget(text));
}

void MainWindow::addUserItem(const ChatUser &user) {
    if (!m_users->findItems(user.name, Qt::MatchExactly).isEmpty()) {
        return;
    }
    auto *item = new QListWidgetItem(m_users);
    item->setText(user.id == m_selfId ? user.name + "（我）" : user.name);
    item->setData(Qt::UserRole, user.id);
}

void MainWindow::updateTargetLabel() {
    QListWidgetItem *item = m_users->currentItem();
    if (item && item->data(Qt::UserRole).toLongLong() != m_selfId) {
        m_targetLabel->setText("发送给：" + item->text());
    } else {
        m_targetLabel->setText("发送给：所有人");
    }
}

void MainWindow::onUserSelectionChanged() {
    updateTargetLabel();
}

void MainWindow::openHistory() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/history.db");

    QString error;
    m_historyReady = m_history->open(path, &error);
    if (!m_historyReady) {
        addNotice("历史记录不可用：" + error);
        return;
    }
    addNotice("未登录，登录后显示该昵称的历史记录");
}

void MainWindow::loadHistory(const QString &owner) {
    const QList<ChatMessage> messages = m_history->latest(owner, kPageSize);
    for (const ChatMessage &message : messages) {
        appendWidget(createMessageWidget(message));
    }
    m_oldestId = messages.isEmpty() ? 0 : messages.first().id;
    m_loadEarlier->setVisible(messages.size() == kPageSize);
}

void MainWindow::onLoadEarlierClicked() {
    const QList<ChatMessage> messages = m_history->before(m_selfName, m_oldestId, kPageSize);
    if (messages.isEmpty()) {
        m_loadEarlier->setVisible(false);
        return;
    }

    QListWidgetItem *anchor = m_messages->item(0);

    int index = 0;
    for (const ChatMessage &message : messages) {
        QWidget *widget = createMessageWidget(message);
        auto *item = new QListWidgetItem;
        item->setSizeHint(widget->sizeHint());
        m_messages->insertItem(index, item);
        m_messages->setItemWidget(item, widget);
        ++index;
    }

    m_oldestId = messages.first().id;
    if (anchor) {
        m_messages->scrollToItem(anchor, QAbstractItemView::PositionAtTop);
    }
    m_loadEarlier->setVisible(messages.size() == kPageSize);
}

void MainWindow::recordMessage(const QString &peer, int direction,
                               const QString &sender, const QString &body) {
    if (!m_historyReady || m_selfName.isEmpty()) {
        return;
    }
    m_history->append(m_selfName, peer, sender, direction,
                      QDateTime::currentMSecsSinceEpoch(), body);
}

void MainWindow::setStatus(const QString &text, const QString &color) {
    m_status->setText(text);
    m_status->setStyleSheet(QStringLiteral("color:%1;font-size:12px;").arg(color));
}

void MainWindow::setConnectedUi(bool connected) {
    m_connectButton->setText(connected ? "断开" : "连接");
    m_nickname->setEnabled(!connected);
    if (!connected) {
        m_users->clear();
        updateTargetLabel();
    }
}

void MainWindow::onConnectClicked() {
    if (m_client->isActive()) {
        m_client->disconnectFromServer();
        return;
    }

    setStatus("● 连接中", "#fa9d3b");
    m_connectButton->setText("断开");
    m_nickname->setEnabled(false);
    addNotice("正在连接 " + m_host->text() + ":" + QString::number(m_port->value()));
    m_client->connectToServer(m_host->text().trimmed(),
                              quint16(m_port->value()),
                              m_nickname->text().trimmed());
}

void MainWindow::onSendClicked() {
    if (!m_client->isConnected()) {
        addNotice("还没连接");
        return;
    }

    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty()) {
        return;
    }
    m_input->clear();

    // 选中了某个在线用户就私聊，否则广播
    qint64 toId = 0;
    QString peer = QStringLiteral("all");
    if (QListWidgetItem *item = m_users->currentItem()) {
        const qint64 id = item->data(Qt::UserRole).toLongLong();
        if (id != m_selfId) {
            toId = id;
            peer = item->text();
        }
    }

    ChatMessage message;
    message.owner = m_selfName;
    message.peer = peer;
    message.sender = m_selfName;
    message.direction = 1;
    message.ts = QDateTime::currentMSecsSinceEpoch();
    message.body = text;
    appendWidget(createMessageWidget(message));

    recordMessage(peer, 1, m_selfName, text);
    m_client->sendText(text, toId);
}

void MainWindow::onConnected() {
    setStatus("● 已连接", "#07c160");
    setConnectedUi(true);
    addNotice("已连接服务器");
}

void MainWindow::onDisconnected() {
    if (m_client->isActive()) {
        setStatus("● 重连中", "#fa9d3b");
        addNotice("连接断开，等待重连");
        return;
    }
    setStatus("● 未连接", "#999999");
    addNotice("已断开");
    setConnectedUi(false);
}

void MainWindow::onReconnecting(int attempt, int delayMs) {
    addNotice(QStringLiteral("第 %1 次重连，%2 秒后")
                  .arg(attempt)
                  .arg(delayMs / 1000.0, 0, 'f', 1));
}

void MainWindow::onLoginResult(bool ok, qint64 selfId, const QString &text) {
    if (!ok) {
        addNotice("登录失败：" + text);
        return;
    }

    m_selfId = selfId;
    if (text != m_selfName) {
        // 换了身份：清屏后加载这个昵称自己的历史
        m_selfName = text;
        QSettings settings;
        settings.setValue("lastName", text);
        if (m_historyReady) {
            m_history->claimLegacy(text);
            m_messages->clear();
            m_fileWidgets.clear();
            loadHistory(text);
        }
    }
    addNotice("登录成功：" + text);
}

void MainWindow::onUserList(const QList<ChatUser> &users) {
    m_users->clear();
    for (const ChatUser &user : users) {
        addUserItem(user);
    }
    updateTargetLabel();
}

void MainWindow::onUserJoined(const ChatUser &user) {
    addUserItem(user);
}

void MainWindow::onUserLeft(qint64 id, const QString &) {
    for (int i = m_users->count() - 1; i >= 0; --i) {
        if (m_users->item(i)->data(Qt::UserRole).toLongLong() == id) {
            delete m_users->takeItem(i);
        }
    }
    updateTargetLabel();
}

void MainWindow::onTextReceived(const QString &name, const QString &text, bool isPrivate) {
    if (name == m_selfName) {
        return;                                        // 自己发的已经本地显示过
    }

    ChatMessage message;
    message.owner = m_selfName;
    message.peer = isPrivate ? name : QStringLiteral("all");
    message.sender = name;
    message.direction = 0;
    message.ts = QDateTime::currentMSecsSinceEpoch();
    message.body = text;
    appendWidget(createMessageWidget(message));

    recordMessage(message.peer, 0, name, text);
}

void MainWindow::onSendFileClicked() {
    if (!m_client->isConnected()) {
        addNotice("还没连接");
        return;
    }

    QListWidgetItem *item = m_users->currentItem();
    if (!item || item->data(Qt::UserRole).toLongLong() == m_selfId) {
        addNotice("先在左侧选一个在线用户，再发文件");
        return;
    }

    const QString path = QFileDialog::getOpenFileName(this, "选择要发送的文件");
    if (path.isEmpty()) {
        return;
    }

    const qint64 toId = item->data(Qt::UserRole).toLongLong();
    const quint32 id = m_client->sendFile(path, toId);
    if (id == 0) {
        return;
    }

    const QFileInfo info(path);
    appendWidget(createFileWidget(id, "发送文件：" + info.fileName(), info.size(), true));
    addNotice("等待 " + item->text() + " 接收");
}

QWidget *MainWindow::createFileWidget(quint32 id, const QString &title,
                                      qint64 total, bool outgoing) {
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 5, 12, 5);
    layout->setSpacing(8);

    auto *box = new QFrame(row);
    box->setFixedWidth(330);
    box->setStyleSheet("QFrame{background:#ffffff;border:1px solid #e6e6e6;border-radius:8px;}");

    auto *titleLabel = new QLabel(title, box);
    titleLabel->setWordWrap(true);
    titleLabel->setStyleSheet("color:#111;font-weight:bold;font-size:12px;border:none;");

    auto *bar = new QProgressBar(box);
    bar->setRange(0, 100);
    bar->setValue(0);
    bar->setTextVisible(false);
    bar->setFixedHeight(8);
    bar->setStyleSheet("QProgressBar{background:#eeeeee;border:none;border-radius:4px;}"
                       "QProgressBar::chunk{background:#07c160;border-radius:4px;}");

    auto *status = new QLabel("0%", box);
    status->setStyleSheet("color:#888;font-size:11px;border:none;");

    auto *cancel = new QPushButton("取消", box);
    cancel->setFixedSize(56, 24);
    cancel->setStyleSheet("QPushButton{background:transparent;border:1px solid #ddd;"
                          "border-radius:4px;color:#666;font-size:11px;}");

    auto *bottom = new QHBoxLayout;
    bottom->addWidget(status, 1);
    bottom->addWidget(cancel);

    auto *inner = new QVBoxLayout(box);
    inner->setContentsMargins(10, 8, 10, 8);
    inner->setSpacing(6);
    inner->addWidget(titleLabel);
    inner->addWidget(bar);
    inner->addLayout(bottom);

    if (outgoing) {
        layout->addStretch(1);
        layout->addWidget(box);
    } else {
        layout->addWidget(box);
        layout->addStretch(1);
    }

    m_fileWidgets.insert(id, FileWidgets{bar, status, cancel});
    connect(cancel, &QPushButton::clicked, this, [this, id] { m_client->cancelFile(id); });

    Q_UNUSED(total);
    return row;
}

void MainWindow::onFileOffer(quint32 id, const QString &fromName,
                             const QString &fileName, qint64 size) {
    appendWidget(createFileWidget(
        id, QStringLiteral("接收文件：%1（来自 %2）").arg(fileName, fromName), size, false));
    addNotice(QStringLiteral("正在接收 %1，保存在下载目录").arg(fileName));
}

void MainWindow::onFileProgress(quint32 id, qint64 transferred, qint64 total) {
    auto it = m_fileWidgets.find(id);
    if (it == m_fileWidgets.end()) {
        return;
    }
    const int percent = total > 0 ? int(transferred * 100 / total) : 0;
    it->bar->setValue(percent);
    it->status->setText(QStringLiteral("%1 / %2 MB · %3%")
                            .arg(transferred / 1048576.0, 0, 'f', 1)
                            .arg(total / 1048576.0, 0, 'f', 1)
                            .arg(percent));
}

void MainWindow::onFileFinished(quint32 id, bool ok, const QString &message) {
    auto it = m_fileWidgets.find(id);
    if (it != m_fileWidgets.end()) {
        if (ok) {
            it->bar->setValue(100);
        }
        it->status->setText(message);
        it->cancel->setVisible(false);
    }
    addNotice(ok ? "文件传输完成：" + message : "文件传输中止：" + message);
}

void MainWindow::onError(const QString &message) {
    addNotice("错误: " + message);
}
