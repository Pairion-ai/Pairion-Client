#pragma once

/**
 * @file messages.h
 * @brief WebSocket protocol message types for the Pairion AsyncAPI contract.
 *
 * Defines one C++ struct per message type in asyncapi.yaml. Outbound messages
 * (Client → Server) and inbound messages (Server → Client) are grouped into
 * std::variant types for type-safe dispatch.
 */

#include <optional>
#include <QJsonObject>
#include <QString>
#include <variant>

namespace pairion::protocol {

// ---------------------------------------------------------------------------
// Outbound messages (Client → Server)
// ---------------------------------------------------------------------------

/**
 * @brief Identifies this device to the server upon WebSocket connection.
 */
struct DeviceIdentify {
    static constexpr const char *kType = "DeviceIdentify";
    QString deviceId;
    QString bearerToken;
    QString clientVersion;
};

/**
 * @brief Heartbeat ping sent at regular intervals to keep the connection alive.
 */
struct HeartbeatPing {
    static constexpr const char *kType = "HeartbeatPing";
    QString timestamp;
};

/**
 * @brief Notifies the server that the wake word was detected locally.
 */
struct WakeWordDetected {
    static constexpr const char *kType = "WakeWordDetected";
    QString timestamp;
    std::optional<double> confidence;
};

/**
 * @brief Signals the start of an inbound audio stream from the client.
 */
struct AudioStreamStartIn {
    static constexpr const char *kType = "AudioStreamStart";
    QString streamId;
    QString codec = QStringLiteral("opus");
    int sampleRate = 16000;
};

/**
 * @brief Signals that the user has stopped speaking.
 */
struct SpeechEnded {
    static constexpr const char *kType = "SpeechEnded";
    QString streamId;
};

/**
 * @brief Signals the end of an inbound audio stream from the client.
 */
struct AudioStreamEndIn {
    static constexpr const char *kType = "AudioStreamEnd";
    QString streamId;
    QString reason;
};

/**
 * @brief A text message sent from the client to the server.
 */
struct TextMessage {
    static constexpr const char *kType = "TextMessage";
    QString text;
};

/// Variant of all outbound text-frame message types.
using OutboundMessage =
    std::variant<DeviceIdentify, HeartbeatPing, WakeWordDetected, AudioStreamStartIn, SpeechEnded,
                 AudioStreamEndIn, TextMessage>;

// ---------------------------------------------------------------------------
// Inbound messages (Server → Client)
// ---------------------------------------------------------------------------

/**
 * @brief Server acknowledges the device and opens a session.
 */
struct SessionOpened {
    static constexpr const char *kType = "SessionOpened";
    QString sessionId;
    QString serverVersion;
};

/**
 * @brief Server closes the session.
 */
struct SessionClosed {
    static constexpr const char *kType = "SessionClosed";
    QString reason;
};

/**
 * @brief Heartbeat pong returned by the server.
 */
struct HeartbeatPong {
    static constexpr const char *kType = "HeartbeatPong";
    QString timestamp;
};

/**
 * @brief Error message from the server.
 */
struct ErrorMessage {
    static constexpr const char *kType = "Error";
    QString code;
    QString message;
};

/**
 * @brief Notification that the agent's processing state has changed.
 */
struct AgentStateChange {
    static constexpr const char *kType = "AgentStateChange";
    QString state;
};

/**
 * @brief Partial (in-progress) transcript from speech recognition.
 */
struct TranscriptPartial {
    static constexpr const char *kType = "TranscriptPartial";
    QString text;
};

/**
 * @brief Final transcript from speech recognition.
 */
struct TranscriptFinal {
    static constexpr const char *kType = "TranscriptFinal";
    QString text;
};

/**
 * @brief Streamed LLM token delta.
 */
struct LlmTokenStream {
    static constexpr const char *kType = "LlmTokenStream";
    QString delta;
};

/**
 * @brief Notification that a tool call has started on the server.
 */
struct ToolCallStarted {
    static constexpr const char *kType = "ToolCallStarted";
    QString toolCallId;
    QString toolName;
    QJsonObject input;
};

/**
 * @brief Notification that a tool call has completed on the server.
 */
struct ToolCallCompleted {
    static constexpr const char *kType = "ToolCallCompleted";
    QString toolCallId;
    QJsonObject output;
};

/**
 * @brief Signals the start of an outbound audio stream from the server.
 */
struct AudioStreamStartOut {
    static constexpr const char *kType = "AudioStreamStart";
    QString streamId;
    QString codec;
    int sampleRate = 0;
};

/**
 * @brief Signals the end of an outbound audio stream from the server.
 */
struct AudioStreamEndOut {
    static constexpr const char *kType = "AudioStreamEnd";
    QString streamId;
    QString reason;
};

/**
 * @brief Server's under-breath acknowledgement during barge-in.
 */
struct UnderBreathAck {
    static constexpr const char *kType = "UnderBreathAck";
    std::optional<QString> acknowledgementType;
};

/// Variant of all inbound text-frame message types.
using InboundMessage =
    std::variant<SessionOpened, SessionClosed, HeartbeatPong, ErrorMessage, AgentStateChange,
                 TranscriptPartial, TranscriptFinal, LlmTokenStream, ToolCallStarted,
                 ToolCallCompleted, AudioStreamStartOut, AudioStreamEndOut, UnderBreathAck>;

} // namespace pairion::protocol
