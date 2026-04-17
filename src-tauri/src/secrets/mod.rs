//! Secrets management for the Pairion Client.
//!
//! Wraps bearer-token reads and writes through a keychain abstraction.
//! In production, tokens are stored in macOS Keychain via `tauri-plugin-keyring`.
//! For M0, we provide a trait-based interface with a file-based fallback
//! and a scrubber utility that removes known secret patterns from log output.

use crate::config;
use std::path::PathBuf;

/// Errors that can occur during secrets operations.
#[derive(Debug, thiserror::Error)]
pub enum SecretsError {
    /// The requested secret was not found in any store.
    #[error("secret not found: {0}")]
    NotFound(String),
    /// An I/O error occurred reading the fallback token file.
    #[error("io error: {0}")]
    Io(#[from] std::io::Error),
    /// A keychain operation failed.
    #[error("keychain error: {0}")]
    Keychain(String),
}

/// Reads the bearer token, trying the keychain first, then the fallback file.
///
/// If the token is found in the fallback file but not the keychain,
/// it is written to the keychain for subsequent runs.
pub fn read_bearer_token(keychain: &dyn KeychainProvider) -> Result<String, SecretsError> {
    // Try keychain first
    if let Ok(token) = keychain.get(config::KEYCHAIN_SERVICE, config::KEYCHAIN_ACCOUNT_TOKEN) {
        if !token.is_empty() {
            return Ok(token);
        }
    }

    // Fall back to file
    let token_path = fallback_token_path();
    if token_path.exists() {
        let token = std::fs::read_to_string(&token_path)?.trim().to_string();
        if !token.is_empty() {
            // Mirror to keychain for subsequent runs
            let _ = keychain.set(
                config::KEYCHAIN_SERVICE,
                config::KEYCHAIN_ACCOUNT_TOKEN,
                &token,
            );
            return Ok(token);
        }
    }

    Err(SecretsError::NotFound(
        "bearer token not found in keychain or fallback file".to_string(),
    ))
}

/// Returns the path to the fallback device token file.
pub fn fallback_token_path() -> PathBuf {
    dirs_next().join(config::DEFAULT_TOKEN_PATH)
}

/// Returns the user's home directory.
fn dirs_next() -> PathBuf {
    std::env::var("HOME")
        .map(PathBuf::from)
        .unwrap_or_else(|_| PathBuf::from("/tmp"))
}

/// Abstraction over keychain operations for testability.
pub trait KeychainProvider: Send + Sync {
    /// Retrieves a secret from the keychain.
    fn get(&self, service: &str, account: &str) -> Result<String, SecretsError>;
    /// Stores a secret in the keychain.
    fn set(&self, service: &str, account: &str, value: &str) -> Result<(), SecretsError>;
    /// Deletes a secret from the keychain.
    fn delete(&self, service: &str, account: &str) -> Result<(), SecretsError>;
}

/// A no-op keychain provider for environments where keychain is unavailable.
/// Returns `NotFound` for all get operations. Used in tests.
pub struct NoopKeychainProvider;

impl KeychainProvider for NoopKeychainProvider {
    fn get(&self, _service: &str, _account: &str) -> Result<String, SecretsError> {
        Err(SecretsError::NotFound("noop keychain".to_string()))
    }

    fn set(&self, _service: &str, _account: &str, _value: &str) -> Result<(), SecretsError> {
        Ok(())
    }

    fn delete(&self, _service: &str, _account: &str) -> Result<(), SecretsError> {
        Ok(())
    }
}

/// In-memory keychain provider for testing.
pub struct InMemoryKeychainProvider {
    store: std::sync::Mutex<std::collections::HashMap<String, String>>,
}

impl InMemoryKeychainProvider {
    /// Creates a new empty in-memory keychain.
    pub fn new() -> Self {
        Self {
            store: std::sync::Mutex::new(std::collections::HashMap::new()),
        }
    }
}

impl Default for InMemoryKeychainProvider {
    fn default() -> Self {
        Self::new()
    }
}

impl KeychainProvider for InMemoryKeychainProvider {
    fn get(&self, service: &str, account: &str) -> Result<String, SecretsError> {
        let key = format!("{service}:{account}");
        let store = self.store.lock().unwrap();
        store.get(&key).cloned().ok_or(SecretsError::NotFound(key))
    }

    fn set(&self, service: &str, account: &str, value: &str) -> Result<(), SecretsError> {
        let key = format!("{service}:{account}");
        let mut store = self.store.lock().unwrap();
        store.insert(key, value.to_string());
        Ok(())
    }

    fn delete(&self, service: &str, account: &str) -> Result<(), SecretsError> {
        let key = format!("{service}:{account}");
        let mut store = self.store.lock().unwrap();
        store.remove(&key);
        Ok(())
    }
}

/// Scrubs known secret patterns from a string.
///
/// In M0 this is a placeholder implementation that demonstrates the scrubbing
/// pathway. The pattern matching will be expanded as real secret formats are
/// established in later milestones.
pub fn scrub_secrets(input: &str) -> String {
    let mut output = input.to_string();
    // Scrub bearer token patterns (e.g., "Bearer <token>")
    let bearer_re = regex_lite::Regex::new(r"Bearer\s+[A-Za-z0-9\-._~+/]+=*").unwrap();
    output = bearer_re
        .replace_all(&output, "Bearer [REDACTED]")
        .to_string();
    // Scrub raw token-like patterns (hex strings of 32+ chars)
    let hex_re = regex_lite::Regex::new(r"\b[a-f0-9]{32,}\b").unwrap();
    output = hex_re.replace_all(&output, "[REDACTED]").to_string();
    output
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_noop_keychain_returns_not_found() {
        let kc = NoopKeychainProvider;
        assert!(kc.get("svc", "acct").is_err());
    }

    #[test]
    fn test_noop_keychain_set_succeeds() {
        let kc = NoopKeychainProvider;
        assert!(kc.set("svc", "acct", "val").is_ok());
    }

    #[test]
    fn test_noop_keychain_delete_succeeds() {
        let kc = NoopKeychainProvider;
        assert!(kc.delete("svc", "acct").is_ok());
    }

    #[test]
    fn test_in_memory_keychain_roundtrip() {
        let kc = InMemoryKeychainProvider::new();
        assert!(kc.get("svc", "acct").is_err());
        kc.set("svc", "acct", "secret123").unwrap();
        assert_eq!(kc.get("svc", "acct").unwrap(), "secret123");
        kc.delete("svc", "acct").unwrap();
        assert!(kc.get("svc", "acct").is_err());
    }

    #[test]
    fn test_in_memory_keychain_default() {
        let kc = InMemoryKeychainProvider::default();
        assert!(kc.get("any", "thing").is_err());
    }

    #[test]
    fn test_read_bearer_token_from_keychain() {
        let kc = InMemoryKeychainProvider::new();
        kc.set(
            config::KEYCHAIN_SERVICE,
            config::KEYCHAIN_ACCOUNT_TOKEN,
            "my-token",
        )
        .unwrap();
        let token = read_bearer_token(&kc).unwrap();
        assert_eq!(token, "my-token");
    }

    #[test]
    fn test_read_bearer_token_from_file_fallback() {
        let dir = tempfile::tempdir().unwrap();
        let pairion_dir = dir.path().join(".pairion");
        std::fs::create_dir_all(&pairion_dir).unwrap();
        let token_file = pairion_dir.join("device.token");
        std::fs::write(&token_file, "file-token\n").unwrap();

        // Temporarily override HOME
        let original_home = std::env::var("HOME").ok();
        std::env::set_var("HOME", dir.path());

        let kc = InMemoryKeychainProvider::new();
        let token = read_bearer_token(&kc).unwrap();
        assert_eq!(token, "file-token");

        // Verify it was mirrored to keychain
        let mirrored = kc
            .get(config::KEYCHAIN_SERVICE, config::KEYCHAIN_ACCOUNT_TOKEN)
            .unwrap();
        assert_eq!(mirrored, "file-token");

        // Restore HOME
        if let Some(home) = original_home {
            std::env::set_var("HOME", home);
        }
    }

    #[test]
    fn test_read_bearer_token_not_found() {
        let dir = tempfile::tempdir().unwrap();
        let original_home = std::env::var("HOME").ok();
        std::env::set_var("HOME", dir.path());

        let kc = NoopKeychainProvider;
        assert!(read_bearer_token(&kc).is_err());

        if let Some(home) = original_home {
            std::env::set_var("HOME", home);
        }
    }

    #[test]
    fn test_fallback_token_path() {
        let path = fallback_token_path();
        assert!(path.to_string_lossy().contains(".pairion/device.token"));
    }

    #[test]
    fn test_scrub_bearer_token() {
        let input = "Authorization: Bearer abc123def456.xyz";
        let output = scrub_secrets(input);
        assert!(output.contains("[REDACTED]"));
        assert!(!output.contains("abc123def456"));
    }

    #[test]
    fn test_scrub_hex_token() {
        let input = "token=a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6";
        let output = scrub_secrets(input);
        assert!(output.contains("[REDACTED]"));
        assert!(!output.contains("a1b2c3d4e5f6a1b2c3d4e5f6"));
    }

    #[test]
    fn test_scrub_no_secrets() {
        let input = "normal log message with no secrets";
        let output = scrub_secrets(input);
        assert_eq!(output, input);
    }
}
