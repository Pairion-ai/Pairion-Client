//! WebSocket message payload types matching the AsyncAPI specification.
//!
//! These structures mirror the `asyncapi.yaml` schemas for the connection
//! lifecycle messages used in M0: `DeviceIdentify`, `IdentifyAck`,
//! `HeartbeatPing`, `HeartbeatPong`, `Error`, and `LogForward`.

use serde::{Deserialize, Serialize};

/// Payload for the `DeviceIdentify` message sent on WebSocket open.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DeviceIdentifyPayload {
    /// Message type discriminator — always `"DeviceIdentify"`.
    pub r#type: String,
    /// The paired Device identifier.
    #[serde(rename = "deviceId")]
    pub device_id: String,
    /// Bearer token for authentication.
    pub token: String,
    /// Client application version.
    #[serde(rename = "clientVersion")]
    pub client_version: String,
    /// ISO 8601 timestamp.
    pub timestamp: String,
}

/// Payload for the `IdentifyAck` message from the Server.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IdentifyAckPayload {
    /// Message type discriminator — always `"IdentifyAck"`.
    pub r#type: String,
    /// Whether the identification was accepted.
    pub accepted: bool,
    /// Server application version.
    #[serde(rename = "serverVersion")]
    pub server_version: String,
    /// Reason for rejection, present only when `accepted` is false.
    pub reason: Option<String>,
    /// ISO 8601 timestamp.
    pub timestamp: String,
}

/// Payload for the `HeartbeatPing` message sent by the Client.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HeartbeatPingPayload {
    /// Message type discriminator — always `"HeartbeatPing"`.
    pub r#type: String,
    /// ISO 8601 timestamp.
    pub timestamp: String,
}

/// Payload for the `HeartbeatPong` message from the Server.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HeartbeatPongPayload {
    /// Message type discriminator — always `"HeartbeatPong"`.
    pub r#type: String,
    /// ISO 8601 timestamp.
    pub timestamp: String,
    /// Round-trip latency in milliseconds.
    #[serde(rename = "latencyMs")]
    pub latency_ms: f64,
}

/// Payload for the `Error` message from the Server.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ErrorPayload {
    /// Message type discriminator — always `"Error"`.
    pub r#type: String,
    /// Machine-readable error code.
    pub code: String,
    /// Human-readable error message.
    pub message: String,
}

/// Payload for the `LogForward` message sent by the Client.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LogForwardPayload {
    /// Message type discriminator — always `"LogForward"`.
    pub r#type: String,
    /// The device id of the sending Client.
    #[serde(rename = "sourceDeviceId")]
    pub source_device_id: Option<String>,
    /// Batch of log entries.
    pub entries: Vec<LogForwardEntry>,
    /// ISO 8601 timestamp.
    pub timestamp: String,
}

/// A single log entry within a `LogForward` message.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LogForwardEntry {
    /// ISO 8601 timestamp of the log event.
    pub timestamp: String,
    /// Log level.
    pub level: String,
    /// Subsystem that produced the entry.
    pub subsystem: String,
    /// Human-readable message.
    pub message: String,
}

// ── M1 Session lifecycle ───────────────────────────────────────

/// Payload for the `WakeWordDetected` message sent when wake phrase fires.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WakeWordDetectedPayload {
    /// Message type discriminator — always `"WakeWordDetected"`.
    pub r#type: String,
    /// Wake-word model confidence (0.0–1.0).
    pub confidence: f64,
    /// Audio signal-to-noise ratio in dB.
    #[serde(rename = "snrDb", skip_serializing_if = "Option::is_none")]
    pub snr_db: Option<f64>,
    /// ISO 8601 timestamp.
    pub timestamp: String,
}

/// Payload for the `AudioStreamStart` message announcing an audio stream.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AudioStreamStartPayload {
    /// Message type discriminator — always `"AudioStreamStart"`.
    pub r#type: String,
    /// Session this stream belongs to.
    #[serde(rename = "sessionId")]
    pub session_id: String,
    /// Unique identifier for this audio stream.
    #[serde(rename = "streamId")]
    pub stream_id: String,
    /// Direction: `"in"` for mic capture, `"out"` for TTS playback.
    pub direction: String,
    /// Audio codec identifier.
    pub codec: String,
    /// Sample rate in Hz.
    #[serde(rename = "sampleRate")]
    pub sample_rate: u32,
    /// Number of audio channels.
    pub channels: u32,
    /// ISO 8601 timestamp.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub timestamp: Option<String>,
}

/// Payload for the `AudioStreamEnd` message.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AudioStreamEndPayload {
    /// Message type discriminator — always `"AudioStreamEnd"`.
    pub r#type: String,
    /// Session this stream belongs to.
    #[serde(rename = "sessionId")]
    pub session_id: String,
    /// The stream being ended.
    #[serde(rename = "streamId")]
    pub stream_id: String,
    /// Reason for ending: `"normal"`, `"interrupted"`, or `"error"`.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub reason: Option<String>,
    /// ISO 8601 timestamp.
    pub timestamp: String,
}

/// Payload for the `SpeechEnded` message (VAD detected end-of-speech).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SpeechEndedPayload {
    /// Message type discriminator — always `"SpeechEnded"`.
    pub r#type: String,
    /// Session this turn belongs to.
    #[serde(rename = "sessionId")]
    pub session_id: String,
    /// The stream that carried this speech.
    #[serde(rename = "streamId")]
    pub stream_id: String,
    /// ISO 8601 timestamp.
    pub timestamp: String,
}

/// Payload for the `TextMessage` message (text-only user turn).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TextMessagePayload {
    /// Message type discriminator — always `"TextMessage"`.
    pub r#type: String,
    /// Session this message belongs to.
    #[serde(rename = "sessionId")]
    pub session_id: String,
    /// The text content.
    pub text: String,
    /// ISO 8601 timestamp.
    pub timestamp: String,
}

// ── M1 Server-to-Client messages ───────────────────────────────

/// Payload for the `SessionOpened` message from the Server.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SessionOpenedPayload {
    /// Message type discriminator — always `"SessionOpened"`.
    pub r#type: String,
    /// Newly assigned session identifier.
    #[serde(rename = "sessionId")]
    pub session_id: String,
    /// Identified user, or `"__guest__"` for unidentified speakers.
    #[serde(rename = "userId")]
    pub user_id: String,
    /// ISO 8601 timestamp.
    pub timestamp: String,
}

/// Payload for the `SessionClosed` message from the Server.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SessionClosedPayload {
    /// Message type discriminator — always `"SessionClosed"`.
    pub r#type: String,
    /// Session being closed.
    #[serde(rename = "sessionId")]
    pub session_id: String,
    /// Close reason.
    pub reason: String,
    /// ISO 8601 timestamp.
    pub timestamp: String,
}

/// Payload for `TranscriptPartial` — streaming STT partial transcript.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TranscriptPartialPayload {
    /// Message type discriminator.
    pub r#type: String,
    /// Session this transcript belongs to.
    #[serde(rename = "sessionId")]
    pub session_id: String,
    /// Partial transcript text.
    pub text: String,
    /// Confidence score (0.0–1.0).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub confidence: Option<f64>,
    /// ISO 8601 timestamp.
    pub timestamp: String,
}

/// Payload for `TranscriptFinal` — finalized STT transcript.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TranscriptFinalPayload {
    /// Message type discriminator.
    pub r#type: String,
    /// Session this transcript belongs to.
    #[serde(rename = "sessionId")]
    pub session_id: String,
    /// Final transcript text.
    pub text: String,
    /// Confidence score (0.0–1.0).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub confidence: Option<f64>,
    /// ISO 8601 timestamp.
    pub timestamp: String,
}

/// Payload for `LlmTokenStream` — streaming LLM tokens.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LlmTokenStreamPayload {
    /// Message type discriminator.
    pub r#type: String,
    /// Session.
    #[serde(rename = "sessionId")]
    pub session_id: String,
    /// The streamed token.
    pub token: String,
    /// Whether this is the final token in the stream.
    pub done: bool,
    /// ISO 8601 timestamp.
    pub timestamp: String,
}

/// Payload for `AgentStateChange` — agent state transition.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AgentStateChangePayload {
    /// Message type discriminator.
    pub r#type: String,
    /// Session.
    #[serde(rename = "sessionId")]
    pub session_id: String,
    /// New agent state.
    pub state: String,
    /// Human-readable reason when thinking.
    #[serde(rename = "thinkingReason", skip_serializing_if = "Option::is_none")]
    pub thinking_reason: Option<String>,
    /// ISO 8601 timestamp.
    pub timestamp: String,
}

/// Payload for `ToolCallStarted` — agent called a tool.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ToolCallStartedPayload {
    /// Message type discriminator.
    pub r#type: String,
    /// Session.
    #[serde(rename = "sessionId")]
    pub session_id: String,
    /// Unique call identifier.
    #[serde(rename = "callId")]
    pub call_id: String,
    /// Tool identifier.
    #[serde(rename = "toolId")]
    pub tool_id: String,
    /// ISO 8601 timestamp.
    pub timestamp: String,
}

/// Payload for `ToolCallCompleted` — tool call finished.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ToolCallCompletedPayload {
    /// Message type discriminator.
    pub r#type: String,
    /// Session.
    #[serde(rename = "sessionId")]
    pub session_id: String,
    /// Unique call identifier.
    #[serde(rename = "callId")]
    pub call_id: String,
    /// Whether the call succeeded.
    pub success: bool,
    /// ISO 8601 timestamp.
    pub timestamp: String,
}

/// A generic incoming message used for type discrimination.
#[derive(Debug, Clone, Deserialize)]
pub struct IncomingMessage {
    /// The message type discriminator.
    pub r#type: String,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_device_identify_serialization() {
        let payload = DeviceIdentifyPayload {
            r#type: "DeviceIdentify".to_string(),
            device_id: "dev-001".to_string(),
            token: "test-token".to_string(),
            client_version: "0.1.0".to_string(),
            timestamp: "2025-01-01T00:00:00Z".to_string(),
        };
        let json = serde_json::to_string(&payload).unwrap();
        assert!(json.contains("\"type\":\"DeviceIdentify\""));
        assert!(json.contains("\"deviceId\":\"dev-001\""));
        assert!(json.contains("\"clientVersion\":\"0.1.0\""));
    }

    #[test]
    fn test_identify_ack_deserialization() {
        let json = r#"{"type":"IdentifyAck","accepted":true,"serverVersion":"1.0.0","timestamp":"2025-01-01T00:00:00Z"}"#;
        let ack: IdentifyAckPayload = serde_json::from_str(json).unwrap();
        assert!(ack.accepted);
        assert_eq!(ack.server_version, "1.0.0");
        assert!(ack.reason.is_none());
    }

    #[test]
    fn test_identify_ack_rejection() {
        let json = r#"{"type":"IdentifyAck","accepted":false,"serverVersion":"1.0.0","reason":"invalid token","timestamp":"2025-01-01T00:00:00Z"}"#;
        let ack: IdentifyAckPayload = serde_json::from_str(json).unwrap();
        assert!(!ack.accepted);
        assert_eq!(ack.reason.as_deref(), Some("invalid token"));
    }

    #[test]
    fn test_heartbeat_ping_serialization() {
        let ping = HeartbeatPingPayload {
            r#type: "HeartbeatPing".to_string(),
            timestamp: "2025-01-01T00:00:00Z".to_string(),
        };
        let json = serde_json::to_string(&ping).unwrap();
        assert!(json.contains("\"type\":\"HeartbeatPing\""));
    }

    #[test]
    fn test_heartbeat_pong_deserialization() {
        let json =
            r#"{"type":"HeartbeatPong","timestamp":"2025-01-01T00:00:00Z","latencyMs":12.5}"#;
        let pong: HeartbeatPongPayload = serde_json::from_str(json).unwrap();
        assert_eq!(pong.r#type, "HeartbeatPong");
        assert!((pong.latency_ms - 12.5).abs() < f64::EPSILON);
    }

    #[test]
    fn test_error_payload() {
        let json = r#"{"type":"Error","code":"auth.invalid_token","message":"Token expired"}"#;
        let err: ErrorPayload = serde_json::from_str(json).unwrap();
        assert_eq!(err.code, "auth.invalid_token");
        assert_eq!(err.message, "Token expired");
    }

    #[test]
    fn test_log_forward_payload() {
        let payload = LogForwardPayload {
            r#type: "LogForward".to_string(),
            source_device_id: Some("dev-001".to_string()),
            entries: vec![LogForwardEntry {
                timestamp: "2025-01-01T00:00:00Z".to_string(),
                level: "info".to_string(),
                subsystem: "ws".to_string(),
                message: "connected".to_string(),
            }],
            timestamp: "2025-01-01T00:00:00Z".to_string(),
        };
        let json = serde_json::to_string(&payload).unwrap();
        assert!(json.contains("\"type\":\"LogForward\""));
        assert!(json.contains("\"sourceDeviceId\":\"dev-001\""));
    }

    #[test]
    fn test_incoming_message_discrimination() {
        let json = r#"{"type":"HeartbeatPong","timestamp":"2025-01-01T00:00:00Z","latencyMs":10}"#;
        let msg: IncomingMessage = serde_json::from_str(json).unwrap();
        assert_eq!(msg.r#type, "HeartbeatPong");
    }

    // ── M1 message tests ──────────────────────────────────────

    #[test]
    fn test_wake_word_detected_serialization() {
        let payload = WakeWordDetectedPayload {
            r#type: "WakeWordDetected".to_string(),
            confidence: 0.95,
            snr_db: Some(25.0),
            timestamp: "2025-01-01T00:00:00Z".to_string(),
        };
        let json = serde_json::to_string(&payload).unwrap();
        assert!(json.contains("\"type\":\"WakeWordDetected\""));
        assert!(json.contains("\"confidence\":0.95"));
        assert!(json.contains("\"snrDb\":25.0"));
    }

    #[test]
    fn test_audio_stream_start_serialization() {
        let payload = AudioStreamStartPayload {
            r#type: "AudioStreamStart".to_string(),
            session_id: "sess-1".to_string(),
            stream_id: "stream-1".to_string(),
            direction: "in".to_string(),
            codec: "opus".to_string(),
            sample_rate: 16000,
            channels: 1,
            timestamp: Some("2025-01-01T00:00:00Z".to_string()),
        };
        let json = serde_json::to_string(&payload).unwrap();
        assert!(json.contains("\"streamId\":\"stream-1\""));
        assert!(json.contains("\"sampleRate\":16000"));
    }

    #[test]
    fn test_audio_stream_end_serialization() {
        let payload = AudioStreamEndPayload {
            r#type: "AudioStreamEnd".to_string(),
            session_id: "sess-1".to_string(),
            stream_id: "stream-1".to_string(),
            reason: Some("normal".to_string()),
            timestamp: "2025-01-01T00:00:00Z".to_string(),
        };
        let json = serde_json::to_string(&payload).unwrap();
        assert!(json.contains("\"type\":\"AudioStreamEnd\""));
    }

    #[test]
    fn test_speech_ended_serialization() {
        let payload = SpeechEndedPayload {
            r#type: "SpeechEnded".to_string(),
            session_id: "sess-1".to_string(),
            stream_id: "stream-1".to_string(),
            timestamp: "2025-01-01T00:00:00Z".to_string(),
        };
        let json = serde_json::to_string(&payload).unwrap();
        assert!(json.contains("\"type\":\"SpeechEnded\""));
        assert!(json.contains("\"streamId\":\"stream-1\""));
    }

    #[test]
    fn test_text_message_serialization() {
        let payload = TextMessagePayload {
            r#type: "TextMessage".to_string(),
            session_id: "sess-1".to_string(),
            text: "What's the weather?".to_string(),
            timestamp: "2025-01-01T00:00:00Z".to_string(),
        };
        let json = serde_json::to_string(&payload).unwrap();
        assert!(json.contains("\"text\":\"What's the weather?\""));
    }

    #[test]
    fn test_session_opened_deserialization() {
        let json = r#"{"type":"SessionOpened","sessionId":"sess-1","userId":"owner","timestamp":"2025-01-01T00:00:00Z"}"#;
        let msg: SessionOpenedPayload = serde_json::from_str(json).unwrap();
        assert_eq!(msg.session_id, "sess-1");
        assert_eq!(msg.user_id, "owner");
    }

    #[test]
    fn test_session_closed_deserialization() {
        let json = r#"{"type":"SessionClosed","sessionId":"sess-1","reason":"normal","timestamp":"2025-01-01T00:00:00Z"}"#;
        let msg: SessionClosedPayload = serde_json::from_str(json).unwrap();
        assert_eq!(msg.reason, "normal");
    }

    #[test]
    fn test_transcript_partial_deserialization() {
        let json = r#"{"type":"TranscriptPartial","sessionId":"s1","text":"what's the","confidence":0.8,"timestamp":"2025-01-01T00:00:00Z"}"#;
        let msg: TranscriptPartialPayload = serde_json::from_str(json).unwrap();
        assert_eq!(msg.text, "what's the");
        assert_eq!(msg.confidence, Some(0.8));
    }

    #[test]
    fn test_transcript_final_deserialization() {
        let json = r#"{"type":"TranscriptFinal","sessionId":"s1","text":"what's the weather","timestamp":"2025-01-01T00:00:00Z"}"#;
        let msg: TranscriptFinalPayload = serde_json::from_str(json).unwrap();
        assert_eq!(msg.text, "what's the weather");
        assert!(msg.confidence.is_none());
    }

    #[test]
    fn test_agent_state_change_deserialization() {
        let json = r#"{"type":"AgentStateChange","sessionId":"s1","state":"thinking","thinkingReason":"calling weather tool","timestamp":"2025-01-01T00:00:00Z"}"#;
        let msg: AgentStateChangePayload = serde_json::from_str(json).unwrap();
        assert_eq!(msg.state, "thinking");
        assert_eq!(msg.thinking_reason.as_deref(), Some("calling weather tool"));
    }

    #[test]
    fn test_llm_token_stream_deserialization() {
        let json = r#"{"type":"LlmTokenStream","sessionId":"s1","token":"Hello","done":false,"timestamp":"2025-01-01T00:00:00Z"}"#;
        let msg: LlmTokenStreamPayload = serde_json::from_str(json).unwrap();
        assert_eq!(msg.token, "Hello");
        assert!(!msg.done);
    }

    #[test]
    fn test_tool_call_started_deserialization() {
        let json = r#"{"type":"ToolCallStarted","sessionId":"s1","callId":"c1","toolId":"weather","timestamp":"2025-01-01T00:00:00Z"}"#;
        let msg: ToolCallStartedPayload = serde_json::from_str(json).unwrap();
        assert_eq!(msg.tool_id, "weather");
    }

    #[test]
    fn test_tool_call_completed_deserialization() {
        let json = r#"{"type":"ToolCallCompleted","sessionId":"s1","callId":"c1","success":true,"timestamp":"2025-01-01T00:00:00Z"}"#;
        let msg: ToolCallCompletedPayload = serde_json::from_str(json).unwrap();
        assert!(msg.success);
    }

    #[test]
    fn test_audio_stream_start_deserialization_from_server() {
        let json = r#"{"type":"AudioStreamStart","sessionId":"s1","streamId":"out-1","direction":"out","codec":"opus","sampleRate":24000,"channels":1}"#;
        let msg: AudioStreamStartPayload = serde_json::from_str(json).unwrap();
        assert_eq!(msg.direction, "out");
        assert_eq!(msg.sample_rate, 24000);
    }
}
