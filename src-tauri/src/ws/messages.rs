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
}
