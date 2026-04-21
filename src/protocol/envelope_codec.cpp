/**
 * @file envelope_codec.cpp
 * @brief Implementation of the EnvelopeCodec for Pairion WebSocket protocol messages.
 */

#include "envelope_codec.h"

#include <QJsonDocument>

namespace pairion::protocol {

QJsonObject EnvelopeCodec::serialize(const OutboundMessage &msg) {
    return std::visit(
        [](const auto &m) -> QJsonObject {
            using T = std::decay_t<decltype(m)>;

            if constexpr (std::is_same_v<T, DeviceIdentify>) {
                return {{QStringLiteral("type"), QString::fromUtf8(T::kType)},
                        {QStringLiteral("deviceId"), m.deviceId},
                        {QStringLiteral("bearerToken"), m.bearerToken},
                        {QStringLiteral("clientVersion"), m.clientVersion}};
            } else if constexpr (std::is_same_v<T, HeartbeatPing>) {
                return {{QStringLiteral("type"), QString::fromUtf8(T::kType)},
                        {QStringLiteral("timestamp"), m.timestamp}};
            } else if constexpr (std::is_same_v<T, WakeWordDetected>) {
                QJsonObject obj;
                obj[QStringLiteral("type")] = QString::fromUtf8(T::kType);
                obj[QStringLiteral("timestamp")] = m.timestamp;
                if (m.confidence.has_value()) {
                    obj[QStringLiteral("confidence")] = m.confidence.value();
                }
                return obj;
            } else if constexpr (std::is_same_v<T, AudioStreamStartIn>) {
                return {{QStringLiteral("type"), QString::fromUtf8(T::kType)},
                        {QStringLiteral("streamId"), m.streamId},
                        {QStringLiteral("codec"), m.codec},
                        {QStringLiteral("sampleRate"), m.sampleRate}};
            } else if constexpr (std::is_same_v<T, SpeechEnded>) {
                return {{QStringLiteral("type"), QString::fromUtf8(T::kType)},
                        {QStringLiteral("streamId"), m.streamId}};
            } else if constexpr (std::is_same_v<T, AudioStreamEndIn>) {
                return {{QStringLiteral("type"), QString::fromUtf8(T::kType)},
                        {QStringLiteral("streamId"), m.streamId},
                        {QStringLiteral("reason"), m.reason}};
            } else if constexpr (std::is_same_v<T, TextMessage>) {
                return {{QStringLiteral("type"), QString::fromUtf8(T::kType)},
                        {QStringLiteral("text"), m.text}};
            }
        },
        msg);
}

QString EnvelopeCodec::serializeToString(const OutboundMessage &msg) {
    QJsonDocument doc(serialize(msg));
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

std::optional<InboundMessage> EnvelopeCodec::deserialize(const QJsonObject &obj) {
    const QString type = obj[QStringLiteral("type")].toString();

    if (type == QLatin1String("SessionOpened")) {
        return SessionOpened{obj[QStringLiteral("sessionId")].toString(),
                             obj[QStringLiteral("serverVersion")].toString()};
    }
    if (type == QLatin1String("SessionClosed")) {
        return SessionClosed{obj[QStringLiteral("reason")].toString()};
    }
    if (type == QLatin1String("HeartbeatPong")) {
        return HeartbeatPong{obj[QStringLiteral("timestamp")].toString()};
    }
    if (type == QLatin1String("Error")) {
        return ErrorMessage{obj[QStringLiteral("code")].toString(),
                            obj[QStringLiteral("message")].toString()};
    }
    if (type == QLatin1String("AgentStateChange")) {
        return AgentStateChange{obj[QStringLiteral("state")].toString()};
    }
    if (type == QLatin1String("TranscriptPartial")) {
        return TranscriptPartial{obj[QStringLiteral("text")].toString()};
    }
    if (type == QLatin1String("TranscriptFinal")) {
        return TranscriptFinal{obj[QStringLiteral("text")].toString()};
    }
    if (type == QLatin1String("LlmTokenStream")) {
        return LlmTokenStream{obj[QStringLiteral("delta")].toString()};
    }
    if (type == QLatin1String("ToolCallStarted")) {
        return ToolCallStarted{obj[QStringLiteral("toolCallId")].toString(),
                               obj[QStringLiteral("toolName")].toString(),
                               obj[QStringLiteral("input")].toObject()};
    }
    if (type == QLatin1String("ToolCallCompleted")) {
        return ToolCallCompleted{obj[QStringLiteral("toolCallId")].toString(),
                                 obj[QStringLiteral("output")].toObject()};
    }
    if (type == QLatin1String("AudioStreamStart")) {
        return AudioStreamStartOut{obj[QStringLiteral("streamId")].toString(),
                                   obj[QStringLiteral("codec")].toString(),
                                   obj[QStringLiteral("sampleRate")].toInt()};
    }
    if (type == QLatin1String("AudioStreamEnd")) {
        return AudioStreamEndOut{obj[QStringLiteral("streamId")].toString(),
                                 obj[QStringLiteral("reason")].toString()};
    }
    if (type == QLatin1String("UnderBreathAck")) {
        std::optional<QString> ackType;
        if (obj.contains(QStringLiteral("acknowledgementType"))) {
            ackType = obj[QStringLiteral("acknowledgementType")].toString();
        }
        return UnderBreathAck{ackType};
    }
    if (type == QLatin1String("MapFocus")) {
        return MapFocus{obj[QStringLiteral("lat")].toDouble(),
                        obj[QStringLiteral("lon")].toDouble(),
                        obj[QStringLiteral("label")].toString(),
                        obj[QStringLiteral("zoom")].toString()};
    }
    if (type == QLatin1String("MapClear")) {
        return MapClear{};
    }

    return std::nullopt;
}

std::optional<InboundMessage> EnvelopeCodec::deserializeFromString(const QString &json) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::nullopt;
    }
    return deserialize(doc.object());
}

} // namespace pairion::protocol
