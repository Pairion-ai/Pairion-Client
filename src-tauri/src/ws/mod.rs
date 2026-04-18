//! WebSocket client for the Pairion Client.
//!
//! Manages a single persistent WebSocket connection to the Pairion Server.
//! Handles the full connection lifecycle: connect, authenticate via
//! `DeviceIdentify`/`IdentifyAck`, maintain heartbeats, and reconnect
//! with exponential backoff on failure.
//!
//! Connection state is exposed via a [`tokio::sync::watch`] channel that
//! the Tauri command layer bridges to the React frontend.

pub mod channel;
mod messages;

pub use channel::{create_outbound_channel, OutboundMessage, OutboundReceiver, OutboundSender};
pub use messages::*;

use crate::config::AppConfig;
use crate::pipeline::{OrchestratorCommand, OrchestratorHandle};
use crate::state::{AgentState, ConnectionState, SessionState};
use futures_util::{SinkExt, StreamExt};
use std::sync::Arc;
use std::time::Duration;
use tokio::sync::watch;
use tokio_tungstenite::tungstenite::Message;
use tracing::{debug, error, info, warn};

/// Runs the WebSocket client loop, managing connections and reconnections.
///
/// This function runs indefinitely, reconnecting on failure with exponential
/// backoff. It updates both connection and session state via the provided
/// watch channel senders. The `token` is used for authentication in `DeviceIdentify`.
/// The `outbound_rx` receives messages from other tasks (audio capture,
/// Tauri commands) for sending over the WebSocket.
pub async fn run_ws_client(
    config: AppConfig,
    device_id: String,
    token: String,
    connection_tx: Arc<watch::Sender<ConnectionState>>,
    session_tx: Arc<watch::Sender<SessionState>>,
    mut outbound_rx: OutboundReceiver,
    orchestrator: OrchestratorHandle,
) {
    let mut attempt: u32 = 0;

    loop {
        let _ = connection_tx.send(if attempt == 0 {
            ConnectionState::Connecting
        } else {
            ConnectionState::Reconnecting { attempt }
        });

        match connect_and_run(
            &config,
            &device_id,
            &token,
            &connection_tx,
            &session_tx,
            &mut outbound_rx,
            &orchestrator,
        )
        .await
        {
            Ok(()) => {
                info!("WebSocket connection closed normally");
            }
            Err(e) => {
                warn!(error = %e, attempt = attempt, "WebSocket connection failed");
            }
        }

        attempt += 1;
        let backoff = calculate_backoff(attempt, config.max_backoff_secs);
        let _ = connection_tx.send(ConnectionState::Reconnecting { attempt });

        info!(
            backoff_secs = backoff.as_secs(),
            attempt = attempt,
            "Reconnecting after backoff"
        );
        tokio::time::sleep(backoff).await;
    }
}

/// Calculates exponential backoff duration for the given attempt number.
pub fn calculate_backoff(attempt: u32, max_secs: u64) -> Duration {
    let secs = (1u64 << attempt.min(5)).min(max_secs);
    Duration::from_secs(secs)
}

/// Connects to the server and runs the message loop until disconnection.
async fn connect_and_run(
    config: &AppConfig,
    device_id: &str,
    token: &str,
    connection_tx: &Arc<watch::Sender<ConnectionState>>,
    session_tx: &Arc<watch::Sender<SessionState>>,
    outbound_rx: &mut OutboundReceiver,
    orchestrator: &OrchestratorHandle,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let (ws_stream, _) = tokio_tungstenite::connect_async(&config.ws_url).await?;
    let (mut write, mut read) = ws_stream.split();

    info!(url = %config.ws_url, "WebSocket connected, sending DeviceIdentify");

    // Send DeviceIdentify
    let identify = DeviceIdentifyPayload {
        r#type: "DeviceIdentify".to_string(),
        device_id: device_id.to_string(),
        token: token.to_string(),
        client_version: env!("CARGO_PKG_VERSION").to_string(),
        timestamp: chrono::Utc::now().to_rfc3339(),
    };
    let identify_json = serde_json::to_string(&identify)?;
    write.send(Message::Text(identify_json.into())).await?;

    // Wait for IdentifyAck
    let ack_msg = tokio::time::timeout(Duration::from_secs(10), read.next())
        .await
        .map_err(|_| "Timeout waiting for IdentifyAck")?
        .ok_or("Connection closed before IdentifyAck")?
        .map_err(|e| format!("WS error: {e}"))?;

    let ack: IdentifyAckPayload = match ack_msg {
        Message::Text(text) => serde_json::from_str(&text)?,
        _ => return Err("Expected text message for IdentifyAck".into()),
    };

    if !ack.accepted {
        let reason = ack.reason.unwrap_or_else(|| "unknown".to_string());
        error!(reason = %reason, "Server rejected identification");
        return Err(format!("Server rejected: {reason}").into());
    }

    info!(server_version = %ack.server_version, "Identified successfully");
    let _ = connection_tx.send(ConnectionState::Connected { latency_ms: None });

    // Heartbeat + message loop
    let heartbeat_interval = Duration::from_secs(config.heartbeat_interval_secs);
    let heartbeat_timeout = Duration::from_secs(config.heartbeat_timeout_secs);
    let mut heartbeat_timer = tokio::time::interval(heartbeat_interval);
    let mut last_pong = tokio::time::Instant::now();

    loop {
        tokio::select! {
            _ = heartbeat_timer.tick() => {
                // Check pong timeout
                if last_pong.elapsed() > heartbeat_timeout {
                    warn!("Heartbeat timeout — no pong received");
                    return Err("Heartbeat timeout".into());
                }

                let ping = HeartbeatPingPayload {
                    r#type: "HeartbeatPing".to_string(),
                    timestamp: chrono::Utc::now().to_rfc3339(),
                };
                let ping_json = serde_json::to_string(&ping)?;
                write.send(Message::Text(ping_json.into())).await?;
            }
            msg = read.next() => {
                match msg {
                    Some(Ok(Message::Text(text))) => {
                        handle_text_message(&text, connection_tx, session_tx, &mut last_pong, orchestrator);
                    }
                    Some(Ok(Message::Binary(data))) => {
                        // Route AudioChunkOut binary frames to the orchestrator
                        debug!(bytes = data.len(), "Routing binary audio to orchestrator");
                        let _ = orchestrator.try_send(OrchestratorCommand::InboundAudio {
                            stream_id: String::new(), // Stream ID from last AudioStreamStart
                            opus_frame: data.to_vec(),
                        });
                    }
                    Some(Ok(Message::Close(_))) => {
                        info!("Server closed connection");
                        return Ok(());
                    }
                    Some(Err(e)) => {
                        error!(error = %e, "WebSocket error");
                        return Err(e.into());
                    }
                    None => {
                        info!("WebSocket stream ended");
                        return Ok(());
                    }
                    _ => {} // Ping, Pong frames handled by tungstenite
                }
            }
            outbound = outbound_rx.recv() => {
                match outbound {
                    Some(OutboundMessage::Json(text)) => {
                        debug!("Sending outbound JSON message");
                        write.send(Message::Text(text.into())).await?;
                    }
                    Some(OutboundMessage::Binary(data)) => {
                        debug!(bytes = data.len(), "Sending outbound binary frame");
                        write.send(Message::Binary(data.into())).await?;
                    }
                    None => {
                        // Channel closed — all senders dropped
                        warn!("Outbound channel closed");
                    }
                }
            }
        }
    }
}

/// Dispatches an incoming JSON text message based on its `type` field.
///
/// Updates connection and session state as appropriate. Unknown message
/// types are logged at debug level and ignored.
fn handle_text_message(
    text: &str,
    connection_tx: &Arc<watch::Sender<ConnectionState>>,
    session_tx: &Arc<watch::Sender<SessionState>>,
    last_pong: &mut tokio::time::Instant,
    orchestrator: &OrchestratorHandle,
) {
    // First try to read the type discriminator
    let Ok(incoming) = serde_json::from_str::<IncomingMessage>(text) else {
        warn!("Received unparseable WS message");
        return;
    };

    match incoming.r#type.as_str() {
        "HeartbeatPong" => {
            if let Ok(pong) = serde_json::from_str::<HeartbeatPongPayload>(text) {
                *last_pong = tokio::time::Instant::now();
                let _ = connection_tx.send(ConnectionState::Connected {
                    latency_ms: Some(pong.latency_ms),
                });
            }
        }
        "SessionOpened" => {
            if let Ok(msg) = serde_json::from_str::<SessionOpenedPayload>(text) {
                info!(session_id = %msg.session_id, user_id = %msg.user_id, "Session opened");
                let _ = session_tx.send(SessionState {
                    session_id: Some(msg.session_id),
                    agent_state: AgentState::Listening,
                    active: true,
                    ..Default::default()
                });
            }
        }
        "SessionClosed" => {
            if let Ok(msg) = serde_json::from_str::<SessionClosedPayload>(text) {
                info!(session_id = %msg.session_id, reason = %msg.reason, "Session closed");
                let _ = session_tx.send(SessionState::default());
            }
        }
        "AgentStateChange" => {
            if let Ok(msg) = serde_json::from_str::<AgentStateChangePayload>(text) {
                let new_state = AgentState::from_str_lossy(&msg.state);
                debug!(state = %msg.state, "Agent state changed");
                session_tx.send_modify(|s| {
                    s.agent_state = new_state;
                });
            }
        }
        "TranscriptPartial" => {
            if let Ok(msg) = serde_json::from_str::<TranscriptPartialPayload>(text) {
                debug!(text = %msg.text, "Partial transcript");
                session_tx.send_modify(|s| {
                    s.partial_transcript = msg.text;
                });
            }
        }
        "TranscriptFinal" => {
            if let Ok(msg) = serde_json::from_str::<TranscriptFinalPayload>(text) {
                info!(text = %msg.text, "Final transcript");
                session_tx.send_modify(|s| {
                    s.final_transcript = msg.text;
                    s.partial_transcript.clear();
                });
            }
        }
        "AudioStreamStart" => {
            if let Ok(msg) = serde_json::from_str::<AudioStreamStartPayload>(text) {
                if msg.direction == "out" {
                    debug!(
                        stream_id = %msg.stream_id,
                        codec = %msg.codec,
                        sample_rate = msg.sample_rate,
                        "Server started outbound audio stream"
                    );
                }
            }
        }
        "AudioStreamEnd" => {
            if let Ok(msg) = serde_json::from_str::<AudioStreamEndPayload>(text) {
                let reason = msg.reason.clone().unwrap_or_else(|| "normal".to_string());
                debug!(
                    stream_id = %msg.stream_id,
                    reason = %reason,
                    "Audio stream ended — routing to orchestrator"
                );
                let _ = orchestrator.try_send(OrchestratorCommand::InboundStreamEnd {
                    stream_id: msg.stream_id,
                    reason,
                });
            }
        }
        "LlmTokenStream" | "ToolCallStarted" | "ToolCallCompleted" => {
            debug!(msg_type = %incoming.r#type, "Received agent trace message");
        }
        "Error" => {
            if let Ok(err) = serde_json::from_str::<ErrorPayload>(text) {
                warn!(code = %err.code, message = %err.message, "Server error");
            }
        }
        other => {
            debug!(msg_type = %other, "Received unhandled message type");
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn make_test_orchestrator() -> OrchestratorHandle {
        let (handle, _rx) = crate::pipeline::create_orchestrator_channel();
        handle
    }

    #[test]
    fn test_calculate_backoff() {
        assert_eq!(calculate_backoff(1, 30), Duration::from_secs(2));
        assert_eq!(calculate_backoff(2, 30), Duration::from_secs(4));
        assert_eq!(calculate_backoff(3, 30), Duration::from_secs(8));
        assert_eq!(calculate_backoff(4, 30), Duration::from_secs(16));
        assert_eq!(calculate_backoff(5, 30), Duration::from_secs(30));
        assert_eq!(calculate_backoff(6, 30), Duration::from_secs(30));
        assert_eq!(calculate_backoff(100, 30), Duration::from_secs(30));
    }

    #[test]
    fn test_calculate_backoff_custom_max() {
        assert_eq!(calculate_backoff(5, 10), Duration::from_secs(10));
    }

    #[test]
    fn test_handle_heartbeat_pong() {
        let (conn_tx, conn_rx) = watch::channel(ConnectionState::default());
        let (sess_tx, _sess_rx) = watch::channel(SessionState::default());
        let conn_tx = Arc::new(conn_tx);
        let sess_tx = Arc::new(sess_tx);
        let mut last_pong = tokio::time::Instant::now();

        let msg = r#"{"type":"HeartbeatPong","timestamp":"2025-01-01T00:00:00Z","latencyMs":15.0}"#;
        let orch = make_test_orchestrator();
        handle_text_message(msg, &conn_tx, &sess_tx, &mut last_pong, &orch);

        let state = conn_rx.borrow().clone();
        match state {
            ConnectionState::Connected { latency_ms } => {
                assert_eq!(latency_ms, Some(15.0));
            }
            other => panic!("Expected Connected, got {other:?}"),
        }
    }

    #[test]
    fn test_handle_session_opened() {
        let (conn_tx, _) = watch::channel(ConnectionState::default());
        let (sess_tx, sess_rx) = watch::channel(SessionState::default());
        let conn_tx = Arc::new(conn_tx);
        let sess_tx = Arc::new(sess_tx);
        let mut last_pong = tokio::time::Instant::now();

        let msg = r#"{"type":"SessionOpened","sessionId":"sess-1","userId":"owner","timestamp":"2025-01-01T00:00:00Z"}"#;
        let orch = make_test_orchestrator();
        handle_text_message(msg, &conn_tx, &sess_tx, &mut last_pong, &orch);

        let session = sess_rx.borrow();
        assert_eq!(session.session_id.as_deref(), Some("sess-1"));
        assert!(session.active);
        assert_eq!(session.agent_state, AgentState::Listening);
    }

    #[test]
    fn test_handle_session_closed() {
        let (conn_tx, _) = watch::channel(ConnectionState::default());
        let (sess_tx, sess_rx) = watch::channel(SessionState {
            session_id: Some("sess-1".to_string()),
            active: true,
            ..Default::default()
        });
        let conn_tx = Arc::new(conn_tx);
        let sess_tx = Arc::new(sess_tx);
        let mut last_pong = tokio::time::Instant::now();

        let msg = r#"{"type":"SessionClosed","sessionId":"sess-1","reason":"normal","timestamp":"2025-01-01T00:00:00Z"}"#;
        let orch = make_test_orchestrator();
        handle_text_message(msg, &conn_tx, &sess_tx, &mut last_pong, &orch);

        let session = sess_rx.borrow();
        assert!(session.session_id.is_none());
        assert!(!session.active);
    }

    #[test]
    fn test_handle_agent_state_change() {
        let (conn_tx, _) = watch::channel(ConnectionState::default());
        let (sess_tx, sess_rx) = watch::channel(SessionState {
            session_id: Some("s1".to_string()),
            active: true,
            ..Default::default()
        });
        let conn_tx = Arc::new(conn_tx);
        let sess_tx = Arc::new(sess_tx);
        let mut last_pong = tokio::time::Instant::now();

        let msg = r#"{"type":"AgentStateChange","sessionId":"s1","state":"thinking","timestamp":"2025-01-01T00:00:00Z"}"#;
        let orch = make_test_orchestrator();
        handle_text_message(msg, &conn_tx, &sess_tx, &mut last_pong, &orch);

        assert_eq!(sess_rx.borrow().agent_state, AgentState::Thinking);
    }

    #[test]
    fn test_handle_transcript_partial() {
        let (conn_tx, _) = watch::channel(ConnectionState::default());
        let (sess_tx, sess_rx) = watch::channel(SessionState::default());
        let conn_tx = Arc::new(conn_tx);
        let sess_tx = Arc::new(sess_tx);
        let mut last_pong = tokio::time::Instant::now();

        let msg = r#"{"type":"TranscriptPartial","sessionId":"s1","text":"what's the","timestamp":"2025-01-01T00:00:00Z"}"#;
        let orch = make_test_orchestrator();
        handle_text_message(msg, &conn_tx, &sess_tx, &mut last_pong, &orch);

        assert_eq!(sess_rx.borrow().partial_transcript, "what's the");
    }

    #[test]
    fn test_handle_transcript_final() {
        let (conn_tx, _) = watch::channel(ConnectionState::default());
        let (sess_tx, sess_rx) = watch::channel(SessionState {
            partial_transcript: "what's the wea".to_string(),
            ..Default::default()
        });
        let conn_tx = Arc::new(conn_tx);
        let sess_tx = Arc::new(sess_tx);
        let mut last_pong = tokio::time::Instant::now();

        let msg = r#"{"type":"TranscriptFinal","sessionId":"s1","text":"what's the weather","timestamp":"2025-01-01T00:00:00Z"}"#;
        let orch = make_test_orchestrator();
        handle_text_message(msg, &conn_tx, &sess_tx, &mut last_pong, &orch);

        let session = sess_rx.borrow();
        assert_eq!(session.final_transcript, "what's the weather");
        assert!(session.partial_transcript.is_empty());
    }

    #[test]
    fn test_handle_unknown_message() {
        let (conn_tx, _) = watch::channel(ConnectionState::default());
        let (sess_tx, _) = watch::channel(SessionState::default());
        let conn_tx = Arc::new(conn_tx);
        let sess_tx = Arc::new(sess_tx);
        let mut last_pong = tokio::time::Instant::now();

        // Should not panic on unknown types
        let msg = r#"{"type":"SomeUnknownType","data":"test"}"#;
        let orch = make_test_orchestrator();
        handle_text_message(msg, &conn_tx, &sess_tx, &mut last_pong, &orch);
    }

    #[test]
    fn test_handle_invalid_json() {
        let (conn_tx, _) = watch::channel(ConnectionState::default());
        let (sess_tx, _) = watch::channel(SessionState::default());
        let conn_tx = Arc::new(conn_tx);
        let sess_tx = Arc::new(sess_tx);
        let mut last_pong = tokio::time::Instant::now();

        // Should not panic on invalid JSON
        let orch = make_test_orchestrator();
        handle_text_message("not json", &conn_tx, &sess_tx, &mut last_pong, &orch);
    }
}
