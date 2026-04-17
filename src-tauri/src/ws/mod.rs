//! WebSocket client for the Pairion Client.
//!
//! Manages a single persistent WebSocket connection to the Pairion Server.
//! Handles the full connection lifecycle: connect, authenticate via
//! `DeviceIdentify`/`IdentifyAck`, maintain heartbeats, and reconnect
//! with exponential backoff on failure.
//!
//! Connection state is exposed via a [`tokio::sync::watch`] channel that
//! the Tauri command layer bridges to the React frontend.

mod messages;

pub use messages::*;

use crate::config::AppConfig;
use crate::state::ConnectionState;
use futures_util::{SinkExt, StreamExt};
use std::sync::Arc;
use std::time::Duration;
use tokio::sync::watch;
use tokio_tungstenite::tungstenite::Message;
use tracing::{error, info, warn};

/// Runs the WebSocket client loop, managing connections and reconnections.
///
/// This function runs indefinitely, reconnecting on failure with exponential
/// backoff. It updates the connection state via the provided watch channel
/// sender. The `token` is used for authentication in `DeviceIdentify`.
pub async fn run_ws_client(
    config: AppConfig,
    device_id: String,
    token: String,
    connection_tx: Arc<watch::Sender<ConnectionState>>,
) {
    let mut attempt: u32 = 0;

    loop {
        let _ = connection_tx.send(if attempt == 0 {
            ConnectionState::Connecting
        } else {
            ConnectionState::Reconnecting { attempt }
        });

        match connect_and_run(&config, &device_id, &token, &connection_tx).await {
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
                        if let Ok(pong) = serde_json::from_str::<HeartbeatPongPayload>(&text) {
                            if pong.r#type == "HeartbeatPong" {
                                last_pong = tokio::time::Instant::now();
                                let _ = connection_tx.send(ConnectionState::Connected {
                                    latency_ms: Some(pong.latency_ms),
                                });
                            }
                        }
                        // Other message types are ignored in M0
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
                    _ => {} // Binary, Ping, Pong frames handled by tungstenite
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

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
}
