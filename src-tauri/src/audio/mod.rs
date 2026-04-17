//! Audio pipeline for the Pairion Client.
//!
//! Handles microphone capture via `cpal`, PCM processing,
//! and playback. The pipeline is designed around non-blocking ring buffers
//! that decouple the real-time cpal callbacks from the processing threads.
//!
//! Architecture §5.3 latency budgets:
//! - Mic → first audio byte on wire: < 40ms
//! - First audio byte receive → playback: < 80ms

pub mod capture;
pub mod codec;
pub mod playback;

pub use capture::{AudioCaptureManager, CaptureError, PreWakeBuffer, PRE_WAKE_BUFFER_SAMPLES};
pub use codec::{OpusCodecError, PairionOpusDecoder, PairionOpusEncoder};
pub use playback::{AudioPlaybackManager, PlaybackError};

/// Audio capture configuration.
#[derive(Debug, Clone)]
pub struct CaptureConfig {
    /// Sample rate in Hz.
    pub sample_rate: u32,
    /// Number of audio channels.
    pub channels: u16,
    /// Frame size in milliseconds for chunking captured audio.
    pub frame_size_ms: u32,
}

impl Default for CaptureConfig {
    fn default() -> Self {
        Self {
            sample_rate: 16_000,
            channels: 1,
            frame_size_ms: 20,
        }
    }
}

impl CaptureConfig {
    /// Returns the number of samples per frame.
    pub fn frame_samples(&self) -> usize {
        (self.sample_rate as usize * self.frame_size_ms as usize) / 1000
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
            sample_rate: 24_000,
            channels: 1,
            jitter_buffer_ms: 50,
        }
    }
}

/// Orchestrates the full audio pipeline (capture + playback).
///
/// Holds the capture and playback managers and provides high-level
/// methods for starting/stopping voice sessions.
pub struct AudioPipeline {
    /// Audio capture manager.
    pub capture: AudioCaptureManager,
    /// Audio playback manager.
    pub playback: AudioPlaybackManager,
    /// Pre-wake circular buffer for retaining initial syllable.
    pub pre_wake_buffer: PreWakeBuffer,
    /// Capture configuration.
    pub capture_config: CaptureConfig,
    /// Playback configuration.
    pub playback_config: PlaybackConfig,
}

impl AudioPipeline {
    /// Creates a new audio pipeline with default configurations.
    pub fn new() -> Self {
        Self {
            capture: AudioCaptureManager::new(),
            playback: AudioPlaybackManager::new(),
            pre_wake_buffer: PreWakeBuffer::new(PRE_WAKE_BUFFER_SAMPLES),
            capture_config: CaptureConfig::default(),
            playback_config: PlaybackConfig::default(),
        }
    }

    /// Starts the capture pipeline.
    pub fn start_capture(&mut self) -> Result<(), CaptureError> {
        self.capture.start()
    }

    /// Stops the capture pipeline.
    pub fn stop_capture(&mut self) {
        self.capture.stop();
    }

    /// Starts the playback pipeline at the given sample rate.
    pub fn start_playback(&mut self, sample_rate: u32) -> Result<(), PlaybackError> {
        self.playback.start(sample_rate)
    }

    /// Stops the playback pipeline.
    pub fn stop_playback(&mut self) {
        self.playback.stop();
    }

    /// Returns whether capture is active.
    pub fn is_capturing(&self) -> bool {
        self.capture.is_capturing()
    }

    /// Returns whether playback is active.
    pub fn is_playing(&self) -> bool {
        self.playback.is_playing()
    }
}

impl Default for AudioPipeline {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_capture_config_default() {
        let config = CaptureConfig::default();
        assert_eq!(config.sample_rate, 16_000);
        assert_eq!(config.channels, 1);
        assert_eq!(config.frame_size_ms, 20);
    }

    #[test]
    fn test_capture_config_frame_samples() {
        let config = CaptureConfig::default();
        // 16000 * 20 / 1000 = 320
        assert_eq!(config.frame_samples(), 320);
    }

    #[test]
    fn test_playback_config_default() {
        let config = PlaybackConfig::default();
        assert_eq!(config.sample_rate, 24_000);
        assert_eq!(config.channels, 1);
        assert_eq!(config.jitter_buffer_ms, 50);
    }

    #[test]
    fn test_audio_pipeline_new() {
        let pipeline = AudioPipeline::new();
        assert!(!pipeline.is_capturing());
        assert!(!pipeline.is_playing());
    }

    #[test]
    fn test_audio_pipeline_default() {
        let pipeline = AudioPipeline::default();
        assert!(!pipeline.is_capturing());
        assert!(!pipeline.is_playing());
    }
}
