//! Audio capture pipeline for the Pairion Client.
//!
//! Opens a cpal input stream against the default macOS input device,
//! captures PCM audio, and writes it into a lock-free ring buffer.
//! A processing task reads from the ring buffer on a dedicated thread.
//!
//! **Non-blocking invariant:** The cpal callback does no allocation,
//! no locking, no logging, and no `.await`. It writes raw samples
//! into a lock-free SPSC ring buffer. This is load-bearing for latency.

use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use cpal::{SampleRate, Stream, StreamConfig};
use ringbuf::traits::{Consumer, Producer, Split};
use ringbuf::HeapRb;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use tracing::{error, info};

/// Size of the ring buffer in samples (16kHz mono, ~2 seconds).
const RING_BUFFER_SAMPLES: usize = 32_000;

/// Pre-wake buffer size in samples (200ms at 16kHz).
pub const PRE_WAKE_BUFFER_SAMPLES: usize = 3_200;

/// Manages audio capture from the system microphone.
pub struct AudioCaptureManager {
    /// The active cpal input stream (kept alive while capturing).
    _stream: Option<Stream>,
    /// Consumer half of the ring buffer for reading captured samples.
    consumer: Option<ringbuf::HeapCons<f32>>,
    /// Flag indicating whether capture is active.
    capturing: Arc<AtomicBool>,
}

impl AudioCaptureManager {
    /// Creates a new capture manager (not yet capturing).
    pub fn new() -> Self {
        Self {
            _stream: None,
            consumer: None,
            capturing: Arc::new(AtomicBool::new(false)),
        }
    }

    /// Starts capturing audio from the default input device at 16kHz mono.
    ///
    /// Returns an error if no input device is available or the stream
    /// cannot be opened.
    pub fn start(&mut self) -> Result<(), CaptureError> {
        if self.capturing.load(Ordering::Relaxed) {
            return Ok(());
        }

        let host = cpal::default_host();
        let device = host
            .default_input_device()
            .ok_or(CaptureError::NoInputDevice)?;

        info!(
            device = device.name().unwrap_or_default(),
            "Opening audio input device"
        );

        let config = StreamConfig {
            channels: 1,
            sample_rate: SampleRate(16_000),
            buffer_size: cpal::BufferSize::Default,
        };

        let rb = HeapRb::<f32>::new(RING_BUFFER_SAMPLES);
        let (mut producer, consumer) = rb.split();

        let capturing = self.capturing.clone();
        capturing.store(true, Ordering::Relaxed);

        let capturing_flag = capturing.clone();

        // The cpal callback: MUST be non-blocking.
        // No allocations, no locks, no logging, no await.
        let stream = device.build_input_stream(
            &config,
            move |data: &[f32], _: &cpal::InputCallbackInfo| {
                if !capturing_flag.load(Ordering::Relaxed) {
                    return;
                }
                // Write as many samples as the ring buffer can accept.
                // Overflow is intentional — we drop old samples rather than block.
                let _ = producer.push_slice(data);
            },
            move |err| {
                error!(error = %err, "Audio capture stream error");
            },
            None,
        )?;

        stream.play()?;
        self._stream = Some(stream);
        self.consumer = Some(consumer);

        info!("Audio capture started at 16kHz mono");
        Ok(())
    }

    /// Stops capturing audio.
    pub fn stop(&mut self) {
        self.capturing.store(false, Ordering::Relaxed);
        self._stream = None;
        self.consumer = None;
        info!("Audio capture stopped");
    }

    /// Returns whether capture is currently active.
    pub fn is_capturing(&self) -> bool {
        self.capturing.load(Ordering::Relaxed)
    }

    /// Reads available samples from the ring buffer into the provided slice.
    ///
    /// Returns the number of samples actually read.
    pub fn read_samples(&mut self, buf: &mut [f32]) -> usize {
        if let Some(consumer) = &mut self.consumer {
            consumer.pop_slice(buf)
        } else {
            0
        }
    }
}

impl Default for AudioCaptureManager {
    fn default() -> Self {
        Self::new()
    }
}

/// Errors that can occur during audio capture.
#[derive(Debug, thiserror::Error)]
pub enum CaptureError {
    /// No audio input device is available.
    #[error("no audio input device available")]
    NoInputDevice,
    /// Error from the cpal audio backend.
    #[error("cpal error: {0}")]
    Cpal(#[from] cpal::BuildStreamError),
    /// Error playing the stream.
    #[error("stream play error: {0}")]
    Play(#[from] cpal::PlayStreamError),
}

/// A circular pre-wake buffer that retains the most recent N samples.
///
/// Used to buffer ~200ms of audio before wake-word detection fires,
/// so the initial syllable of the wake phrase isn't clipped when
/// streaming begins.
pub struct PreWakeBuffer {
    buf: Vec<f32>,
    write_pos: usize,
    filled: bool,
}

impl PreWakeBuffer {
    /// Creates a new pre-wake buffer with the given capacity in samples.
    pub fn new(capacity: usize) -> Self {
        Self {
            buf: vec![0.0; capacity],
            write_pos: 0,
            filled: false,
        }
    }

    /// Writes samples into the circular buffer.
    pub fn write(&mut self, samples: &[f32]) {
        for &s in samples {
            self.buf[self.write_pos] = s;
            self.write_pos = (self.write_pos + 1) % self.buf.len();
            if self.write_pos == 0 {
                self.filled = true;
            }
        }
    }

    /// Drains the buffered samples in chronological order.
    pub fn drain(&mut self) -> Vec<f32> {
        let len = self.buf.len();
        let mut result = Vec::with_capacity(len);
        if self.filled {
            result.extend_from_slice(&self.buf[self.write_pos..]);
            result.extend_from_slice(&self.buf[..self.write_pos]);
        } else {
            result.extend_from_slice(&self.buf[..self.write_pos]);
        }
        self.write_pos = 0;
        self.filled = false;
        result
    }

    /// Returns the number of valid samples in the buffer.
    pub fn len(&self) -> usize {
        if self.filled {
            self.buf.len()
        } else {
            self.write_pos
        }
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
    fn test_capture_manager_default() {
        let mgr = AudioCaptureManager::default();
        assert!(!mgr.is_capturing());
    }

    #[test]
    fn test_capture_manager_not_capturing_initially() {
        let mgr = AudioCaptureManager::new();
        assert!(!mgr.is_capturing());
    }

    #[test]
    fn test_read_samples_when_not_capturing() {
        let mut mgr = AudioCaptureManager::new();
        let mut buf = [0.0f32; 320];
        assert_eq!(mgr.read_samples(&mut buf), 0);
    }

    #[test]
    fn test_pre_wake_buffer_empty() {
        let buf = PreWakeBuffer::new(100);
        assert!(buf.is_empty());
        assert_eq!(buf.len(), 0);
    }

    #[test]
    fn test_pre_wake_buffer_write_and_drain() {
        let mut buf = PreWakeBuffer::new(5);
        buf.write(&[1.0, 2.0, 3.0]);
        assert_eq!(buf.len(), 3);
        let drained = buf.drain();
        assert_eq!(drained, vec![1.0, 2.0, 3.0]);
    }

    #[test]
    fn test_pre_wake_buffer_circular_overflow() {
        let mut buf = PreWakeBuffer::new(4);
        buf.write(&[1.0, 2.0, 3.0, 4.0, 5.0, 6.0]);
        assert_eq!(buf.len(), 4);
        let drained = buf.drain();
        // After wrapping: positions 0,1 have 5.0,6.0; positions 2,3 have 3.0,4.0
        // Chronological order: 3.0, 4.0, 5.0, 6.0
        assert_eq!(drained, vec![3.0, 4.0, 5.0, 6.0]);
    }

    #[test]
    fn test_pre_wake_buffer_drain_resets() {
        let mut buf = PreWakeBuffer::new(4);
        buf.write(&[1.0, 2.0]);
        let _ = buf.drain();
        assert!(buf.is_empty());
    }

    #[test]
    fn test_ring_buffer_constant_size() {
        assert!(RING_BUFFER_SAMPLES > 0);
        assert_eq!(PRE_WAKE_BUFFER_SAMPLES, 3200);
    }
}
