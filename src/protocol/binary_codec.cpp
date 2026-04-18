/**
 * @file binary_codec.cpp
 * @brief Implementation of the binary WebSocket frame codec.
 */

#include "binary_codec.h"

#include <QLoggingCategory>
#include <QUuid>

Q_LOGGING_CATEGORY(lcBinaryCodec, "pairion.protocol.binary")

namespace pairion::protocol {

QByteArray BinaryCodec::encodeBinaryFrame(const QString &streamId, const QByteArray &opusPayload) {
    QByteArray prefix = streamIdToPrefix(streamId);
    return prefix + opusPayload;
}

DecodedBinaryFrame BinaryCodec::decodeBinaryFrame(const QByteArray &frame) {
    if (frame.size() < kPrefixLength) {
        qCWarning(lcBinaryCodec) << "Binary frame too short:" << frame.size() << "bytes";
        return {};
    }
    return {frame.left(kPrefixLength), frame.mid(kPrefixLength)};
}

QByteArray BinaryCodec::streamIdToPrefix(const QString &streamId) {
    QUuid uuid = QUuid::fromString(streamId);
    if (uuid.isNull()) {
        qCWarning(lcBinaryCodec) << "Invalid stream ID UUID:" << streamId;
        return QByteArray(kPrefixLength, '\0');
    }
    QByteArray raw = uuid.toRfc4122();
    return raw.left(kPrefixLength);
}

} // namespace pairion::protocol
