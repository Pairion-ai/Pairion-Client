//! Wake-word detection for the Pairion Client.
//!
//! Provides real wake-word detection using the openWakeWord three-model
//! ONNX pipeline: melspectrogram → embedding → wake-word scorer.
//! M1 uses the "hey jarvis" pre-trained model as a placeholder until
//! a custom "Hey Pairion" model is trained in a later milestone.
//!
//! False-wake suppression: a wake event within 500ms of a previous
//! one is suppressed.

pub mod models;

use std::time::Instant;

/// Result of processing an audio frame through the wake-word detector.
#[derive(Debug, Clone, PartialEq)]
pub enum WakeWordResult {
    /// No wake word detected in this frame.
    None,
    /// Wake word detected with the given confidence (0.0–1.0).
    Detected {
        /// Detection confidence.
        confidence: f64,
    },
}

/// Trait for wake-word detection implementations.
pub trait WakeWordDetector: Send {
    /// Processes a chunk of 16kHz mono PCM audio and returns a detection result.
    fn process(&mut self, samples: &[f32]) -> WakeWordResult;
    /// Resets the detector state.
    fn reset(&mut self);
    /// Returns the current sensitivity threshold.
    fn sensitivity(&self) -> f64;
    /// Sets the sensitivity threshold (0.0–1.0).
    fn set_sensitivity(&mut self, sensitivity: f64);
}

/// openWakeWord three-model ONNX pipeline detector.
///
/// Pipeline: audio (1280 samples at 16kHz = 80ms chunks)
///   → melspectrogram model → mel features
///   → embedding model → audio embeddings
///   → hey_jarvis model → wake probability score
///
/// The "hey jarvis" model is the M1 placeholder. The internal comment
/// notes this; the actual wake phrase used at M1 is "Hey Jarvis".
pub struct OpenWakeWordDetector {
    mel_session: ort::session::Session,
    emb_session: ort::session::Session,
    ww_session: ort::session::Session,
    /// Detection threshold (default 0.5 per openWakeWord recommendation).
    threshold: f64,
    /// Buffer for accumulating audio samples (1280 per chunk).
    audio_buffer: Vec<f32>,
    /// Ring buffer of recent embeddings for the wake-word model (needs 16 frames).
    embedding_buffer: Vec<Vec<f32>>,
    /// Last detection time for false-wake suppression.
    last_detection: Option<Instant>,
    /// Suppression window in ms.
    suppression_ms: u64,
}

impl OpenWakeWordDetector {
    /// Creates a new openWakeWord detector, loading all three ONNX models.
    ///
    /// Models are expected at `~/Library/Application Support/Pairion/models/openwakeword/`.
    /// Call [`models::ensure_model`] before constructing this detector.
    pub fn new(
        mel_path: &std::path::Path,
        emb_path: &std::path::Path,
        ww_path: &std::path::Path,
        threshold: f64,
    ) -> Result<Self, WakeWordError> {
        let start = Instant::now();

        let mel_session = ort::session::Session::builder()
            .map_err(|e| WakeWordError::ModelLoad(format!("mel builder: {e}")))?
            .commit_from_file(mel_path)
            .map_err(|e| WakeWordError::ModelLoad(format!("melspectrogram: {e}")))?;

        let emb_session = ort::session::Session::builder()
            .map_err(|e| WakeWordError::ModelLoad(format!("emb builder: {e}")))?
            .commit_from_file(emb_path)
            .map_err(|e| WakeWordError::ModelLoad(format!("embedding: {e}")))?;

        let ww_session = ort::session::Session::builder()
            .map_err(|e| WakeWordError::ModelLoad(format!("ww builder: {e}")))?
            .commit_from_file(ww_path)
            .map_err(|e| WakeWordError::ModelLoad(format!("hey_jarvis: {e}")))?;

        tracing::info!(
            load_time_ms = start.elapsed().as_millis(),
            "openWakeWord models loaded"
        );

        Ok(Self {
            mel_session,
            emb_session,
            ww_session,
            threshold,
            audio_buffer: Vec::with_capacity(1280),
            embedding_buffer: Vec::new(),
            last_detection: None,
            suppression_ms: 500,
        })
    }

    /// Loads all models from the default cache directory, downloading if needed.
    pub fn load_default(threshold: f64) -> Result<Self, WakeWordError> {
        let mel_path = models::ensure_model(
            "melspectrogram.onnx",
            models::EXPECTED_SHA256_MELSPECTROGRAM,
        )
        .map_err(|e| WakeWordError::ModelLoad(format!("{e}")))?;

        let emb_path =
            models::ensure_model("embedding_model.onnx", models::EXPECTED_SHA256_EMBEDDING)
                .map_err(|e| WakeWordError::ModelLoad(format!("{e}")))?;

        let ww_path =
            models::ensure_model("hey_jarvis_v0.1.onnx", models::EXPECTED_SHA256_HEY_JARVIS)
                .map_err(|e| WakeWordError::ModelLoad(format!("{e}")))?;

        Self::new(&mel_path, &emb_path, &ww_path, threshold)
    }

    /// Runs the three-model inference pipeline on a 1280-sample audio chunk.
    fn run_pipeline(&mut self, audio_chunk: &[f32]) -> Result<f64, WakeWordError> {
        use ort::value::TensorRef;

        // Step 1: melspectrogram — input: [1, 1280], output: [time, 1, dim, 32]
        let mel_input =
            ndarray::Array2::from_shape_vec((1, audio_chunk.len()), audio_chunk.to_vec())
                .map_err(|e| WakeWordError::Inference(format!("mel input: {e}")))?;
        let mel_ref = TensorRef::from_array_view(&mel_input)
            .map_err(|e| WakeWordError::Inference(format!("mel tensor: {e}")))?;

        let mel_outputs = self
            .mel_session
            .run(ort::inputs![mel_ref])
            .map_err(|e| WakeWordError::Inference(format!("mel run: {e}")))?;

        let (mel_shape, mel_data) = mel_outputs[0]
            .try_extract_tensor::<f32>()
            .map_err(|e| WakeWordError::Inference(format!("mel extract: {e}")))?;

        // mel output shape: [time, 1, dim, 32] — use time dim for embedding
        let emb_height = mel_shape[0] as usize;
        let emb_width = 32usize;
        let needed = emb_height * emb_width;
        if mel_data.len() < needed {
            return Ok(0.0);
        }

        // Step 2: embedding — input: [1, height, 32, 1], output: [1, 1, 1, 96]
        let emb_input = ndarray::Array4::from_shape_vec(
            (1, emb_height, emb_width, 1),
            mel_data[..needed].to_vec(),
        )
        .map_err(|e| WakeWordError::Inference(format!("emb reshape: {e}")))?;
        let emb_ref = TensorRef::from_array_view(&emb_input)
            .map_err(|e| WakeWordError::Inference(format!("emb tensor: {e}")))?;

        let emb_outputs = self
            .emb_session
            .run(ort::inputs![emb_ref])
            .map_err(|e| WakeWordError::Inference(format!("emb run: {e}")))?;

        let (_emb_shape, emb_data) = emb_outputs[0]
            .try_extract_tensor::<f32>()
            .map_err(|e| WakeWordError::Inference(format!("emb extract: {e}")))?;

        let embedding: Vec<f32> = emb_data.to_vec();

        // Step 3: accumulate embeddings (need 16 frames for wake-word model)
        self.embedding_buffer.push(embedding);
        if self.embedding_buffer.len() > 16 {
            self.embedding_buffer.remove(0);
        }
        if self.embedding_buffer.len() < 16 {
            return Ok(0.0);
        }

        // Build [1, 16, 96] tensor from the 16 most recent embeddings
        let mut ww_data = Vec::with_capacity(16 * 96);
        for emb in &self.embedding_buffer {
            if emb.len() >= 96 {
                ww_data.extend_from_slice(&emb[..96]);
            } else {
                ww_data.extend_from_slice(emb);
                ww_data.resize(ww_data.len() + (96 - emb.len()), 0.0);
            }
        }
        let ww_input = ndarray::Array3::from_shape_vec((1, 16, 96), ww_data)
            .map_err(|e| WakeWordError::Inference(format!("ww input: {e}")))?;
        let ww_ref = TensorRef::from_array_view(&ww_input)
            .map_err(|e| WakeWordError::Inference(format!("ww tensor: {e}")))?;

        let ww_outputs = self
            .ww_session
            .run(ort::inputs![ww_ref])
            .map_err(|e| WakeWordError::Inference(format!("ww run: {e}")))?;

        let (_ww_shape, ww_data) = ww_outputs[0]
            .try_extract_tensor::<f32>()
            .map_err(|e| WakeWordError::Inference(format!("ww extract: {e}")))?;

        Ok(ww_data.first().copied().unwrap_or(0.0) as f64)
    }
}

impl WakeWordDetector for OpenWakeWordDetector {
    fn process(&mut self, samples: &[f32]) -> WakeWordResult {
        self.audio_buffer.extend_from_slice(samples);

        // Process in 1280-sample chunks (80ms at 16kHz)
        while self.audio_buffer.len() >= 1280 {
            let chunk: Vec<f32> = self.audio_buffer.drain(..1280).collect();

            match self.run_pipeline(&chunk) {
                Ok(score) => {
                    if score >= self.threshold {
                        // False-wake suppression
                        if let Some(last) = self.last_detection {
                            if last.elapsed().as_millis() < self.suppression_ms as u128 {
                                continue;
                            }
                        }
                        self.last_detection = Some(Instant::now());
                        return WakeWordResult::Detected { confidence: score };
                    }
                }
                Err(e) => {
                    tracing::warn!(error = %e, "Wake-word inference error");
                }
            }
        }

        WakeWordResult::None
    }

    fn reset(&mut self) {
        self.audio_buffer.clear();
        self.embedding_buffer.clear();
        self.last_detection = None;
    }

    fn sensitivity(&self) -> f64 {
        self.threshold
    }

    fn set_sensitivity(&mut self, sensitivity: f64) {
        self.threshold = sensitivity.clamp(0.0, 1.0);
    }
}

/// Energy-based wake-word detector for development testing.
///
/// Fires when the audio energy exceeds a threshold, simulating wake-word
/// detection. NOT the default runtime path — use [`OpenWakeWordDetector`]
/// for production.
pub struct EnergyWakeDetector {
    sensitivity: f64,
    last_detection: Option<Instant>,
    suppression_ms: u64,
}

impl EnergyWakeDetector {
    /// Creates a new energy-based wake detector.
    pub fn new(sensitivity: f64) -> Self {
        Self {
            sensitivity,
            last_detection: None,
            suppression_ms: 500,
        }
    }

    /// Returns the energy threshold derived from sensitivity.
    fn threshold(&self) -> f32 {
        (1.0 - self.sensitivity as f32) * 0.1
    }
}

impl Default for EnergyWakeDetector {
    fn default() -> Self {
        Self::new(0.6)
    }
}

impl WakeWordDetector for EnergyWakeDetector {
    fn process(&mut self, samples: &[f32]) -> WakeWordResult {
        if samples.is_empty() {
            return WakeWordResult::None;
        }
        let energy: f32 =
            (samples.iter().map(|s| s * s).sum::<f32>() / samples.len() as f32).sqrt();

        if energy > self.threshold() {
            if let Some(last) = self.last_detection {
                if last.elapsed().as_millis() < self.suppression_ms as u128 {
                    return WakeWordResult::None;
                }
            }
            let confidence = (energy / 0.5).min(1.0) as f64;
            self.last_detection = Some(Instant::now());
            WakeWordResult::Detected { confidence }
        } else {
            WakeWordResult::None
        }
    }

    fn reset(&mut self) {
        self.last_detection = None;
    }

    fn sensitivity(&self) -> f64 {
        self.sensitivity
    }

    fn set_sensitivity(&mut self, sensitivity: f64) {
        self.sensitivity = sensitivity.clamp(0.0, 1.0);
    }
}

/// A mock wake-word detector for testing that always returns a detection.
pub struct AlwaysWakeDetector;

impl WakeWordDetector for AlwaysWakeDetector {
    fn process(&mut self, _samples: &[f32]) -> WakeWordResult {
        WakeWordResult::Detected { confidence: 0.99 }
    }
    fn reset(&mut self) {}
    fn sensitivity(&self) -> f64 {
        0.5
    }
    fn set_sensitivity(&mut self, _sensitivity: f64) {}
}

/// A mock wake-word detector for testing that never detects.
pub struct NeverWakeDetector;

impl WakeWordDetector for NeverWakeDetector {
    fn process(&mut self, _samples: &[f32]) -> WakeWordResult {
        WakeWordResult::None
    }
    fn reset(&mut self) {}
    fn sensitivity(&self) -> f64 {
        0.5
    }
    fn set_sensitivity(&mut self, _sensitivity: f64) {}
}

/// Errors from wake-word detection.
#[derive(Debug, thiserror::Error)]
pub enum WakeWordError {
    /// Failed to load an ONNX model.
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
    fn test_energy_detector_default() {
        let det = EnergyWakeDetector::default();
        assert!((det.sensitivity() - 0.6).abs() < f64::EPSILON);
    }

    #[test]
    fn test_energy_detector_silent_audio() {
        let mut det = EnergyWakeDetector::new(0.6);
        let silence = vec![0.0f32; 320];
        assert_eq!(det.process(&silence), WakeWordResult::None);
    }

    #[test]
    fn test_energy_detector_loud_audio() {
        let mut det = EnergyWakeDetector::new(0.6);
        let loud = vec![0.5f32; 320];
        match det.process(&loud) {
            WakeWordResult::Detected { confidence } => assert!(confidence > 0.0),
            WakeWordResult::None => panic!("Expected detection on loud audio"),
        }
    }

    #[test]
    fn test_energy_detector_false_wake_suppression() {
        let mut det = EnergyWakeDetector::new(0.6);
        let loud = vec![0.5f32; 320];
        assert!(matches!(
            det.process(&loud),
            WakeWordResult::Detected { .. }
        ));
        assert_eq!(det.process(&loud), WakeWordResult::None);
    }

    #[test]
    fn test_energy_detector_reset() {
        let mut det = EnergyWakeDetector::new(0.6);
        let loud = vec![0.5f32; 320];
        let _ = det.process(&loud);
        det.reset();
        assert!(matches!(
            det.process(&loud),
            WakeWordResult::Detected { .. }
        ));
    }

    #[test]
    fn test_energy_detector_set_sensitivity() {
        let mut det = EnergyWakeDetector::new(0.5);
        det.set_sensitivity(0.9);
        assert!((det.sensitivity() - 0.9).abs() < f64::EPSILON);
    }

    #[test]
    fn test_energy_detector_sensitivity_clamped() {
        let mut det = EnergyWakeDetector::new(0.5);
        det.set_sensitivity(1.5);
        assert!((det.sensitivity() - 1.0).abs() < f64::EPSILON);
        det.set_sensitivity(-0.5);
        assert!(det.sensitivity().abs() < f64::EPSILON);
    }

    #[test]
    fn test_energy_detector_empty_input() {
        let mut det = EnergyWakeDetector::new(0.6);
        assert_eq!(det.process(&[]), WakeWordResult::None);
    }

    #[test]
    fn test_always_wake_detector() {
        let mut det = AlwaysWakeDetector;
        match det.process(&[0.0; 320]) {
            WakeWordResult::Detected { confidence } => {
                assert!((confidence - 0.99).abs() < f64::EPSILON);
            }
            WakeWordResult::None => panic!("AlwaysWakeDetector should always detect"),
        }
    }

    #[test]
    fn test_never_wake_detector() {
        let mut det = NeverWakeDetector;
        assert_eq!(det.process(&[1.0; 320]), WakeWordResult::None);
    }

    #[test]
    fn test_openwakeword_model_loading() {
        // Only runs if models are downloaded
        let mel_path = models::model_cache_dir().join("melspectrogram.onnx");
        if mel_path.exists() {
            let result = OpenWakeWordDetector::load_default(0.5);
            assert!(result.is_ok(), "Model loading failed");
        }
    }

    #[test]
    fn test_openwakeword_silence_no_detection() {
        let mel_path = models::model_cache_dir().join("melspectrogram.onnx");
        if mel_path.exists() {
            let mut det = OpenWakeWordDetector::load_default(0.5).unwrap();
            // Feed 2 seconds of silence (32000 samples at 16kHz)
            let silence = vec![0.0f32; 32000];
            let result = det.process(&silence);
            assert_eq!(result, WakeWordResult::None);
        }
    }
}
