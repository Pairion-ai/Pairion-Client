//! Mock WebSocket server fixture for Client integration tests.
//!
//! Provides a lightweight in-process WebSocket server that implements
//! just enough of the AsyncAPI protocol to let Client tests run without
//! a real Pairion-Server. Supports `DeviceIdentify`/`IdentifyAck`,
//! `HeartbeatPing`/`HeartbeatPong`, and returns `Error` for unknown messages.

use futures_util::{SinkExt, StreamExt};
use std::net::SocketAddr;
use tokio::net::TcpListener;
use tokio_tungstenite::tungstenite::Message;

/// A mock Pairion Server for testing.
pub struct MockServer {
    /// The address the server is listening on.
    pub addr: SocketAddr,
    /// Handle to cancel the server task.
    cancel: tokio::sync::watch::Sender<bool>,
}

impl MockServer {
    /// Starts a mock server on a random available port.
    pub async fn start() -> Self {
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let addr = listener.local_addr().unwrap();
        let (cancel_tx, cancel_rx) = tokio::sync::watch::channel(false);

        tokio::spawn(async move {
            run_mock_server(listener, cancel_rx).await;
        });

        Self {
            addr,
            cancel: cancel_tx,
        }
    }

    /// Returns the WebSocket URL for this mock server.
    pub fn ws_url(&self) -> String {
        format!("ws://{}", self.addr)
    }

    /// Shuts down the mock server.
    pub fn shutdown(&self) {
        let _ = self.cancel.send(true);
    }
}

impl Drop for MockServer {
    fn drop(&mut self) {
        self.shutdown();
    }
}

async fn run_mock_server(listener: TcpListener, mut cancel: tokio::sync::watch::Receiver<bool>) {
    loop {
        tokio::select! {
            result = listener.accept() => {
                if let Ok((stream, _)) = result {
                    let cancel_rx = cancel.clone();
                    tokio::spawn(async move {
                        handle_connection(stream, cancel_rx).await;
                    });
                }
            }
            _ = cancel.changed() => {
                if *cancel.borrow() {
                    return;
                }
            }
        }
    }
}

async fn handle_connection(
    stream: tokio::net::TcpStream,
    mut cancel: tokio::sync::watch::Receiver<bool>,
) {
    let ws_stream = tokio_tungstenite::accept_async(stream).await;
    let Ok(ws_stream) = ws_stream else {
        return;
    };

    let (mut write, mut read) = ws_stream.split();

    loop {
        tokio::select! {
            msg = read.next() => {
                match msg {
                    Some(Ok(Message::Text(text))) => {
                        let response = handle_message(&text);
                        if write.send(Message::Text(response.into())).await.is_err() {
                            return;
                        }
                    }
                    Some(Ok(Message::Close(_))) | None => return,
                    _ => {}
                }
            }
            _ = cancel.changed() => {
                if *cancel.borrow() {
                    let _ = write.send(Message::Close(None)).await;
                    return;
                }
            }
        }
    }
}

fn handle_message(text: &str) -> String {
    let msg: serde_json::Value = match serde_json::from_str(text) {
        Ok(v) => v,
        Err(_) => {
            return serde_json::json!({
                "type": "Error",
                "code": "protocol.parse_error",
                "message": "Failed to parse message"
            })
            .to_string();
        }
    };

    let msg_type = msg.get("type").and_then(|v| v.as_str()).unwrap_or("");

    match msg_type {
        "DeviceIdentify" => serde_json::json!({
            "type": "IdentifyAck",
            "accepted": true,
            "serverVersion": "0.1.0-mock",
            "timestamp": chrono::Utc::now().to_rfc3339()
        })
        .to_string(),

        "HeartbeatPing" => {
            let client_ts = msg.get("timestamp").and_then(|v| v.as_str()).unwrap_or("");
            let now = chrono::Utc::now();
            let latency = if let Ok(sent) = chrono::DateTime::parse_from_rfc3339(client_ts) {
                (now - sent.with_timezone(&chrono::Utc)).num_milliseconds() as f64
            } else {
                0.0
            };
            serde_json::json!({
                "type": "HeartbeatPong",
                "timestamp": now.to_rfc3339(),
                "latencyMs": latency
            })
            .to_string()
        }

        _ => serde_json::json!({
            "type": "Error",
            "code": "protocol.unknown_message",
            "message": format!("Unknown message type: {msg_type}")
        })
        .to_string(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_handle_device_identify() {
        let msg = r#"{"type":"DeviceIdentify","deviceId":"dev-1","token":"tok","clientVersion":"0.1.0","timestamp":"2025-01-01T00:00:00Z"}"#;
        let response = handle_message(msg);
        let parsed: serde_json::Value = serde_json::from_str(&response).unwrap();
        assert_eq!(parsed["type"], "IdentifyAck");
        assert_eq!(parsed["accepted"], true);
    }

    #[test]
    fn test_handle_heartbeat_ping() {
        let msg = serde_json::json!({
            "type": "HeartbeatPing",
            "timestamp": chrono::Utc::now().to_rfc3339()
        })
        .to_string();
        let response = handle_message(&msg);
        let parsed: serde_json::Value = serde_json::from_str(&response).unwrap();
        assert_eq!(parsed["type"], "HeartbeatPong");
        assert!(parsed["latencyMs"].is_number());
    }

    #[test]
    fn test_handle_unknown_message() {
        let msg = r#"{"type":"SomeUnknownType","data":"test"}"#;
        let response = handle_message(msg);
        let parsed: serde_json::Value = serde_json::from_str(&response).unwrap();
        assert_eq!(parsed["type"], "Error");
        assert_eq!(parsed["code"], "protocol.unknown_message");
    }

    #[test]
    fn test_handle_invalid_json() {
        let response = handle_message("not json at all");
        let parsed: serde_json::Value = serde_json::from_str(&response).unwrap();
        assert_eq!(parsed["type"], "Error");
        assert_eq!(parsed["code"], "protocol.parse_error");
    }

    #[tokio::test]
    async fn test_mock_server_lifecycle() {
        let server = MockServer::start().await;
        let url = server.ws_url();
        assert!(url.starts_with("ws://127.0.0.1:"));

        // Connect and identify
        let (mut ws, _) = tokio_tungstenite::connect_async(&url).await.unwrap();

        let identify = serde_json::json!({
            "type": "DeviceIdentify",
            "deviceId": "test-dev",
            "token": "test-token",
            "clientVersion": "0.1.0",
            "timestamp": chrono::Utc::now().to_rfc3339()
        });
        ws.send(Message::Text(identify.to_string().into()))
            .await
            .unwrap();

        let ack = ws.next().await.unwrap().unwrap();
        if let Message::Text(text) = ack {
            let parsed: serde_json::Value = serde_json::from_str(&text).unwrap();
            assert_eq!(parsed["type"], "IdentifyAck");
            assert_eq!(parsed["accepted"], true);
        } else {
            panic!("Expected text message");
        }

        // Send heartbeat
        let ping = serde_json::json!({
            "type": "HeartbeatPing",
            "timestamp": chrono::Utc::now().to_rfc3339()
        });
        ws.send(Message::Text(ping.to_string().into()))
            .await
            .unwrap();

        let pong = ws.next().await.unwrap().unwrap();
        if let Message::Text(text) = pong {
            let parsed: serde_json::Value = serde_json::from_str(&text).unwrap();
            assert_eq!(parsed["type"], "HeartbeatPong");
        } else {
            panic!("Expected text message");
        }

        server.shutdown();
    }
}
