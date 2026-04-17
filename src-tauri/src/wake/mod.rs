//! Wake-word detection for the Pairion Client.
//!
//! Provides a trait-based abstraction for wake-word detection, with a
//! threshold-based implementation that processes audio frames and fires
//! when the wake phrase "Hey Pairion" is detected with sufficient confidence.
//!
//! In M1, wake-word detection is implemented as a simple energy-based
//! detector for development testing. The full openWakeWord ONNX model
//! integration is wired when model files are available from the Server.
//!
//! False-wake suppression: a wake event within 500ms of a previous
//! one is suppressed.

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

/// Energy-based wake-word detector for development testing.
///
/// Fires when the audio energy exceeds a threshold, simulating wake-word
/// detection. This is replaced by the ONNX-based openWakeWord detector
/// when model files are available.
pub struct EnergyWakeDetector {
    sensitivity: f64,
    last_detection: Option<Instant>,
    /// Minimum interval between detections (false-wake suppression).
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
        // Higher sensitivity → lower threshold → easier to trigger
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

        // Compute RMS energy
        let energy: f32 =
            (samples.iter().map(|s| s * s).sum::<f32>() / samples.len() as f32).sqrt();

        if energy > self.threshold() {
            // False-wake suppression
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
            WakeWordResult::Detected { confidence } => {
                assert!(confidence > 0.0);
            }
            WakeWordResult::None => panic!("Expected detection on loud audio"),
        }
    }

    #[test]
    fn test_energy_detector_false_wake_suppression() {
        let mut det = EnergyWakeDetector::new(0.6);
        let loud = vec![0.5f32; 320];

        // First detection should succeed
        assert!(matches!(
            det.process(&loud),
            WakeWordResult::Detected { .. }
        ));
        // Immediate second detection should be suppressed
        assert_eq!(det.process(&loud), WakeWordResult::None);
    }

    #[test]
    fn test_energy_detector_reset() {
        let mut det = EnergyWakeDetector::new(0.6);
        let loud = vec![0.5f32; 320];
        let _ = det.process(&loud);
        det.reset();
        // After reset, should detect again
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
}
