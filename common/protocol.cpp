#include "protocol.h"

#include <QDataStream>

namespace proto {

const char *typeName(Type t) {
    switch (t) {
    case Type::LoginReq:    return "LoginReq";
    case Type::LoginAck:    return "LoginAck";
    case Type::UserList:    return "UserList";
    case Type::UserJoin:    return "UserJoin";
    case Type::UserLeave:   return "UserLeave";
    case Type::ChatText:    return "ChatText";
    case Type::FileOffer:   return "FileOffer";
    case Type::FileAccept:  return "FileAccept";
    case Type::FileReject:  return "FileReject";
    case Type::FileChunk:   return "FileChunk";
    case Type::FileEnd:     return "FileEnd";
    case Type::Ping:        return "Ping";
    case Type::Pong:        return "Pong";
    case Type::Error:       return "Error";
    }
    return "Unknown";
}

QByteArray encode(Type type, const QByteArray &payload) {
    QByteArray frame;
    frame.reserve(kHeaderSize + payload.size());

    QDataStream out(&frame, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);
    out << kMagic << quint32(payload.size()) << quint16(type) << quint16(0);

    frame.append(payload);
    return frame;
}

DecodeStatus decode(QByteArray &buffer, Type *type, QByteArray *payload) {
    if (buffer.size() < kHeaderSize) {
        return DecodeStatus::NeedMore;          // 头都没收全
    }

    QDataStream in(buffer);
    in.setByteOrder(QDataStream::BigEndian);

    quint32 magic = 0;
    quint32 len = 0;
    quint16 rawType = 0;
    quint16 flags = 0;
    in >> magic >> len >> rawType >> flags;
    (void)flags;

    if (magic != kMagic) {
        return DecodeStatus::BadMagic;          // 错位后永远解不出来，只能断
    }
    if (len > kMaxPayload) {
        return DecodeStatus::TooLarge;          // 防伪造长度把内存打爆
    }
    if (quint32(buffer.size()) < kHeaderSize + len) {
        return DecodeStatus::NeedMore;          // 半包，等下一次
    }

    buffer.remove(0, kHeaderSize);
    *payload = buffer.left(int(len));
    buffer.remove(0, int(len));
    *type = static_cast<Type>(rawType);
    return DecodeStatus::Ok;
}

}  // namespace proto
