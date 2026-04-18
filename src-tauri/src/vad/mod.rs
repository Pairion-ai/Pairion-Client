//! Voice Activity Detection (VAD) for the Pairion Client.
//!
//! Provides real Silero VAD ONNX model integration for detecting
//! end-of-speech. The VAD runs on the post-wake audio stream and
//! triggers when 800ms of sustained silence is detected.
//!
//! Silero VAD model: stateful LSTM that takes 512-sample chunks at 16kHz
//! and returns a speech probability per chunk.

use std::time::Instant;

/// Result of processing an audio frame through the VAD.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum VadResult {
    /// Speech is still active.
    Speech,
    /// Silence detected but not yet long enough to trigger end-of-speech.
    Silence,
    /// Sustained silence exceeded the threshold — end-of-speech.
    EndOfSpeech,
}

/// Trait for voice activity detection implementations.
pub trait VoiceActivityDetector: Send {
    /// Processes a chunk of 16kHz mono PCM audio and returns a VAD result.
    fn process(&mut self, samples: &[f32]) -> VadResult;
    /// Resets the detector state (e.g., between sessions).
    fn reset(&mut self);
    /// Returns the silence threshold duration in milliseconds.
    fn silence_threshold_ms(&self) -> u64;
}

/// Silero VAD ONNX model-based voice activity detector.
///
/// Input: 512-sample chunks at 16kHz (32ms per chunk).
/// The model is stateful (LSTM hidden/cell states are maintained).
/// Speech probability > 0.5 = speech; sustained sub-threshold = end-of-speech.
pub struct SileroVad {
    session: ort::session::Session,
    /// Speech probability threshold.
    speech_threshold: f32,
    /// Duration of silence required to trigger end-of-speech.
    silence_threshold_ms: u64,
    /// When silence started.
    silence_start: Option<Instant>,
    /// Whether end-of-speech has already fired.
    fired: bool,
    /// LSTM hidden state [2, 1, 64].
    h_state: Vec<f32>,
    /// LSTM cell state [2, 1, 64].
    c_state: Vec<f32>,
    /// Audio sample buffer (accumulate until 512 samples).
    audio_buffer: Vec<f32>,
}

impl SileroVad {
    /// Creates a new Silero VAD from the model file path.
    pub fn new(
        model_path: &std::path::Path,
        speech_threshold: f32,
        silence_threshold_ms: u64,
    ) -> Result<Self, VadError> {
        let session = ort::session::Session::builder()
            .map_err(|e| VadError::ModelLoad(format!("builder: {e}")))?
            .commit_from_file(model_path)
            .map_err(|e| VadError::ModelLoad(format!("{e}")))?;

        tracing::info!("Silero VAD model loaded");

        Ok(Self {
            session,
            speech_threshold,
            silence_threshold_ms,
            silence_start: None,
            fired: false,
            h_state: vec![0.0f32; 2 * 64],
            c_state: vec![0.0f32; 2 * 64],
            audio_buffer: Vec::with_capacity(512),
        })
    }

    /// Loads the Silero VAD model from the default cache directory.
    pub fn load_default(silence_threshold_ms: u64) -> Result<Self, VadError> {
        let path = crate::wake::models::ensure_model(
            "silero_vad.onnx",
            crate::wake::models::EXPECTED_SHA256_SILERO_VAD,
        )
        .map_err(|e| VadError::ModelLoad(format!("{e}")))?;

        Self::new(&path, 0.5, silence_threshold_ms)
    }

    /// Runs inference on a 512-sample chunk.
    fn infer(&mut self, chunk: &[f32]) -> Result<f32, VadError> {
        use ort::value::TensorRef;

        let input = ndarray::Array2::from_shape_vec((1, chunk.len()), chunk.to_vec())
            .map_err(|e| VadError::Inference(format!("input: {e}")))?;
        let input_ref = TensorRef::from_array_view(&input)
            .map_err(|e| VadError::Inference(format!("input tensor: {e}")))?;

        let sr = ndarray::arr0(16000_i64);
        let sr_ref = TensorRef::from_array_view(&sr)
            .map_err(|e| VadError::Inference(format!("sr tensor: {e}")))?;

        let h = ndarray::Array3::from_shape_vec((2, 1, 64), self.h_state.clone())
            .map_err(|e| VadError::Inference(format!("h state: {e}")))?;
        let h_ref = TensorRef::from_array_view(&h)
            .map_err(|e| VadError::Inference(format!("h tensor: {e}")))?;

        let c = ndarray::Array3::from_shape_vec((2, 1, 64), self.c_state.clone())
            .map_err(|e| VadError::Inference(format!("c state: {e}")))?;
        let c_ref = TensorRef::from_array_view(&c)
            .map_err(|e| VadError::Inference(format!("c tensor: {e}")))?;

        let outputs = self
            .session
            .run(ort::inputs![input_ref, sr_ref, h_ref, c_ref])
            .map_err(|e| VadError::Inference(format!("run: {e}")))?;

        // Extract speech probability
        let (_prob_shape, prob_data) = outputs[0]
            .try_extract_tensor::<f32>()
            .map_err(|e| VadError::Inference(format!("output: {e}")))?;
        let speech_prob = prob_data.first().copied().unwrap_or(0.0);

        // Update LSTM states
        if let Ok((_hn_shape, hn_data)) = outputs[1].try_extract_tensor::<f32>() {
            self.h_state = hn_data.to_vec();
        }
        if let Ok((_cn_shape, cn_data)) = outputs[2].try_extract_tensor::<f32>() {
            self.c_state = cn_data.to_vec();
        }

        Ok(speech_prob)
    }
}

impl VoiceActivityDetector for SileroVad {
    fn process(&mut self, samples: &[f32]) -> VadResult {
        if self.fired {
            return VadResult::EndOfSpeech;
        }

        self.audio_buffer.extend_from_slice(samples);

        let mut last_result = VadResult::Silence;

        // Process in 512-sample chunks
        while self.audio_buffer.len() >= 512 {
            let chunk: Vec<f32> = self.audio_buffer.drain(..512).collect();

            match self.infer(&chunk) {
                Ok(speech_prob) => {
                    if speech_prob > self.speech_threshold {
                        self.silence_start = None;
                        last_result = VadResult::Speech;
                    } else {
                        match self.silence_start {
                            Some(start) => {
                                if start.elapsed().as_millis() >= self.silence_threshold_ms as u128
                                {
                                    self.fired = true;
                                    return VadResult::EndOfSpeech;
                                }
                                last_result = VadResult::Silence;
                            }
                            None => {
                                self.silence_start = Some(Instant::now());
                                last_result = VadResult::Silence;
                            }
                        }
                    }
                }
                Err(e) => {
                    tracing::warn!(error = %e, "Silero VAD inference error");
                }
            }
        }

        last_result
    }

    fn reset(&mut self) {
        self.silence_start = None;
        self.fired = false;
        self.h_state = vec![0.0f32; 2 * 64];
        self.c_state = vec![0.0f32; 2 * 64];
        self.audio_buffer.clear();
    }

    fn silence_threshold_ms(&self) -> u64 {
        self.silence_threshold_ms
    }
}

/// Energy-based VAD for development testing (NOT the default runtime path).
pub struct EnergyVad {
    /// Energy threshold below which audio is silence.
    energy_threshold: f32,
    /// Duration of silence required to trigger end-of-speech.
    silence_threshold_ms: u64,
    /// When silence started.
    silence_start: Option<Instant>,
    /// Whether end-of-speech has already been fired.
    fired: bool,
}

impl EnergyVad {
    /// Creates a new energy-based VAD.
    pub fn new(energy_threshold: f32, silence_threshold_ms: u64) -> Self {
        Self {
            energy_threshold,
            silence_threshold_ms,
            silence_start: None,
            fired: false,
        }
    }
}

impl Default for EnergyVad {
    fn default() -> Self {
        Self::new(0.01, 800)
    }
}

impl VoiceActivityDetector for EnergyVad {
    fn process(&mut self, samples: &[f32]) -> VadResult {
        if self.fired || samples.is_empty() {
            return VadResult::EndOfSpeech;
        }
        let energy: f32 =
            (samples.iter().map(|s| s * s).sum::<f32>() / samples.len() as f32).sqrt();

        if energy > self.energy_threshold {
            self.silence_start = None;
            VadResult::Speech
        } else {
            match self.silence_start {
                Some(start) => {
                    if start.elapsed().as_millis() >= self.silence_threshold_ms as u128 {
                        self.fired = true;
                        VadResult::EndOfSpeech
                    } else {
                        VadResult::Silence
                    }
                }
                None => {
                    self.silence_start = Some(Instant::now());
                    VadResult::Silence
                }
            }
        }
    }

    fn reset(&mut self) {
        self.silence_start = None;
        self.fired = false;
    }

    fn silence_threshold_ms(&self) -> u64 {
        self.silence_threshold_ms
    }
}

/// A mock VAD that always reports speech.
pub struct AlwaysSpeechVad;

impl VoiceActivityDetector for AlwaysSpeechVad {
    fn process(&mut self, _samples: &[f32]) -> VadResult {
        VadResult::Speech
    }
    fn reset(&mut self) {}
    fn silence_threshold_ms(&self) -> u64 {
        800
    }
}

/// A mock VAD that immediately reports end-of-speech.
pub struct ImmediateEndVad;

impl VoiceActivityDetector for ImmediateEndVad {
    fn process(&mut self, _samples: &[f32]) -> VadResult {
        VadResult::EndOfSpeech
    }
    fn reset(&mut self) {}
    fn silence_threshold_ms(&self) -> u64 {
        0
    }
}

/// Errors from VAD operations.
#[derive(Debug, thiserror::Error)]
pub enum VadError {
    /// Failed to load the ONNX model.
    #[error("model load error: {0}")]
    ModelLoad(String),
    /// Inference failed.
    #[error("inference error: {0}")]
    Inference(String),
    /// ORT error.
    #[error("ort error: {0}")]
    Ort(#[from] ort::Error),
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_energy_vad_default() {
        let vad = EnergyVad::default();
        assert_eq!(vad.silence_threshold_ms(), 800);
    }

    #[test]
    fn test_energy_vad_speech() {
        let mut vad = EnergyVad::default();
        let speech = vec![0.5f32; 320];
        assert_eq!(vad.process(&speech), VadResult::Speech);
    }

    #[test]
    fn test_energy_vad_silence() {
        let mut vad = EnergyVad::default();
        let silence = vec![0.001f32; 320];
        assert_eq!(vad.process(&silence), VadResult::Silence);
    }

    #[test]
    fn test_energy_vad_speech_resets_silence() {
        let mut vad = EnergyVad::default();
        let silence = vec![0.001f32; 320];
        let speech = vec![0.5f32; 320];
        assert_eq!(vad.process(&silence), VadResult::Silence);
        assert_eq!(vad.process(&speech), VadResult::Speech);
        assert_eq!(vad.process(&silence), VadResult::Silence);
    }

    #[test]
    fn test_energy_vad_reset() {
        let mut vad = EnergyVad::default();
        let silence = vec![0.001f32; 320];
        let _ = vad.process(&silence);
        vad.reset();
        assert_eq!(vad.process(&silence), VadResult::Silence);
    }

    #[test]
    fn test_energy_vad_empty_input() {
        let mut vad = EnergyVad::default();
        assert_eq!(vad.process(&[]), VadResult::EndOfSpeech);
    }

    #[test]
    fn test_energy_vad_end_of_speech_sticks() {
        let mut vad = EnergyVad::new(0.01, 0);
        let silence = vec![0.001f32; 320];
        let _ = vad.process(&silence);
        assert_eq!(vad.process(&silence), VadResult::EndOfSpeech);
        assert_eq!(vad.process(&[0.5; 320]), VadResult::EndOfSpeech);
    }

    #[test]
    fn test_always_speech_vad() {
        let mut vad = AlwaysSpeechVad;
        assert_eq!(vad.process(&[0.0; 320]), VadResult::Speech);
    }

    #[test]
    fn test_immediate_end_vad() {
        let mut vad = ImmediateEndVad;
        assert_eq!(vad.process(&[0.5; 320]), VadResult::EndOfSpeech);
    }

    #[test]
    fn test_silero_vad_loads() {
        let model_path = crate::wake::models::model_cache_dir().join("silero_vad.onnx");
        if model_path.exists() {
            let result = SileroVad::load_default(800);
            assert!(result.is_ok(), "Silero VAD load failed");
        }
    }

    #[test]
    fn test_silero_vad_silence() {
        let model_path = crate::wake::models::model_cache_dir().join("silero_vad.onnx");
        if model_path.exists() {
            let mut vad = SileroVad::load_default(800).unwrap();
            let silence = vec![0.0f32; 512];
            let result = vad.process(&silence);
            // Silence should not be classified as speech
            assert_ne!(result, VadResult::Speech);
        }
    }
}
