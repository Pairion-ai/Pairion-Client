//! Logging infrastructure for the Pairion Client.
//!
//! Configures the [`tracing`] crate for structured logging and provides
//! log forwarding to the Pairion Server via the `LogForward` AsyncAPI message.
//! In M0, log forwarding is stubbed — entries are batched but forwarding
//! is a no-op until the WebSocket client is fully operational.

use serde::{Deserialize, Serialize};
use tracing_subscriber::EnvFilter;

/// A single structured log entry destined for the Server.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LogEntry {
    /// ISO 8601 timestamp.
    pub timestamp: String,
    /// Log level.
    pub level: String,
    /// Subsystem that produced the entry.
    pub subsystem: String,
    /// Human-readable message.
    pub message: String,
}

/// Initializes the tracing subscriber for the Client application.
///
/// Sets up a JSON-formatted subscriber with an environment filter defaulting
/// to `info` level. Call this once during application startup.
pub fn init_tracing() {
    let filter = EnvFilter::try_from_default_env().unwrap_or_else(|_| EnvFilter::new("info"));

    tracing_subscriber::fmt()
        .with_env_filter(filter)
        .with_target(true)
        .with_thread_ids(false)
        .with_file(true)
        .with_line_number(true)
        .init();
}

/// Creates a new log entry with the current timestamp.
pub fn create_log_entry(level: &str, subsystem: &str, message: &str) -> LogEntry {
    LogEntry {
        timestamp: chrono::Utc::now().to_rfc3339(),
        level: level.to_string(),
        subsystem: subsystem.to_string(),
        message: message.to_string(),
    }
}

/// Batches log entries for forwarding to the Server.
///
/// In M0, this is a stub that collects entries. The actual forwarding
/// mechanism will be wired when the WebSocket client is fully operational
/// and the `LogForward` message pathway is exercised end-to-end.
#[derive(Debug, Default)]
pub struct LogForwarder {
    buffer: std::sync::Mutex<Vec<LogEntry>>,
}

impl LogForwarder {
    /// Creates a new log forwarder with an empty buffer.
    pub fn new() -> Self {
        Self {
            buffer: std::sync::Mutex::new(Vec::new()),
        }
    }

    /// Pushes a log entry into the forwarding buffer.
    pub fn push(&self, entry: LogEntry) {
        let mut buf = self.buffer.lock().unwrap();
        buf.push(entry);
    }

    /// Drains all buffered entries, returning them for forwarding.
    pub fn drain(&self) -> Vec<LogEntry> {
        let mut buf = self.buffer.lock().unwrap();
        buf.drain(..).collect()
    }

    /// Returns the number of buffered entries.
    pub fn len(&self) -> usize {
        self.buffer.lock().unwrap().len()
    }

    /// Returns true if the buffer is empty.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_create_log_entry() {
        let entry = create_log_entry("info", "ws", "connected to server");
        assert_eq!(entry.level, "info");
        assert_eq!(entry.subsystem, "ws");
        assert_eq!(entry.message, "connected to server");
        assert!(!entry.timestamp.is_empty());
    }

    #[test]
    fn test_log_entry_serialization() {
        let entry = create_log_entry("warn", "audio", "buffer underrun");
        let json = serde_json::to_string(&entry).expect("serialize");
        assert!(json.contains("\"level\":\"warn\""));
        assert!(json.contains("\"subsystem\":\"audio\""));
    }

    #[test]
    fn test_log_forwarder_push_and_drain() {
        let forwarder = LogForwarder::new();
        assert!(forwarder.is_empty());
        assert_eq!(forwarder.len(), 0);

        forwarder.push(create_log_entry("info", "test", "message 1"));
        forwarder.push(create_log_entry("debug", "test", "message 2"));
        assert_eq!(forwarder.len(), 2);
        assert!(!forwarder.is_empty());

        let entries = forwarder.drain();
        assert_eq!(entries.len(), 2);
        assert!(forwarder.is_empty());
    }

    #[test]
    fn test_log_forwarder_default() {
        let forwarder = LogForwarder::default();
        assert!(forwarder.is_empty());
    }
}
