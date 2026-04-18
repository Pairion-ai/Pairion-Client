//! Model file management for wake-word detection.
//!
//! Handles downloading, caching, and SHA-256 verification of the
//! openWakeWord ONNX model files. Models are downloaded on first run
//! from the openWakeWord GitHub release and cached at
//! `~/Library/Application Support/Pairion/models/openwakeword/`.

use sha2::{Digest, Sha256};
use std::path::{Path, PathBuf};
use tracing::{error, info};

/// Base URL for openWakeWord model downloads.
const MODEL_BASE_URL: &str = "https://github.com/dscripka/openWakeWord/releases/download/v0.5.1";

/// Expected SHA-256 of `melspectrogram.onnx`.
pub const EXPECTED_SHA256_MELSPECTROGRAM: &str =
    "ba2b0e0f8b7b875369a2c89cb13360ff53bac436f2895cced9f479fa65eb176f";

/// Expected SHA-256 of `embedding_model.onnx`.
pub const EXPECTED_SHA256_EMBEDDING: &str =
    "70d164290c1d095d1d4ee149bc5e00543250a7316b59f31d056cff7bd3075c1f";

/// Expected SHA-256 of `hey_jarvis_v0.1.onnx`.
pub const EXPECTED_SHA256_HEY_JARVIS: &str =
    "94a13cfe60075b132f6a472e7e462e8123ee70861bc3fb58434a73712ee0d2cb";

/// Expected SHA-256 of `silero_vad.onnx`.
pub const EXPECTED_SHA256_SILERO_VAD: &str =
    "a35ebf52fd3ce5f1469b2a36158dba761bc47b973ea3382b3186ca15b1f5af28";

/// Returns the model cache directory path.
pub fn model_cache_dir() -> PathBuf {
    let home = std::env::var("HOME").unwrap_or_else(|_| "/tmp".to_string());
    PathBuf::from(home).join("Library/Application Support/Pairion/models/openwakeword")
}

/// Ensures a model file exists at the expected path, downloading if absent.
///
/// Returns the path to the verified model file.
pub fn ensure_model(filename: &str, expected_sha256: &str) -> Result<PathBuf, ModelError> {
    let dir = model_cache_dir();
    std::fs::create_dir_all(&dir).map_err(|e| ModelError::Io(e.to_string()))?;

    let path = dir.join(filename);

    if path.exists() {
        let actual = compute_sha256(&path)?;
        if actual == expected_sha256 {
            info!(
                file = %filename,
                sha256 = %actual,
                "Model file verified"
            );
            return Ok(path);
        }
        error!(
            file = %filename,
            expected = %expected_sha256,
            actual = %actual,
            "Model checksum mismatch — re-downloading"
        );
    }

    download_model(filename, &path)?;

    let actual = compute_sha256(&path)?;
    if actual != expected_sha256 {
        return Err(ModelError::ChecksumMismatch {
            file: filename.to_string(),
            expected: expected_sha256.to_string(),
            actual,
        });
    }

    info!(
        file = %filename,
        sha256 = %actual,
        size_bytes = std::fs::metadata(&path).map(|m| m.len()).unwrap_or(0),
        "Model downloaded and verified"
    );

    Ok(path)
}

/// Downloads a model file from the openWakeWord GitHub release.
fn download_model(filename: &str, dest: &Path) -> Result<(), ModelError> {
    let url = format!("{MODEL_BASE_URL}/{filename}");
    info!(url = %url, dest = %dest.display(), "Downloading model");

    let output = std::process::Command::new("curl")
        .args(["-sL", "-o"])
        .arg(dest.as_os_str())
        .arg(&url)
        .output()
        .map_err(|e| ModelError::Download(format!("curl failed: {e}")))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(ModelError::Download(format!(
            "curl exited {}: {stderr}",
            output.status
        )));
    }

    Ok(())
}

/// Computes the SHA-256 hash of a file.
pub fn compute_sha256(path: &Path) -> Result<String, ModelError> {
    let data = std::fs::read(path).map_err(|e| ModelError::Io(e.to_string()))?;
    let hash = Sha256::digest(&data);
    Ok(format!("{hash:x}"))
}

/// Errors from model management operations.
#[derive(Debug, thiserror::Error)]
pub enum ModelError {
    /// I/O error reading/writing model files.
    #[error("io error: {0}")]
    Io(String),
    /// Model download failed.
    #[error("download failed: {0}")]
    Download(String),
    /// SHA-256 checksum mismatch after download.
    #[error("checksum mismatch for {file}: expected {expected}, got {actual}")]
    ChecksumMismatch {
        /// Filename.
        file: String,
        /// Expected SHA-256.
        expected: String,
        /// Actual SHA-256.
        actual: String,
    },
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_model_cache_dir() {
        let dir = model_cache_dir();
        assert!(dir
            .to_string_lossy()
            .contains("Pairion/models/openwakeword"));
    }

    #[test]
    fn test_sha256_constants_are_64_hex_chars() {
        for hash in [
            EXPECTED_SHA256_MELSPECTROGRAM,
            EXPECTED_SHA256_EMBEDDING,
            EXPECTED_SHA256_HEY_JARVIS,
            EXPECTED_SHA256_SILERO_VAD,
        ] {
            assert_eq!(hash.len(), 64, "SHA-256 should be 64 hex chars");
            assert!(
                hash.chars().all(|c| c.is_ascii_hexdigit()),
                "SHA-256 should be hex"
            );
        }
    }

    #[test]
    fn test_compute_sha256_on_temp_file() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("test.bin");
        std::fs::write(&path, b"hello world").unwrap();
        let hash = compute_sha256(&path).unwrap();
        assert_eq!(hash.len(), 64);
        // Known SHA-256 of "hello world"
        assert_eq!(
            hash,
            "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9"
        );
    }

    #[test]
    fn test_compute_sha256_nonexistent_file() {
        let result = compute_sha256(Path::new("/nonexistent/file.bin"));
        assert!(result.is_err());
    }

    #[test]
    fn test_ensure_model_with_existing_verified_file() {
        let dir = model_cache_dir();
        let path = dir.join("melspectrogram.onnx");
        if path.exists() {
            let result = ensure_model("melspectrogram.onnx", EXPECTED_SHA256_MELSPECTROGRAM);
            assert!(result.is_ok());
        }
    }
}
