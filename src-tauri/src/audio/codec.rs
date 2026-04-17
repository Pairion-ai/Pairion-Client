//! Opus encoding and decoding for the Pairion Client audio pipeline.
//!
//! Wraps the `audiopus` crate to provide Opus encoding of captured PCM
//! audio and decoding of incoming TTS audio. Both encoder and decoder
//! run on dedicated threads via [`tokio::task::spawn_blocking`] to
//! avoid blocking the Tokio executor.
//!
//! Encoding: 16 kHz mono, 20ms frames (320 samples), 24–32 kbps VBR.
//! Decoding: configurable sample rate (typically 24 kHz from Server TTS).

use audiopus::coder::{Decoder as OpusDecoder, Encoder as OpusEncoder};
use audiopus::packet::Packet;
use audiopus::{Application, Channels, MutSignals, SampleRate};

/// Maximum size of an encoded Opus frame in bytes.
const MAX_OPUS_FRAME_BYTES: usize = 1276;

/// Opus audio encoder for the capture path.
///
/// Encodes 16 kHz mono PCM frames into Opus packets suitable for
/// transmission as `AudioChunkIn` binary WebSocket frames.
pub struct PairionOpusEncoder {
    encoder: OpusEncoder,
    /// Number of samples per frame (320 for 20ms at 16kHz).
    frame_samples: usize,
}

impl PairionOpusEncoder {
    /// Creates a new Opus encoder for 16 kHz mono audio.
    pub fn new() -> Result<Self, OpusCodecError> {
        let encoder = OpusEncoder::new(SampleRate::Hz16000, Channels::Mono, Application::Voip)
            .map_err(|e| OpusCodecError::Init(format!("encoder: {e}")))?;

        Ok(Self {
            encoder,
            frame_samples: 320, // 20ms at 16kHz
        })
    }

    /// Encodes a single frame of PCM samples into an Opus packet.
    ///
    /// The input must contain exactly `frame_samples()` samples (320 for 16kHz/20ms).
    /// Returns the encoded Opus bytes.
    pub fn encode(&self, pcm: &[f32]) -> Result<Vec<u8>, OpusCodecError> {
        if pcm.len() != self.frame_samples {
            return Err(OpusCodecError::FrameSize {
                expected: self.frame_samples,
                got: pcm.len(),
            });
        }

        let mut output = vec![0u8; MAX_OPUS_FRAME_BYTES];
        let encoded_len = self
            .encoder
            .encode_float(pcm, &mut output)
            .map_err(|e| OpusCodecError::Encode(format!("{e}")))?;

        output.truncate(encoded_len);
        Ok(output)
    }

    /// Returns the expected number of samples per frame.
    pub fn frame_samples(&self) -> usize {
        self.frame_samples
    }
}

/// Opus audio decoder for the playback path.
///
/// Decodes incoming `AudioChunkOut` Opus packets into PCM samples
/// for the jitter buffer and cpal output.
pub struct PairionOpusDecoder {
    decoder: OpusDecoder,
    /// Output sample rate.
    sample_rate: u32,
    /// Maximum number of samples per decoded frame.
    max_frame_samples: usize,
}

impl PairionOpusDecoder {
    /// Creates a new Opus decoder for the given sample rate.
    ///
    /// Common sample rates: 16000, 24000, 48000 Hz.
    pub fn new(sample_rate: u32) -> Result<Self, OpusCodecError> {
        let opus_rate = match sample_rate {
            8000 => SampleRate::Hz8000,
            12000 => SampleRate::Hz12000,
            16000 => SampleRate::Hz16000,
            24000 => SampleRate::Hz24000,
            48000 => SampleRate::Hz48000,
            _ => {
                return Err(OpusCodecError::Init(format!(
                    "unsupported sample rate: {sample_rate}"
                )))
            }
        };

        let decoder = OpusDecoder::new(opus_rate, Channels::Mono)
            .map_err(|e| OpusCodecError::Init(format!("decoder: {e}")))?;

        // Max 120ms frame at the given rate
        let max_frame_samples = (sample_rate as usize * 120) / 1000;

        Ok(Self {
            decoder,
            sample_rate,
            max_frame_samples,
        })
    }

    /// Decodes an Opus packet into PCM samples.
    ///
    /// Returns the decoded f32 PCM samples.
    pub fn decode(&mut self, opus_data: &[u8]) -> Result<Vec<f32>, OpusCodecError> {
        let packet: Packet<'_> = opus_data
            .try_into()
            .map_err(|e| OpusCodecError::Decode(format!("invalid packet: {e}")))?;

        let mut pcm_f32 = vec![0.0f32; self.max_frame_samples];
        let output: MutSignals<'_, f32> = pcm_f32
            .as_mut_slice()
            .try_into()
            .map_err(|e| OpusCodecError::Decode(format!("signal buffer: {e}")))?;

        let decoded_samples = self
            .decoder
            .decode_float(Some(packet), output, false)
            .map_err(|e| OpusCodecError::Decode(format!("{e}")))?;

        pcm_f32.truncate(decoded_samples);
        Ok(pcm_f32)
    }

    /// Returns the output sample rate.
    pub fn sample_rate(&self) -> u32 {
        self.sample_rate
    }
}

/// Errors that can occur during Opus encoding or decoding.
#[derive(Debug, thiserror::Error)]
pub enum OpusCodecError {
    /// Failed to initialize the encoder or decoder.
    #[error("opus init error: {0}")]
    Init(String),
    /// Encoding failed.
    #[error("opus encode error: {0}")]
    Encode(String),
    /// Decoding failed.
    #[error("opus decode error: {0}")]
    Decode(String),
    /// Incorrect frame size provided.
    #[error("frame size mismatch: expected {expected}, got {got}")]
    FrameSize {
        /// Expected number of samples.
        expected: usize,
        /// Actual number of samples provided.
        got: usize,
    },
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_encoder_creation() {
        let encoder = PairionOpusEncoder::new();
        assert!(encoder.is_ok());
        assert_eq!(encoder.unwrap().frame_samples(), 320);
    }

    #[test]
    fn test_decoder_creation_16k() {
        let decoder = PairionOpusDecoder::new(16000);
        assert!(decoder.is_ok());
        assert_eq!(decoder.unwrap().sample_rate(), 16000);
    }

    #[test]
    fn test_decoder_creation_24k() {
        let decoder = PairionOpusDecoder::new(24000);
        assert!(decoder.is_ok());
        assert_eq!(decoder.unwrap().sample_rate(), 24000);
    }

    #[test]
    fn test_decoder_creation_48k() {
        let decoder = PairionOpusDecoder::new(48000);
        assert!(decoder.is_ok());
    }

    #[test]
    fn test_decoder_unsupported_rate() {
        let decoder = PairionOpusDecoder::new(44100);
        assert!(decoder.is_err());
    }

    #[test]
    fn test_encode_wrong_frame_size() {
        let encoder = PairionOpusEncoder::new().unwrap();
        let short_pcm = vec![0.0f32; 160]; // Too short
        let result = encoder.encode(&short_pcm);
        assert!(result.is_err());
        match result.unwrap_err() {
            OpusCodecError::FrameSize { expected, got } => {
                assert_eq!(expected, 320);
                assert_eq!(got, 160);
            }
            other => panic!("Expected FrameSize error, got {other:?}"),
        }
    }

    #[test]
    fn test_encode_silence() {
        let encoder = PairionOpusEncoder::new().unwrap();
        let silence = vec![0.0f32; 320];
        let encoded = encoder.encode(&silence).unwrap();
        assert!(!encoded.is_empty());
        assert!(encoded.len() < MAX_OPUS_FRAME_BYTES);
    }

    #[test]
    fn test_encode_decode_roundtrip() {
        let encoder = PairionOpusEncoder::new().unwrap();
        let mut decoder = PairionOpusDecoder::new(16000).unwrap();

        // Generate a simple sine wave
        let pcm: Vec<f32> = (0..320)
            .map(|i| (i as f32 * 440.0 * 2.0 * std::f32::consts::PI / 16000.0).sin() * 0.5)
            .collect();

        let encoded = encoder.encode(&pcm).unwrap();
        let decoded = decoder.decode(&encoded).unwrap();

        // Opus is lossy — verify decoded length matches and values are reasonable
        assert_eq!(decoded.len(), 320);
        // Check that decoded audio is not silence (energy preserved within lossy bounds)
        let energy: f32 = decoded.iter().map(|s| s * s).sum::<f32>() / decoded.len() as f32;
        assert!(energy > 0.01, "Decoded audio should not be silence");
    }

    #[test]
    fn test_encode_loud_signal() {
        let encoder = PairionOpusEncoder::new().unwrap();
        let loud = vec![0.8f32; 320];
        let encoded = encoder.encode(&loud).unwrap();
        assert!(!encoded.is_empty());
    }

    #[test]
    fn test_decode_returns_pcm_in_range() {
        let encoder = PairionOpusEncoder::new().unwrap();
        let mut decoder = PairionOpusDecoder::new(16000).unwrap();

        let pcm: Vec<f32> = (0..320)
            .map(|i| (i as f32 * 1000.0 * 2.0 * std::f32::consts::PI / 16000.0).sin())
            .collect();

        let encoded = encoder.encode(&pcm).unwrap();
        let decoded = decoder.decode(&encoded).unwrap();

        for sample in &decoded {
            assert!(
                *sample >= -1.5 && *sample <= 1.5,
                "Sample {sample} out of reasonable range"
            );
        }
    }
}
