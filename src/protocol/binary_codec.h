#pragma once

/**
 * @file binary_codec.h
 * @brief Encoder/decoder for binary WebSocket frames with a 4-byte stream ID prefix.
 *
 * Binary audio frames in the Pairion protocol carry a 4-byte prefix derived from
 * the stream ID UUID (first 4 bytes of the RFC 4122 raw representation, big-endian),
 * followed by the variable-length Opus payload. This codec handles assembly and
 * disassembly of those frames.
 */

#include <QByteArray>
#include <QString>

namespace pairion::protocol {

/**
 * @brief Result of decoding a binary frame.
 */
struct DecodedBinaryFrame {
    QByteArray prefix;  ///< 4-byte stream ID prefix.
    QByteArray payload; ///< Opus frame payload.
};

/**
 * @brief Codec for binary WebSocket audio frames with 4-byte stream ID prefix.
 *
 * The prefix is the first 4 bytes of the stream ID UUID in RFC 4122 big-endian
 * format (the time_low field). The server uses this prefix to route binary frames
 * to the correct audio stream session.
 */
class BinaryCodec {
  public:
    /// Length of the stream ID prefix in bytes.
    static constexpr int kPrefixLength = 4;

    /**
     * @brief Encode a binary frame with stream ID prefix.
     * @param streamId UUID string identifying the audio stream.
     * @param opusPayload Raw Opus frame bytes.
     * @return 4-byte prefix + payload concatenated.
     */
    static QByteArray encodeBinaryFrame(const QString &streamId, const QByteArray &opusPayload);

    /**
     * @brief Decode a binary frame into prefix and payload.
     * @param frame The raw binary frame (must be >= 4 bytes).
     * @return Decoded prefix and payload. Empty if frame is too short.
     */
    static DecodedBinaryFrame decodeBinaryFrame(const QByteArray &frame);

    /**
     * @brief Extract the 4-byte prefix from a stream ID UUID string.
     * @param streamId UUID string.
     * @return First 4 bytes of the UUID's RFC 4122 raw form.
     */
    static QByteArray streamIdToPrefix(const QString &streamId);
};

} // namespace pairion::protocol
