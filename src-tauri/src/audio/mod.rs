//! Audio pipeline for the Pairion Client.
//!
//! Handles microphone capture via `cpal`, Opus encoding/decoding,
//! and playback. In M0 this module contains only type declarations;
//! the full audio pipeline is implemented in M1.

/// Audio capture configuration.
#[derive(Debug, Clone)]
pub struct CaptureConfig {
    /// Sample rate in Hz.
    pub sample_rate: u32,
    /// Number of audio channels.
    pub channels: u16,
    /// Opus frame size in milliseconds.
    pub frame_size_ms: u32,
}

impl Default for CaptureConfig {
    fn default() -> Self {
        Self {
            sample_rate: 48000,
            channels: 1,
            frame_size_ms: 20,
        }
    }
}

/// Audio playback configuration.
#[derive(Debug, Clone)]
pub struct PlaybackConfig {
    /// Sample rate in Hz.
    pub sample_rate: u32,
    /// Number of audio channels.
    pub channels: u16,
    /// Jitter buffer target size in milliseconds.
    pub jitter_buffer_ms: u32,
}

impl Default for PlaybackConfig {
    fn default() -> Self {
        Self {
            sample_rate: 48000,
            channels: 1,
            jitter_buffer_ms: 50,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_capture_config_default() {
        let config = CaptureConfig::default();
        assert_eq!(config.sample_rate, 48000);
        assert_eq!(config.channels, 1);
        assert_eq!(config.frame_size_ms, 20);
    }

    #[test]
    fn test_playback_config_default() {
        let config = PlaybackConfig::default();
        assert_eq!(config.sample_rate, 48000);
        assert_eq!(config.channels, 1);
        assert_eq!(config.jitter_buffer_ms, 50);
    }
}
