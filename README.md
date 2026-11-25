# qt-chat

[![build](https://github.com/mingtianjian1/qt-chat/actions/workflows/build.yml/badge.svg)](https://github.com/mingtianjian1/qt-chat/actions/workflows/build.yml)

Qt 6 / C++17 的局域网聊天与文件传输工具，C/S 架构：服务端无状态中转，客户端是 GUI、聊天记录存本地 SQLite。

这不是"再做一个微信"，而是练出五个能讲 20 分钟的面试故事：**协议怎么设计、粘包怎么处理、线程怎么挪、背压怎么做、程序怎么打包**。

## 架构

```
[client A] ──TCP──┐
                  ├── [chatserver：无状态中转]
[client B] ──TCP──┘
```

| 进程 | 职责 | 不做什么 |
|---|---|---|
| `chatserver` | 监听、在线表、登录、广播文字、转发文件块、心跳超时清理 | 不存历史、不做离线消息、不落库 |
| `chatclient` | 界面、本地昵称、聊天记录落库、文件选择与进度、重连 | 不监听端口、不承担服务端逻辑 |

线程模型分两段：**v1 全部跑 GUI 线程**（`QTcpSocket` 本身异步，低负载够用）；**v2 把网络层与文件 IO 整体 `moveToThread`**，结果用 QueuedConnection 回传。改造前后要有对比数字，否则故事不成立。

## 协议

12 字节头，大端：

| 偏移 | 长度 | 字段 | 说明 |
|---|---|---|---|
| 0 | 4 | `magic` | 固定 `0x43484154`（`'CHAT'`），错则流已错位，断开 |
| 4 | 4 | `payloadLen` | payload 字节数，上限 1 MB，超限拒收并断开 |
| 8 | 2 | `type` | 消息类型 |
| 10 | 2 | `flags` | 预留，v1 固定 0 |
| 12 | N | `payload` | 文字类为 UTF-8 JSON，文件块为二进制 |

| 类型 | 方向 | payload |
|---|---|---|
| `LoginReq` / `LoginAck` | C→S / S→C | JSON：`name` / `ok, code, selfId, reason` |
| `UserList` / `UserJoin` / `UserLeave` | S→C | JSON：`users:[{id,name}]` / `id, name` |
| `ChatText` | C→S→C | JSON：`from, fromName, to(0=广播), ts, text` |
| `FileOffer` / `FileAccept` / `FileReject` | C→S→C | JSON：`fileId, fromName, name, size, chunks` / `fileId` |
| `FileChunk` | C→S→C | 二进制：`fileId(u32) + seq(u32) + data` |
| `FileEnd` | C→S→C | JSON：`fileId, ok` |
| `Ping` / `Pong` | 双向 | 空 |
| `Error` | S→C | JSON：`code, msg` |

接收侧必须有缓冲循环：半包等待、多包循环、magic 校验、长度上限。三个保护缺一个都会在现场翻车——见 `common/protocol.cpp`。

## 目录

```
qt-chat/
├── CMakeLists.txt
├── common/          # 帧编解码，server / client 共用的静态库
│   ├── protocol.h
│   └── protocol.cpp
├── server/          # chatserver（QCoreApplication + QTcpServer）
└── client/          # chatclient（Qt Widgets）
```

## 构建

环境：Windows + Qt 6.9.3 MinGW 64-bit + CMake ≥ 3.16 + Ninja。

```bat
set PATH=C:\Qt\6.9.3\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;%PATH%
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.9.3/mingw_64
cmake --build build
```

运行 `build/server/chatserver.exe` 与 `build/client/chatclient.exe`。换台机器跑，需要 `windeployqt` 带上 Qt 的 DLL。

## 进度

- [x] 仓库、CMake 骨架、12 字节帧编解码（`common/`）
- [ ] **v0 骨架**（3h）：服务端监听、客户端连上、互发一行字；判据是改一个字对面 1 秒内收到、日志能看到帧类型
- [ ] **v1 可用**（8h）：昵称登录、在线列表、广播、上下线通知、心跳、断线重连
- [ ] **v1.5 本地库**（4h）：SQLite 存历史、分页、重启后还在
- [ ] **v2 文件**（10h）：选文件/拖拽、分块、进度、取消；网络层进工作线程
- [ ] **v3 收口**（5h）：ASan 零报错、GitHub Actions、windeployqt、README + 架构图

## 不做

注册登录、服务端数据库与离线消息、群聊、富文本/表情、音视频、NAT 打洞、TLS、断点续传。理由统一：不加新故事，只加工作量。
