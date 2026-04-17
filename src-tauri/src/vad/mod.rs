//! Voice Activity Detection (VAD) for the Pairion Client.
//!
//! Provides a trait-based abstraction for detecting end-of-speech.
//! The VAD runs on the post-wake audio stream and triggers when
//! 800ms of sustained silence is detected, signaling the Client
//! to send `AudioStreamEnd` + `SpeechEnded` messages.
//!
//! In M1, VAD is implemented as a simple energy-based silence detector.
//! The full Silero VAD ONNX model integration is wired when model
//! files are available.

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

/// Energy-based VAD that detects end-of-speech after sustained silence.
///
/// Speech is determined by RMS energy above a threshold. Once energy
/// drops below the threshold, a silence timer starts. If silence
/// persists for `silence_threshold_ms`, `VadResult::EndOfSpeech` is returned.
pub struct EnergyVad {
    /// Energy threshold below which audio is considered silence.
    energy_threshold: f32,
    /// Duration of silence required to trigger end-of-speech.
    silence_threshold_ms: u64,
    /// When silence started, if currently silent.
    silence_start: Option<Instant>,
    /// Whether end-of-speech has already been fired for this session.
    fired: bool,
}

impl EnergyVad {
    /// Creates a new energy-based VAD.
    ///
    /// - `energy_threshold`: RMS energy below which audio is silence (default 0.01).
    /// - `silence_threshold_ms`: how long silence must persist (default 800ms).
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
            // Speech detected — reset silence timer
            self.silence_start = None;
            VadResult::Speech
        } else {
            // Silence detected
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
        // Silence timer should have reset
        assert_eq!(vad.process(&silence), VadResult::Silence);
    }

    #[test]
    fn test_energy_vad_reset() {
        let mut vad = EnergyVad::default();
        let silence = vec![0.001f32; 320];
        let _ = vad.process(&silence);
        vad.reset();
        // After reset, should start fresh
        assert_eq!(vad.process(&silence), VadResult::Silence);
    }

    #[test]
    fn test_energy_vad_empty_input() {
        let mut vad = EnergyVad::default();
        // Empty input after fire should return EndOfSpeech
        assert_eq!(vad.process(&[]), VadResult::EndOfSpeech);
    }

    #[test]
    fn test_energy_vad_end_of_speech_sticks() {
        let mut vad = EnergyVad::new(0.01, 0); // 0ms threshold = immediate
        let silence = vec![0.001f32; 320];
        // First silence starts timer, second should trigger end
        let _ = vad.process(&silence);
        // With 0ms threshold, next call triggers immediately
        assert_eq!(vad.process(&silence), VadResult::EndOfSpeech);
        // Once fired, stays fired
        assert_eq!(vad.process(&[0.5; 320]), VadResult::EndOfSpeech);
    }

    #[test]
    fn test_always_speech_vad() {
        let mut vad = AlwaysSpeechVad;
        assert_eq!(vad.process(&[0.0; 320]), VadResult::Speech);
        assert_eq!(vad.silence_threshold_ms(), 800);
    }

    #[test]
    fn test_immediate_end_vad() {
        let mut vad = ImmediateEndVad;
        assert_eq!(vad.process(&[0.5; 320]), VadResult::EndOfSpeech);
    }
}
