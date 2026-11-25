#pragma once

#include <QByteArray>
#include <QtGlobal>

// 帧格式（12 字节头，大端）：
//   magic(u32) | payloadLen(u32) | type(u16) | flags(u16) | payload
//
// 收发两侧都用这个头，server 与 client 共用，避免两处手写不一致。
namespace proto {

constexpr quint32 kMagic      = 0x43484154;    // 'CHAT'
constexpr int     kHeaderSize = 12;
constexpr quint32 kMaxPayload = 1024 * 1024;   // 1 MB，超过就是伪造或错位
constexpr quint32 kChunkSize  = 256 * 1024;    // 文件块 256 KB

enum class Type : quint16 {
    LoginReq = 1, LoginAck,
    UserList, UserJoin, UserLeave,
    ChatText,
    FileOffer, FileAccept, FileReject, FileChunk, FileEnd,
    Ping, Pong,
    Error,
};

enum class DecodeStatus {
    Ok,        // 解出一帧，buffer 已消费掉这一帧
    NeedMore,  // 半包：继续等下一次 readyRead
    BadMagic,  // 流已错位，无法恢复，只能断开
    TooLarge,  // 长度超上限，拒收并断开
};

const char *typeName(Type t);

// 组一帧。payload 为空也行（Ping/Pong）。
QByteArray encode(Type type, const QByteArray &payload = {});

// 从缓冲区里解一帧。成功时消费 buffer 中的字节，并写出 type / payload；
// 失败时不消费 buffer，调用方按 DecodeStatus 决定等待还是断开。
DecodeStatus decode(QByteArray &buffer, Type *type, QByteArray *payload);

}  // namespace proto
