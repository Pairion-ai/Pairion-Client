//! Audio playback pipeline for the Pairion Client.
//!
//! Opens a cpal output stream against the default macOS output device
//! and plays back decoded PCM audio from a jitter buffer. Incoming
//! audio frames are written to the buffer by the WebSocket handler;
//! the cpal output callback reads from it in real-time.
//!
//! **Non-blocking invariant:** The cpal output callback does no allocation,
//! no locking, no logging, and no `.await`. It reads samples from a
//! lock-free SPSC ring buffer.

use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use cpal::{SampleRate, Stream, StreamConfig};
use ringbuf::traits::{Consumer, Producer, Split};
use ringbuf::HeapRb;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use tracing::{error, info, warn};

/// Size of the playback ring buffer in samples (24kHz mono, ~2 seconds).
const PLAYBACK_BUFFER_SAMPLES: usize = 48_000;

/// Manages audio playback through the system speaker.
pub struct AudioPlaybackManager {
    /// The active cpal output stream.
    _stream: Option<Stream>,
    /// Producer half of the ring buffer for writing decoded samples.
    producer: Option<ringbuf::HeapProd<f32>>,
    /// Flag indicating whether playback is active.
    playing: Arc<AtomicBool>,
    /// Target sample rate for playback.
    sample_rate: u32,
}

impl AudioPlaybackManager {
    /// Creates a new playback manager.
    pub fn new() -> Self {
        Self {
            _stream: None,
            producer: None,
            playing: Arc::new(AtomicBool::new(false)),
            sample_rate: 24_000,
        }
    }

    /// Opens the output stream for playback at the given sample rate.
    pub fn start(&mut self, sample_rate: u32) -> Result<(), PlaybackError> {
        if self.playing.load(Ordering::Relaxed) {
            return Ok(());
        }

        self.sample_rate = sample_rate;

        let host = cpal::default_host();
        let device = host
            .default_output_device()
            .ok_or(PlaybackError::NoOutputDevice)?;

        info!(
            device = device.name().unwrap_or_default(),
            sample_rate = sample_rate,
            "Opening audio output device"
        );

        let config = StreamConfig {
            channels: 1,
            sample_rate: SampleRate(sample_rate),
            buffer_size: cpal::BufferSize::Default,
        };

        let rb = HeapRb::<f32>::new(PLAYBACK_BUFFER_SAMPLES);
        let (producer, mut consumer) = rb.split();

        let playing = self.playing.clone();
        playing.store(true, Ordering::Relaxed);

        let playing_flag = playing.clone();

        // Output callback: MUST be non-blocking.
        let stream = device.build_output_stream(
            &config,
            move |data: &mut [f32], _: &cpal::OutputCallbackInfo| {
                if !playing_flag.load(Ordering::Relaxed) {
                    // Fill with silence
                    for sample in data.iter_mut() {
                        *sample = 0.0;
                    }
                    return;
                }
                let read = consumer.pop_slice(data);
                // Fill remainder with silence if buffer underrun
                for sample in data[read..].iter_mut() {
                    *sample = 0.0;
                }
            },
            move |err| {
                error!(error = %err, "Audio playback stream error");
            },
            None,
        )?;

        stream.play()?;
        self._stream = Some(stream);
        self.producer = Some(producer);

        info!(sample_rate = sample_rate, "Audio playback started");
        Ok(())
    }

    /// Writes decoded PCM samples into the playback buffer.
    ///
    /// Returns the number of samples actually written. If the buffer is
    /// full, excess samples are dropped with a warning.
    pub fn write_samples(&mut self, samples: &[f32]) -> usize {
        if let Some(producer) = &mut self.producer {
            let written = producer.push_slice(samples);
            if written < samples.len() {
                warn!(
                    dropped = samples.len() - written,
                    "Playback buffer overflow — dropping samples"
                );
            }
            written
        } else {
            0
        }
    }

    /// Stops playback and closes the output stream.
    pub fn stop(&mut self) {
        self.playing.store(false, Ordering::Relaxed);
        self._stream = None;
        self.producer = None;
        info!("Audio playback stopped");
    }

    /// Flushes the playback buffer immediately (for barge-in interrupt).
    ///
    /// In M1 this is a no-op stub. Real implementation in M4 when barge-in
    /// interrupt semantics are added.
    pub fn flush(&mut self) {
        // M4: drain the ring buffer and stop playback immediately
        self.stop();
    }

    /// Returns whether playback is currently active.
    pub fn is_playing(&self) -> bool {
        self.playing.load(Ordering::Relaxed)
    }

    /// Returns the current playback sample rate.
    pub fn sample_rate(&self) -> u32 {
        self.sample_rate
    }
}

impl Default for AudioPlaybackManager {
    fn default() -> Self {
        Self::new()
    }
}

/// Errors that can occur during audio playback.
#[derive(Debug, thiserror::Error)]
pub enum PlaybackError {
    /// No audio output device is available.
    #[error("no audio output device available")]
    NoOutputDevice,
    /// Error from the cpal audio backend.
    #[error("cpal error: {0}")]
    Cpal(#[from] cpal::BuildStreamError),
    /// Error playing the stream.
    #[error("stream play error: {0}")]
    Play(#[from] cpal::PlayStreamError),
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_playback_manager_default() {
        let mgr = AudioPlaybackManager::default();
        assert!(!mgr.is_playing());
        assert_eq!(mgr.sample_rate(), 24_000);
    }

    #[test]
    fn test_playback_manager_not_playing_initially() {
        let mgr = AudioPlaybackManager::new();
        assert!(!mgr.is_playing());
    }

    #[test]
    fn test_write_samples_when_not_playing() {
        let mut mgr = AudioPlaybackManager::new();
        assert_eq!(mgr.write_samples(&[0.1, 0.2, 0.3]), 0);
    }

    #[test]
    fn test_playback_buffer_constant() {
        assert!(PLAYBACK_BUFFER_SAMPLES > 0);
    }

    #[test]
    fn test_flush_stops_playback() {
        let mut mgr = AudioPlaybackManager::new();
        mgr.flush();
        assert!(!mgr.is_playing());
    }
}
