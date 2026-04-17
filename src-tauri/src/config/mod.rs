//! Configuration types for the Pairion Client.
//!
//! Defines server connection parameters, local file paths, and application
//! preferences. Configuration is loaded once at startup and made available
//! through the shared application state.

use serde::{Deserialize, Serialize};

/// Default WebSocket URL for the Pairion Server.
pub const DEFAULT_WS_URL: &str = "ws://localhost:18789/ws/v1";

/// Default heartbeat interval in seconds.
pub const HEARTBEAT_INTERVAL_SECS: u64 = 15;

/// Default heartbeat timeout in seconds.
pub const HEARTBEAT_TIMEOUT_SECS: u64 = 30;

/// Maximum reconnection backoff delay in seconds.
pub const MAX_BACKOFF_SECS: u64 = 30;

/// Default path for the fallback device token file.
pub const DEFAULT_TOKEN_PATH: &str = ".pairion/device.token";

/// Keychain service identifier for Pairion secrets.
pub const KEYCHAIN_SERVICE: &str = "ai.pairion.client";

/// Keychain account name for the bearer token.
pub const KEYCHAIN_ACCOUNT_TOKEN: &str = "device-bearer-token";

/// Client application configuration.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppConfig {
    /// WebSocket server URL.
    pub ws_url: String,
    /// Heartbeat ping interval in seconds.
    pub heartbeat_interval_secs: u64,
    /// Heartbeat pong timeout in seconds.
    pub heartbeat_timeout_secs: u64,
    /// Maximum backoff delay for reconnection in seconds.
    pub max_backoff_secs: u64,
}

impl Default for AppConfig {
    fn default() -> Self {
        Self {
            ws_url: DEFAULT_WS_URL.to_string(),
            heartbeat_interval_secs: HEARTBEAT_INTERVAL_SECS,
            heartbeat_timeout_secs: HEARTBEAT_TIMEOUT_SECS,
            max_backoff_secs: MAX_BACKOFF_SECS,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_default_config() {
        let config = AppConfig::default();
        assert_eq!(config.ws_url, DEFAULT_WS_URL);
        assert_eq!(config.heartbeat_interval_secs, 15);
        assert_eq!(config.heartbeat_timeout_secs, 30);
        assert_eq!(config.max_backoff_secs, 30);
    }

    #[test]
    fn test_config_serialization() {
        let config = AppConfig::default();
        let json = serde_json::to_string(&config).expect("serialize");
        let deserialized: AppConfig = serde_json::from_str(&json).expect("deserialize");
        assert_eq!(deserialized.ws_url, config.ws_url);
    }

    #[test]
    fn test_constants() {
        assert_eq!(DEFAULT_WS_URL, "ws://localhost:18789/ws/v1");
        assert_eq!(HEARTBEAT_INTERVAL_SECS, 15);
        assert_eq!(HEARTBEAT_TIMEOUT_SECS, 30);
        assert_eq!(MAX_BACKOFF_SECS, 30);
        assert_eq!(DEFAULT_TOKEN_PATH, ".pairion/device.token");
        assert_eq!(KEYCHAIN_SERVICE, "ai.pairion.client");
        assert_eq!(KEYCHAIN_ACCOUNT_TOKEN, "device-bearer-token");
    }
}
