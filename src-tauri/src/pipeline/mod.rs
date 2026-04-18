//! Audio session orchestrator for the Pairion Client.
//!
//! Coordinates the per-session audio pipeline: capture consumption,
//! Opus encoding, outbound binary-frame sending, inbound binary-frame
//! receiving, Opus decoding, and playback. Uses an actor-style design
//! with a dedicated Tokio task holding the pipeline state, resolving
//! ownership across task boundaries.

use crate::audio::{CaptureConfig, PairionOpusDecoder, PairionOpusEncoder};
use crate::state::{AgentState, SessionState};
use crate::ws::{OutboundMessage, OutboundSender};
use std::sync::Arc;
use std::time::Instant;
use tokio::sync::{mpsc, watch};
use tracing::{debug, error, info, warn};

/// Commands that external systems send to the orchestrator.
#[derive(Debug)]
pub enum OrchestratorCommand {
    /// Wake word fired — start streaming including pre-roll buffer.
    StartFromWake {
        /// The allocated stream identifier.
        stream_id: String,
        /// Pre-wake buffered audio samples (~200ms).
        pre_roll: Vec<f32>,
    },
    /// Manual session start from Tauri command — begin capture from now.
    StartManual {
        /// The allocated stream identifier.
        stream_id: String,
    },
    /// Stop the current session.
    StopCurrent,
    /// Inbound binary audio frame from the Server (TTS).
    InboundAudio {
        /// The stream this frame belongs to.
        stream_id: String,
        /// Opus-encoded audio data.
        opus_frame: Vec<u8>,
    },
    /// Server finished sending audio.
    InboundStreamEnd {
        /// The stream being ended.
        stream_id: String,
        /// End reason: "normal", "interrupted", "error".
        reason: String,
    },
    /// Inbound PCM samples from the capture ring buffer (fed by a reader task).
    CapturedAudio {
        /// Raw PCM samples at 16kHz mono.
        samples: Vec<f32>,
    },
    /// VAD detected end-of-speech.
    VadEndOfSpeech,
    /// Application is closing.
    Shutdown,
}

/// Handle for sending commands to the orchestrator.
#[derive(Debug, Clone)]
pub struct OrchestratorHandle {
    tx: mpsc::Sender<OrchestratorCommand>,
}

impl OrchestratorHandle {
    /// Creates a test-only handle from a raw sender. Does not spawn a task.
    #[cfg(test)]
    pub fn new_test(tx: mpsc::Sender<OrchestratorCommand>) -> Self {
        Self { tx }
    }

    /// Sends a command to the orchestrator.
    pub async fn send(&self, cmd: OrchestratorCommand) -> Result<(), OrchestratorError> {
        self.tx
            .send(cmd)
            .await
            .map_err(|_| OrchestratorError::ChannelClosed)
    }

    /// Tries to send a command without waiting.
    pub fn try_send(&self, cmd: OrchestratorCommand) -> Result<(), OrchestratorError> {
        self.tx
            .try_send(cmd)
            .map_err(|_| OrchestratorError::ChannelClosed)
    }
}

/// State of the active capture session within the orchestrator.
struct ActiveSession {
    /// Stream identifier for correlation.
    stream_id: String,
    /// Opus encoder for outbound audio.
    encoder: PairionOpusEncoder,
    /// PCM sample accumulator for frame-aligned encoding.
    encode_buffer: Vec<f32>,
    /// Timestamp of session start for latency measurement.
    start_time: Instant,
    /// Whether the first opus frame has been sent.
    first_frame_sent: bool,
}

/// Creates the orchestrator command channel without spawning the task.
///
/// Returns the handle (for sending commands) and the receiver (for the task).
/// Call [`spawn_orchestrator_task`] inside a live Tokio runtime to start
/// the orchestrator's event loop. This two-step construction avoids the
/// "no reactor running" panic that occurs when `tokio::spawn` is called
/// during synchronous Tauri app initialization before the runtime exists.
pub fn create_orchestrator_channel() -> (OrchestratorHandle, mpsc::Receiver<OrchestratorCommand>) {
    let (cmd_tx, cmd_rx) = mpsc::channel::<OrchestratorCommand>(64);
    (OrchestratorHandle { tx: cmd_tx }, cmd_rx)
}

/// Spawns the orchestrator event loop as a Tokio task.
///
/// Must be called from within a live Tokio runtime (e.g., inside
/// `tauri::Builder::setup()`). Consumes the receiver half of the
/// command channel created by [`create_orchestrator_channel`].
pub fn spawn_orchestrator_task(
    cmd_rx: mpsc::Receiver<OrchestratorCommand>,
    outbound_tx: OutboundSender,
    session_tx: Arc<watch::Sender<SessionState>>,
) {
    tauri::async_runtime::spawn(async move {
        run_orchestrator(cmd_rx, outbound_tx, session_tx).await;
    });
}

/// Legacy API: Creates the channel AND spawns the task in one call.
///
/// Only usable inside a live Tokio runtime. Retained for test convenience.
#[cfg(test)]
pub fn spawn_orchestrator(
    outbound_tx: OutboundSender,
    session_tx: Arc<watch::Sender<SessionState>>,
) -> OrchestratorHandle {
    let (handle, cmd_rx) = create_orchestrator_channel();

    tokio::spawn(async move {
        run_orchestrator(cmd_rx, outbound_tx, session_tx).await;
    });

    handle
}

/// The orchestrator's main event loop.
async fn run_orchestrator(
    mut cmd_rx: mpsc::Receiver<OrchestratorCommand>,
    outbound_tx: OutboundSender,
    session_tx: Arc<watch::Sender<SessionState>>,
) {
    let mut active_session: Option<ActiveSession> = None;
    let mut decoder: Option<PairionOpusDecoder> = None;
    let capture_config = CaptureConfig::default();
    let frame_samples = capture_config.frame_samples();

    info!("Audio session orchestrator started");

    while let Some(cmd) = cmd_rx.recv().await {
        match cmd {
            OrchestratorCommand::StartFromWake {
                stream_id,
                pre_roll,
            } => {
                let start_time = Instant::now();
                info!(
                    stream_id = %stream_id,
                    pre_roll_samples = pre_roll.len(),
                    event = "wake.detected",
                    "Starting session from wake word"
                );

                // Send WakeWordDetected
                let wake_msg = crate::ws::WakeWordDetectedPayload {
                    r#type: "WakeWordDetected".to_string(),
                    confidence: 0.9,
                    snr_db: None,
                    timestamp: chrono::Utc::now().to_rfc3339(),
                };
                if let Ok(msg) = OutboundMessage::json(&wake_msg) {
                    let _ = outbound_tx.send(msg).await;
                }

                // Send AudioStreamStart
                let stream_start = crate::ws::AudioStreamStartPayload {
                    r#type: "AudioStreamStart".to_string(),
                    session_id: stream_id.clone(),
                    stream_id: stream_id.clone(),
                    direction: "in".to_string(),
                    codec: "opus".to_string(),
                    sample_rate: 16000,
                    channels: 1,
                    timestamp: Some(chrono::Utc::now().to_rfc3339()),
                };
                if let Ok(msg) = OutboundMessage::json(&stream_start) {
                    let _ = outbound_tx.send(msg).await;
                }
                info!(event = "stream.start", stream_id = %stream_id, "Audio stream started");

                // Create encoder and seed with pre-roll
                match PairionOpusEncoder::new() {
                    Ok(encoder) => {
                        let mut session = ActiveSession {
                            stream_id,
                            encoder,
                            encode_buffer: pre_roll,
                            start_time,
                            first_frame_sent: false,
                        };
                        // Encode any complete frames from pre-roll
                        encode_and_send(&mut session, &outbound_tx, frame_samples).await;

                        session_tx.send_modify(|s| {
                            s.session_id = Some(session.stream_id.clone());
                            s.agent_state = AgentState::Listening;
                            s.active = true;
                            s.current_stream_id = Some(session.stream_id.clone());
                        });

                        active_session = Some(session);
                    }
                    Err(e) => {
                        error!(error = %e, "Failed to create Opus encoder");
                    }
                }
            }

            OrchestratorCommand::StartManual { stream_id } => {
                info!(stream_id = %stream_id, "Starting manual session");

                let stream_start = crate::ws::AudioStreamStartPayload {
                    r#type: "AudioStreamStart".to_string(),
                    session_id: stream_id.clone(),
                    stream_id: stream_id.clone(),
                    direction: "in".to_string(),
                    codec: "opus".to_string(),
                    sample_rate: 16000,
                    channels: 1,
                    timestamp: Some(chrono::Utc::now().to_rfc3339()),
                };
                if let Ok(msg) = OutboundMessage::json(&stream_start) {
                    let _ = outbound_tx.send(msg).await;
                }

                match PairionOpusEncoder::new() {
                    Ok(encoder) => {
                        let session = ActiveSession {
                            stream_id: stream_id.clone(),
                            encoder,
                            encode_buffer: Vec::new(),
                            start_time: Instant::now(),
                            first_frame_sent: false,
                        };

                        session_tx.send_modify(|s| {
                            s.session_id = Some(stream_id);
                            s.agent_state = AgentState::Listening;
                            s.active = true;
                        });

                        active_session = Some(session);
                    }
                    Err(e) => {
                        error!(error = %e, "Failed to create Opus encoder");
                    }
                }
            }

            OrchestratorCommand::CapturedAudio { samples } => {
                if let Some(session) = &mut active_session {
                    session.encode_buffer.extend_from_slice(&samples);
                    encode_and_send(session, &outbound_tx, frame_samples).await;
                }
            }

            OrchestratorCommand::VadEndOfSpeech => {
                if let Some(session) = active_session.take() {
                    info!(
                        stream_id = %session.stream_id,
                        "VAD end-of-speech — ending audio stream"
                    );

                    // Send AudioStreamEnd
                    let stream_end = crate::ws::AudioStreamEndPayload {
                        r#type: "AudioStreamEnd".to_string(),
                        session_id: session.stream_id.clone(),
                        stream_id: session.stream_id.clone(),
                        reason: Some("normal".to_string()),
                        timestamp: chrono::Utc::now().to_rfc3339(),
                    };
                    if let Ok(msg) = OutboundMessage::json(&stream_end) {
                        let _ = outbound_tx.send(msg).await;
                    }

                    // Send SpeechEnded
                    let speech_ended = crate::ws::SpeechEndedPayload {
                        r#type: "SpeechEnded".to_string(),
                        session_id: session.stream_id.clone(),
                        stream_id: session.stream_id.clone(),
                        timestamp: chrono::Utc::now().to_rfc3339(),
                    };
                    if let Ok(msg) = OutboundMessage::json(&speech_ended) {
                        let _ = outbound_tx.send(msg).await;
                    }

                    // Transition to thinking while we wait for Server response
                    session_tx.send_modify(|s| {
                        s.agent_state = AgentState::Thinking;
                    });
                }
            }

            OrchestratorCommand::InboundAudio {
                stream_id,
                opus_frame,
            } => {
                // Lazy-init decoder on first inbound audio
                if decoder.is_none() {
                    match PairionOpusDecoder::new(24000) {
                        Ok(d) => {
                            info!(
                                event = "tts.first_chunk",
                                stream_id = %stream_id,
                                "First TTS audio chunk received"
                            );
                            decoder = Some(d);
                        }
                        Err(e) => {
                            error!(error = %e, "Failed to create Opus decoder");
                            continue;
                        }
                    }
                }

                if let Some(dec) = &mut decoder {
                    match dec.decode(&opus_frame) {
                        Ok(pcm) => {
                            debug!(
                                samples = pcm.len(),
                                stream_id = %stream_id,
                                "Decoded TTS audio"
                            );
                            // PCM is ready for the playback jitter buffer.
                            // In the full pipeline, this writes to AudioPlaybackManager.
                            // The playback manager would be started here if not already.
                            info!(
                                event = "playback.first",
                                samples = pcm.len(),
                                stream_id = %stream_id,
                                "Audio decoded for playback"
                            );
                        }
                        Err(e) => {
                            warn!(error = %e, "Failed to decode TTS audio");
                        }
                    }
                }
            }

            OrchestratorCommand::InboundStreamEnd { stream_id, reason } => {
                info!(
                    stream_id = %stream_id,
                    reason = %reason,
                    "Inbound audio stream ended"
                );

                if reason == "interrupted" {
                    // M4 barge-in: flush playback immediately.
                    // M1: just log and proceed without crashing.
                    debug!("Interrupted stream — graceful handling (full barge-in is M4)");
                }

                // Reset decoder for next stream
                decoder = None;

                // Transition back to idle
                session_tx.send_modify(|s| {
                    s.agent_state = AgentState::Idle;
                    s.active = false;
                    s.session_id = None;
                    s.current_stream_id = None;
                    s.partial_transcript.clear();
                });
            }

            OrchestratorCommand::StopCurrent => {
                if let Some(session) = active_session.take() {
                    info!(stream_id = %session.stream_id, "Stopping current session");

                    let stream_end = crate::ws::AudioStreamEndPayload {
                        r#type: "AudioStreamEnd".to_string(),
                        session_id: session.stream_id.clone(),
                        stream_id: session.stream_id.clone(),
                        reason: Some("normal".to_string()),
                        timestamp: chrono::Utc::now().to_rfc3339(),
                    };
                    if let Ok(msg) = OutboundMessage::json(&stream_end) {
                        let _ = outbound_tx.send(msg).await;
                    }
                }

                decoder = None;
                session_tx.send_modify(|s| {
                    *s = SessionState::default();
                });
            }

            OrchestratorCommand::Shutdown => {
                info!("Orchestrator shutting down");
                drop(active_session);
                drop(decoder);
                break;
            }
        }
    }

    info!("Audio session orchestrator stopped");
}

/// Encodes complete frames from the buffer and sends them as binary WS frames.
async fn encode_and_send(
    session: &mut ActiveSession,
    outbound_tx: &OutboundSender,
    frame_samples: usize,
) {
    while session.encode_buffer.len() >= frame_samples {
        let frame: Vec<f32> = session.encode_buffer.drain(..frame_samples).collect();
        match session.encoder.encode(&frame) {
            Ok(opus_data) => {
                if !session.first_frame_sent {
                    session.first_frame_sent = true;
                    info!(
                        event = "stream.first_frame",
                        elapsed_ms = session.start_time.elapsed().as_millis(),
                        bytes = opus_data.len(),
                        "First Opus frame encoded and sending"
                    );
                }
                let _ = outbound_tx.send(OutboundMessage::binary(opus_data)).await;
            }
            Err(e) => {
                warn!(error = %e, "Opus encode failed");
            }
        }
    }
}

/// Errors from the orchestrator.
#[derive(Debug, thiserror::Error)]
pub enum OrchestratorError {
    /// The orchestrator command channel is closed.
    #[error("orchestrator channel closed")]
    ChannelClosed,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn test_orchestrator_handle_send() {
        let (outbound_tx, _outbound_rx) = crate::ws::create_outbound_channel();
        let (session_tx, _session_rx) = watch::channel(SessionState::default());
        let handle = spawn_orchestrator(outbound_tx, Arc::new(session_tx));

        let result = handle
            .send(OrchestratorCommand::StartManual {
                stream_id: "test-stream".to_string(),
            })
            .await;
        assert!(result.is_ok());

        // Give orchestrator time to process
        tokio::time::sleep(tokio::time::Duration::from_millis(50)).await;

        let _ = handle.send(OrchestratorCommand::Shutdown).await;
    }

    #[tokio::test]
    async fn test_orchestrator_manual_session_updates_state() {
        let (outbound_tx, _outbound_rx) = crate::ws::create_outbound_channel();
        let (session_tx, session_rx) = watch::channel(SessionState::default());
        let session_tx = Arc::new(session_tx);
        let handle = spawn_orchestrator(outbound_tx, session_tx);

        handle
            .send(OrchestratorCommand::StartManual {
                stream_id: "test-stream".to_string(),
            })
            .await
            .unwrap();

        tokio::time::sleep(tokio::time::Duration::from_millis(50)).await;

        let state = session_rx.borrow().clone();
        assert!(state.active);
        assert_eq!(state.agent_state, AgentState::Listening);
        assert_eq!(state.session_id.as_deref(), Some("test-stream"));

        let _ = handle.send(OrchestratorCommand::Shutdown).await;
    }

    #[tokio::test]
    async fn test_orchestrator_stop_resets_state() {
        let (outbound_tx, _outbound_rx) = crate::ws::create_outbound_channel();
        let (session_tx, session_rx) = watch::channel(SessionState::default());
        let session_tx = Arc::new(session_tx);
        let handle = spawn_orchestrator(outbound_tx, session_tx);

        handle
            .send(OrchestratorCommand::StartManual {
                stream_id: "s1".to_string(),
            })
            .await
            .unwrap();
        tokio::time::sleep(tokio::time::Duration::from_millis(50)).await;

        handle.send(OrchestratorCommand::StopCurrent).await.unwrap();
        tokio::time::sleep(tokio::time::Duration::from_millis(50)).await;

        let state = session_rx.borrow().clone();
        assert!(!state.active);
        assert_eq!(state.agent_state, AgentState::Idle);

        let _ = handle.send(OrchestratorCommand::Shutdown).await;
    }

    #[tokio::test]
    async fn test_orchestrator_captured_audio_encodes() {
        let (outbound_tx, mut outbound_rx) = crate::ws::create_outbound_channel();
        let (session_tx, _session_rx) = watch::channel(SessionState::default());
        let handle = spawn_orchestrator(outbound_tx, Arc::new(session_tx));

        handle
            .send(OrchestratorCommand::StartManual {
                stream_id: "s1".to_string(),
            })
            .await
            .unwrap();
        tokio::time::sleep(tokio::time::Duration::from_millis(50)).await;

        // Send enough audio for one Opus frame (320 samples at 16kHz = 20ms)
        let audio = vec![0.1f32; 320];
        handle
            .send(OrchestratorCommand::CapturedAudio { samples: audio })
            .await
            .unwrap();
        tokio::time::sleep(tokio::time::Duration::from_millis(100)).await;

        // Should have received: AudioStreamStart (JSON) + AudioChunkIn (binary)
        let mut json_count = 0;
        let mut binary_count = 0;
        while let Some(msg) = outbound_rx.try_recv() {
            match msg {
                OutboundMessage::Json(_) => json_count += 1,
                OutboundMessage::Binary(_) => binary_count += 1,
            }
        }
        assert!(json_count >= 1, "Should have sent AudioStreamStart JSON");
        assert!(binary_count >= 1, "Should have sent AudioChunkIn binary");

        let _ = handle.send(OrchestratorCommand::Shutdown).await;
    }

    #[tokio::test]
    async fn test_orchestrator_vad_end_sends_messages() {
        let (outbound_tx, mut outbound_rx) = crate::ws::create_outbound_channel();
        let (session_tx, session_rx) = watch::channel(SessionState::default());
        let session_tx = Arc::new(session_tx);
        let handle = spawn_orchestrator(outbound_tx, session_tx);

        handle
            .send(OrchestratorCommand::StartManual {
                stream_id: "s1".to_string(),
            })
            .await
            .unwrap();
        tokio::time::sleep(tokio::time::Duration::from_millis(50)).await;

        handle
            .send(OrchestratorCommand::VadEndOfSpeech)
            .await
            .unwrap();
        tokio::time::sleep(tokio::time::Duration::from_millis(50)).await;

        // Should transition to Thinking
        assert_eq!(session_rx.borrow().agent_state, AgentState::Thinking);

        // Should have sent AudioStreamEnd + SpeechEnded
        let mut messages = Vec::new();
        while let Some(msg) = outbound_rx.try_recv() {
            if let OutboundMessage::Json(text) = msg {
                messages.push(text);
            }
        }
        let has_stream_end = messages.iter().any(|m| m.contains("\"AudioStreamEnd\""));
        let has_speech_ended = messages.iter().any(|m| m.contains("\"SpeechEnded\""));
        assert!(has_stream_end, "Should have sent AudioStreamEnd");
        assert!(has_speech_ended, "Should have sent SpeechEnded");

        let _ = handle.send(OrchestratorCommand::Shutdown).await;
    }

    #[tokio::test]
    async fn test_orchestrator_inbound_stream_end_resets() {
        let (outbound_tx, _outbound_rx) = crate::ws::create_outbound_channel();
        let (session_tx, session_rx) = watch::channel(SessionState::default());
        let session_tx = Arc::new(session_tx);
        let handle = spawn_orchestrator(outbound_tx, session_tx);

        handle
            .send(OrchestratorCommand::StartManual {
                stream_id: "s1".to_string(),
            })
            .await
            .unwrap();
        tokio::time::sleep(tokio::time::Duration::from_millis(50)).await;

        handle
            .send(OrchestratorCommand::InboundStreamEnd {
                stream_id: "s1".to_string(),
                reason: "normal".to_string(),
            })
            .await
            .unwrap();
        tokio::time::sleep(tokio::time::Duration::from_millis(50)).await;

        let state = session_rx.borrow().clone();
        assert!(!state.active);
        assert_eq!(state.agent_state, AgentState::Idle);

        let _ = handle.send(OrchestratorCommand::Shutdown).await;
    }

    #[tokio::test]
    async fn test_orchestrator_interrupted_stream_no_crash() {
        let (outbound_tx, _outbound_rx) = crate::ws::create_outbound_channel();
        let (session_tx, _session_rx) = watch::channel(SessionState::default());
        let handle = spawn_orchestrator(outbound_tx, Arc::new(session_tx));

        // Should not crash on interrupted stream end
        handle
            .send(OrchestratorCommand::InboundStreamEnd {
                stream_id: "s1".to_string(),
                reason: "interrupted".to_string(),
            })
            .await
            .unwrap();
        tokio::time::sleep(tokio::time::Duration::from_millis(50)).await;

        let _ = handle.send(OrchestratorCommand::Shutdown).await;
    }

    #[tokio::test]
    async fn test_orchestrator_wake_session() {
        let (outbound_tx, mut outbound_rx) = crate::ws::create_outbound_channel();
        let (session_tx, session_rx) = watch::channel(SessionState::default());
        let session_tx = Arc::new(session_tx);
        let handle = spawn_orchestrator(outbound_tx, session_tx);

        let pre_roll = vec![0.05f32; 3200]; // 200ms at 16kHz
        handle
            .send(OrchestratorCommand::StartFromWake {
                stream_id: "wake-s1".to_string(),
                pre_roll,
            })
            .await
            .unwrap();
        tokio::time::sleep(tokio::time::Duration::from_millis(100)).await;

        // Should be listening
        assert_eq!(session_rx.borrow().agent_state, AgentState::Listening);

        // Should have sent WakeWordDetected + AudioStreamStart + binary frames
        let mut json_msgs = Vec::new();
        let mut binary_count = 0;
        while let Some(msg) = outbound_rx.try_recv() {
            match msg {
                OutboundMessage::Json(text) => json_msgs.push(text),
                OutboundMessage::Binary(_) => binary_count += 1,
            }
        }
        assert!(
            json_msgs.iter().any(|m| m.contains("WakeWordDetected")),
            "Should have sent WakeWordDetected"
        );
        assert!(
            json_msgs.iter().any(|m| m.contains("AudioStreamStart")),
            "Should have sent AudioStreamStart"
        );
        // Pre-roll of 3200 samples = 10 frames of 320 → should have sent binary frames
        assert!(
            binary_count >= 1,
            "Should have sent binary frames from pre-roll"
        );

        let _ = handle.send(OrchestratorCommand::Shutdown).await;
    }
}
