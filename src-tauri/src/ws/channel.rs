//! Outbound WebSocket message channel.
//!
//! Provides a bounded mpsc channel for sending messages from any task
//! (audio capture, Tauri commands) to the WebSocket writer task.
//! The WS client task owns the receiver and is the sole sender on the wire.
//!
//! **Backpressure:** When the channel is full, audio frames take priority.
//! Heartbeats and log-forwards are dropped first.

use serde::Serialize;
use tokio::sync::mpsc;

/// Capacity of the outbound message channel.
pub const OUTBOUND_CHANNEL_CAPACITY: usize = 256;

/// An outbound WebSocket message queued for sending.
#[derive(Debug)]
pub enum OutboundMessage {
    /// A JSON text message.
    Json(String),
    /// A binary audio frame (Opus-encoded).
    Binary(Vec<u8>),
}

impl OutboundMessage {
    /// Creates a JSON outbound message from a serializable payload.
    pub fn json<T: Serialize>(payload: &T) -> Result<Self, serde_json::Error> {
        let text = serde_json::to_string(payload)?;
        Ok(Self::Json(text))
    }

    /// Creates a binary outbound message from raw bytes.
    pub fn binary(data: Vec<u8>) -> Self {
        Self::Binary(data)
    }

    /// Returns true if this is a binary (audio) message.
    pub fn is_binary(&self) -> bool {
        matches!(self, Self::Binary(_))
    }

    /// Returns true if this is a JSON text message.
    pub fn is_json(&self) -> bool {
        matches!(self, Self::Json(_))
    }
}

/// Creates a new outbound message channel pair.
///
/// The sender can be cloned and shared across tasks (audio capture,
/// Tauri commands). The receiver is consumed by the WS client task.
pub fn create_outbound_channel() -> (OutboundSender, OutboundReceiver) {
    let (tx, rx) = mpsc::channel(OUTBOUND_CHANNEL_CAPACITY);
    (OutboundSender(tx), OutboundReceiver(rx))
}

/// Sender half of the outbound message channel.
///
/// Cloneable — shared across the audio capture task, Tauri command
/// handlers, and any other task that needs to send messages to the Server.
#[derive(Debug, Clone)]
pub struct OutboundSender(mpsc::Sender<OutboundMessage>);

impl OutboundSender {
    /// Sends a message to the WS writer task.
    ///
    /// Returns an error if the channel is closed (WS client disconnected).
    pub async fn send(&self, msg: OutboundMessage) -> Result<(), ChannelError> {
        self.0.send(msg).await.map_err(|_| ChannelError::Closed)
    }

    /// Tries to send a message without waiting.
    ///
    /// Returns an error if the channel is full or closed. Audio frames
    /// should use `send()` (async); heartbeats and logs can use `try_send()`
    /// and accept drops under backpressure.
    pub fn try_send(&self, msg: OutboundMessage) -> Result<(), ChannelError> {
        self.0.try_send(msg).map_err(|e| match e {
            mpsc::error::TrySendError::Full(_) => ChannelError::Full,
            mpsc::error::TrySendError::Closed(_) => ChannelError::Closed,
        })
    }
}

/// Receiver half of the outbound message channel.
///
/// Consumed by the WS client task. Not cloneable.
pub struct OutboundReceiver(mpsc::Receiver<OutboundMessage>);

impl OutboundReceiver {
    /// Receives the next outbound message, waiting if necessary.
    pub async fn recv(&mut self) -> Option<OutboundMessage> {
        self.0.recv().await
    }
}

/// Errors from the outbound channel.
#[derive(Debug, thiserror::Error)]
pub enum ChannelError {
    /// The channel is closed (WS client disconnected).
    #[error("outbound channel closed")]
    Closed,
    /// The channel is full (backpressure).
    #[error("outbound channel full")]
    Full,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn test_channel_send_receive_json() {
        let (tx, mut rx) = create_outbound_channel();
        let msg = OutboundMessage::Json(r#"{"type":"test"}"#.to_string());
        assert!(msg.is_json());
        assert!(!msg.is_binary());

        tx.send(OutboundMessage::Json("hello".to_string()))
            .await
            .unwrap();
        let received = rx.recv().await.unwrap();
        assert!(received.is_json());
    }

    #[tokio::test]
    async fn test_channel_send_receive_binary() {
        let (tx, mut rx) = create_outbound_channel();
        tx.send(OutboundMessage::binary(vec![1, 2, 3]))
            .await
            .unwrap();
        let received = rx.recv().await.unwrap();
        assert!(received.is_binary());
    }

    #[tokio::test]
    async fn test_channel_try_send() {
        let (tx, mut rx) = create_outbound_channel();
        tx.try_send(OutboundMessage::Json("test".to_string()))
            .unwrap();
        let received = rx.recv().await.unwrap();
        assert!(received.is_json());
    }

    #[tokio::test]
    async fn test_channel_closed() {
        let (tx, rx) = create_outbound_channel();
        drop(rx);
        let result = tx.send(OutboundMessage::Json("test".to_string())).await;
        assert!(result.is_err());
    }

    #[tokio::test]
    async fn test_channel_try_send_closed() {
        let (tx, rx) = create_outbound_channel();
        drop(rx);
        let result = tx.try_send(OutboundMessage::Json("test".to_string()));
        assert!(result.is_err());
    }

    #[test]
    fn test_outbound_message_json_from_serializable() {
        #[derive(serde::Serialize)]
        struct TestMsg {
            r#type: String,
        }
        let msg = OutboundMessage::json(&TestMsg {
            r#type: "test".to_string(),
        })
        .unwrap();
        assert!(msg.is_json());
    }

    #[test]
    fn test_channel_capacity() {
        assert_eq!(OUTBOUND_CHANNEL_CAPACITY, 256);
    }
}
