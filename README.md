# qt-chat

[![build](https://github.com/mingtianjian1/qt-chat/actions/workflows/build.yml/badge.svg)](https://github.com/mingtianjian1/qt-chat/actions/workflows/build.yml)

Qt 6 / C++17 的局域网聊天与文件传输工具。C/S 架构：服务端无状态中转，客户端负责界面与本地历史（SQLite）。

## 功能

- 文字消息：广播与私聊
- 在线列表、上线下线通知
- 心跳保活、断线重连
- 文件分块传输、进度显示、中途取消
- 本地聊天记录（SQLite）、分页加载

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

线程模型：v1 全部运行在 GUI 线程（`QTcpSocket` 本身异步，低负载场景足够）；v2 将网络层与文件 IO 整体 `moveToThread`，结果通过 QueuedConnection 回传界面。两个版本都保留响应时间与 CPU 占用的对比数据。

## 协议

12 字节头，大端：

| 偏移 | 长度 | 字段 | 说明 |
|---|---|---|---|
| 0 | 4 | `magic` | 固定 `0x43484154`（`'CHAT'`），错则流已错位，断开 |
| 4 | 4 | `payloadLen` | payload 字节数，上限 1 MB，超限拒收并断开 |
| 8 | 2 | `type` | 消息类型 |
| 10 | 2 | `flags` | 预留，固定 0 |
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

接收侧用缓冲循环解析：半包等待、多包循环、magic 校验、长度上限，实现在 `common/protocol.cpp`。

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

依赖：

- Qt 6（Widgets、Network 模块）
- CMake ≥ 3.16
- Ninja
- 与 Qt 配套的 C++17 编译器（Windows 下为 MinGW 64-bit）

Windows 上确保 Qt 的 `bin` 目录、CMake、Ninja 都在 `PATH` 里（后两者 Qt 安装器自带，位于 Qt 安装目录的 `Tools\CMake_64\bin` 与 `Tools\Ninja`），然后：

```bat
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=<Qt 安装目录>/<版本>/mingw_64
cmake --build build
```

把 `CMAKE_PREFIX_PATH` 设为环境变量后，`-D` 参数可以省掉。Linux / macOS 装好系统 Qt 6 后换成对应的 Qt 前缀即可。

也可以直接用 Qt Creator 打开 `CMakeLists.txt` 构建，它自带 CMake 与 Ninja。

运行 `build/server/chatserver.exe` 与 `build/client/chatclient.exe`。部署到其它机器时用 `windeployqt` 带上 Qt 运行库。

## 运行

先起服务端，再起两个客户端：

```bat
build\server\chatserver.exe
build\client\chatclient.exe
build\client\chatclient.exe
```

客户端填不同的昵称、点「连接」，发一行字，两个窗口都会显示 `昵称: 内容`，右侧列表是当前在线的人。服务端日志会打出连接数、登录、帧类型与 payload 长度。

## Roadmap

| 阶段 | 内容 | 状态 |
|---|---|---|
| v0 | 服务端监听、客户端连接、单条文字收发 | 已完成 |
| v1 | 昵称登录、在线列表、广播、上下线通知 | 已完成 |
| v1.1 | 心跳保活、断线重连 | 计划 |
| v1.5 | SQLite 历史记录、分页加载 | 计划 |
| v2 | 文件分块传输、进度、取消；网络层移入工作线程 | 计划 |
| v3 | ASan 全流程、GitHub Actions、`windeployqt` 打包 | 计划 |

## 不做

注册登录、服务端数据库与离线消息、群聊、富文本与表情、音视频、NAT 打洞、TLS、断点续传。这些功能与项目目标无关，不纳入范围。
