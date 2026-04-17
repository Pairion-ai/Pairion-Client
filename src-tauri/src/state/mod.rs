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
}

impl AppState {
    /// Creates a new application state with the given device id and server URL.
    pub fn new(device_id: String, server_url: String) -> Self {
        let (tx, rx) = watch::channel(ConnectionState::default());
        Self {
            connection_tx: Arc::new(tx),
            connection_rx: rx,
            device_id,
            server_url,
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
