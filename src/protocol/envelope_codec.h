#pragma once

/**
 * @file envelope_codec.h
 * @brief JSON envelope serializer and deserializer for the Pairion WebSocket protocol.
 *
 * Converts between C++ message structs and JSON text frames. The `type` field
 * in the JSON envelope acts as the discriminator for deserialization.
 */

#include "messages.h"

#include <optional>
#include <QJsonObject>
#include <QString>

namespace pairion::protocol {

/**
 * @brief Codec for serializing outbound messages and deserializing inbound messages.
 *
 * Thread-safe: all methods are stateless and can be called from any thread.
 */
class EnvelopeCodec {
  public:
    /**
     * @brief Serialize an outbound message to a QJsonObject.
     * @param msg The outbound message variant.
     * @return JSON object ready for transmission as a text frame.
     */
    static QJsonObject serialize(const OutboundMessage &msg);

    /**
     * @brief Serialize an outbound message directly to a JSON string.
     * @param msg The outbound message variant.
     * @return Compact JSON string.
     */
    static QString serializeToString(const OutboundMessage &msg);

    /**
     * @brief Deserialize a JSON object into an inbound message.
     * @param obj The JSON object parsed from a text frame.
     * @return The deserialized message, or std::nullopt if the type is unknown.
     */
    static std::optional<InboundMessage> deserialize(const QJsonObject &obj);

    /**
     * @brief Deserialize a JSON string into an inbound message.
     * @param json The raw JSON string from a text frame.
     * @return The deserialized message, or std::nullopt on parse error or unknown type.
     */
    static std::optional<InboundMessage> deserializeFromString(const QString &json);
};

} // namespace pairion::protocol
