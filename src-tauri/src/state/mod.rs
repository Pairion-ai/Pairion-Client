//! Shared application state for the Pairion Client.
//!
//! Provides thread-safe access to connection state and device identity
//! via [`tokio::sync::watch`] channels. The WebSocket client updates
//! these; Tauri commands read them to serve React frontend queries.

use serde::{Deserialize, Serialize};
use std::sync::Arc;
use tokio::sync::watch;

/// Connection state as exposed to the React frontend.
#[derive(Debug, Clone, Default, Serialize, Deserialize, PartialEq)]
#[serde(tag = "status")]
pub enum ConnectionState {
    /// Initial state before first connection attempt.
    #[default]
    #[serde(rename = "connecting")]
    Connecting,
    /// WebSocket is open and IdentifyAck received.
    #[serde(rename = "connected")]
    Connected {
        /// Last heartbeat latency in milliseconds.
        latency_ms: Option<f64>,
    },
    /// WebSocket closed or never opened, not retrying.
    #[serde(rename = "disconnected")]
    Disconnected {
        /// Reason for disconnection, if available.
        reason: Option<String>,
    },
    /// Attempting to reconnect after a failure.
    #[serde(rename = "reconnecting")]
    Reconnecting {
        /// Current retry attempt number (1-based).
        attempt: u32,
    },
}

impl ConnectionState {
    /// Returns a human-readable label for the current state.
    pub fn label(&self) -> String {
        match self {
            Self::Connecting => "Connecting\u{2026}".to_string(),
            Self::Connected { .. } => "Connected".to_string(),
            Self::Disconnected { .. } => "Disconnected".to_string(),
            Self::Reconnecting { attempt } => {
                format!("Reconnecting\u{2026} (attempt {attempt})")
            }
        }
    }
}

/// Agent state as reported by the Server via `AgentStateChange` messages.
#[derive(Debug, Clone, Default, Serialize, Deserialize, PartialEq, Eq)]
pub enum AgentState {
    /// No active session; waiting for wake word or user input.
    #[default]
    Idle,
    /// Wake word fired; capturing user speech.
    Listening,
    /// Server is processing (STT, LLM, tool calls).
    Thinking,
    /// TTS audio is playing.
    Speaking,
    /// Identifying speaker (M5+).
    Identifying,
    /// Handing off to another device/node (M6+).
    Handoff,
}

impl AgentState {
    /// Parses an agent state string from the Server's `AgentStateChange` message.
    pub fn from_str_lossy(s: &str) -> Self {
        match s {
            "idle" => Self::Idle,
            "listening" => Self::Listening,
            "thinking" => Self::Thinking,
            "speaking" => Self::Speaking,
            "identifying" => Self::Identifying,
            "handoff" => Self::Handoff,
            _ => Self::Idle,
        }
    }

    /// Returns a human-readable label for the current state.
    pub fn label(&self) -> &'static str {
        match self {
            Self::Idle => "Idle",
            Self::Listening => "Listening",
            Self::Thinking => "Thinking",
            Self::Speaking => "Speaking",
            Self::Identifying => "Identifying",
            Self::Handoff => "Handoff",
        }
    }
}

/// Session state tracking the current voice session.
#[derive(Debug, Clone, Default, Serialize, Deserialize, PartialEq)]
pub struct SessionState {
    /// Current session ID, if any.
    pub session_id: Option<String>,
    /// Current agent state.
    pub agent_state: AgentState,
    /// Latest partial transcript from STT.
    pub partial_transcript: String,
    /// Latest finalized transcript.
    pub final_transcript: String,
    /// Whether a session is currently active.
    pub active: bool,
    /// The audio stream ID for the current capture, if any.
    pub current_stream_id: Option<String>,
}

/// Shared application state accessible across Tauri commands and the WS client.
#[derive(Debug, Clone)]
pub struct AppState {
    /// Sender half of the connection state watch channel.
    pub connection_tx: Arc<watch::Sender<ConnectionState>>,
    /// Receiver half of the connection state watch channel.
    pub connection_rx: watch::Receiver<ConnectionState>,
    /// The unique device identifier for this Client instance.
    pub device_id: String,
    /// The WebSocket server URL.
    pub server_url: String,
    /// Sender half of the session state watch channel.
    pub session_tx: Arc<watch::Sender<SessionState>>,
    /// Receiver half of the session state watch channel.
    pub session_rx: watch::Receiver<SessionState>,
}

impl AppState {
    /// Creates a new application state with the given device id and server URL.
    pub fn new(device_id: String, server_url: String) -> Self {
        let (conn_tx, conn_rx) = watch::channel(ConnectionState::default());
        let (sess_tx, sess_rx) = watch::channel(SessionState::default());
        Self {
            connection_tx: Arc::new(conn_tx),
            connection_rx: conn_rx,
            device_id,
            server_url,
            session_tx: Arc::new(sess_tx),
            session_rx: sess_rx,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_connection_state_default() {
        let state = ConnectionState::default();
        assert_eq!(state, ConnectionState::Connecting);
    }

    #[test]
    fn test_connection_state_labels() {
        assert_eq!(ConnectionState::Connecting.label(), "Connecting\u{2026}");
        assert_eq!(
            ConnectionState::Connected {
                latency_ms: Some(12.5)
            }
            .label(),
            "Connected"
        );
        assert_eq!(
            ConnectionState::Disconnected { reason: None }.label(),
            "Disconnected"
        );
        assert_eq!(
            ConnectionState::Reconnecting { attempt: 3 }.label(),
            "Reconnecting\u{2026} (attempt 3)"
        );
    }

    #[test]
    fn test_connection_state_serialization() {
        let state = ConnectionState::Connected {
            latency_ms: Some(15.2),
        };
        let json = serde_json::to_string(&state).expect("serialize");
        assert!(json.contains("\"status\":\"connected\""));
        assert!(json.contains("\"latency_ms\":15.2"));

        let reconnecting = ConnectionState::Reconnecting { attempt: 2 };
        let json = serde_json::to_string(&reconnecting).expect("serialize");
        assert!(json.contains("\"status\":\"reconnecting\""));
        assert!(json.contains("\"attempt\":2"));
    }

    #[test]
    fn test_app_state_creation() {
        let state = AppState::new(
            "dev-001".to_string(),
            "ws://localhost:18789/ws/v1".to_string(),
        );
        assert_eq!(state.device_id, "dev-001");
        assert_eq!(state.server_url, "ws://localhost:18789/ws/v1");
        assert_eq!(*state.connection_rx.borrow(), ConnectionState::Connecting);
    }

    #[test]
    fn test_agent_state_from_str_lossy() {
        assert_eq!(AgentState::from_str_lossy("idle"), AgentState::Idle);
        assert_eq!(
            AgentState::from_str_lossy("listening"),
            AgentState::Listening
        );
        assert_eq!(AgentState::from_str_lossy("thinking"), AgentState::Thinking);
        assert_eq!(AgentState::from_str_lossy("speaking"), AgentState::Speaking);
        assert_eq!(
            AgentState::from_str_lossy("identifying"),
            AgentState::Identifying
        );
        assert_eq!(AgentState::from_str_lossy("handoff"), AgentState::Handoff);
        assert_eq!(AgentState::from_str_lossy("unknown"), AgentState::Idle);
    }

    #[test]
    fn test_agent_state_labels() {
        assert_eq!(AgentState::Idle.label(), "Idle");
        assert_eq!(AgentState::Listening.label(), "Listening");
        assert_eq!(AgentState::Thinking.label(), "Thinking");
        assert_eq!(AgentState::Speaking.label(), "Speaking");
    }

    #[test]
    fn test_session_state_default() {
        let state = SessionState::default();
        assert!(state.session_id.is_none());
        assert_eq!(state.agent_state, AgentState::Idle);
        assert!(state.partial_transcript.is_empty());
        assert!(state.final_transcript.is_empty());
        assert!(!state.active);
    }

    #[test]
    fn test_session_state_watch_channel() {
        let state = AppState::new(
            "dev-001".to_string(),
            "ws://localhost:18789/ws/v1".to_string(),
        );
        let new_session = SessionState {
            session_id: Some("sess-1".to_string()),
            agent_state: AgentState::Listening,
            active: true,
            ..Default::default()
        };
        let _ = state.session_tx.send(new_session.clone());
        assert_eq!(*state.session_rx.borrow(), new_session);
    }

    #[test]
    fn test_app_state_update() {
        let state = AppState::new(
            "dev-001".to_string(),
            "ws://localhost:18789/ws/v1".to_string(),
        );
        let _ = state.connection_tx.send(ConnectionState::Connected {
            latency_ms: Some(10.0),
        });
        assert_eq!(
            *state.connection_rx.borrow(),
            ConnectionState::Connected {
                latency_ms: Some(10.0)
            }
        );
    }
}
