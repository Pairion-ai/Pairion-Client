#pragma once

/**
 * @file audio_session_orchestrator.h
 * @brief Central state machine coordinating the audio pipeline.
 *
 * Manages the lifecycle: Idle → AwaitingWake → Streaming → EndingSpeech → Idle.
 * Coordinates wake detection, VAD, Opus encoding, and WebSocket binary streaming.
 * Lives on the main thread; receives signals from worker threads via queued connections.
 */

#include "../audio/pairion_audio_capture.h"
#include "../audio/pairion_audio_playback.h"
#include "../audio/pairion_opus_encoder.h"
#include "../state/connection_state.h"
#include "../vad/silero_vad.h"
#include "../wake/open_wakeword_detector.h"
#include "../ws/pairion_websocket_client.h"

#include <QObject>
#include <QString>
#include <QTimer>

namespace pairion::pipeline {

/**
 * @brief Orchestrates the audio pipeline state machine.
 *
 * All dependencies are injected via constructor. All slots run on the main thread
 * (via Qt::QueuedConnection from worker threads), so m_state is accessed
 * single-threaded without synchronization.
 */
class AudioSessionOrchestrator : public QObject {
    Q_OBJECT
  public:
    /**
     * @brief Pipeline states.
     *
     * Idle          — quiescent, no capture or inference active.
     * AwaitingWake  — listening for the wake word.
     * Streaming     — user is speaking; audio is being encoded and sent.
     * EndingSpeech  — reserved for future graceful-close flow.
     * PlayingBack   — Pairion is speaking; VAD runs at barge-in threshold.
     */
    enum class State { Idle, AwaitingWake, Streaming, EndingSpeech, PlayingBack };

    /**
     * @brief Construct the orchestrator with all pipeline dependencies.
     */
    AudioSessionOrchestrator(
        pairion::audio::PairionAudioCapture *capture, pairion::audio::PairionOpusEncoder *encoder,
        pairion::wake::OpenWakewordDetector *wakeDetector, pairion::vad::SileroVad *vad,
        pairion::ws::PairionWebSocketClient *wsClient, pairion::state::ConnectionState *connState,
        pairion::audio::PairionAudioPlayback *playback = nullptr, QObject *parent = nullptr);

    /// @brief Current pipeline state.
    State state() const;

  public slots:
    /// @brief Start the pipeline and begin listening for the wake word.
    void startListening();
    /// @brief Stop all pipeline activity.
    void shutdown();

    /**
     * @brief Override the barge-in minimum duration timer interval.
     *
     * The default is pairion::kBargeInMinDurationMs (400 ms). Tests set this
     * to a small value (e.g. 1 ms) to exercise the barge-in path without blocking.
     * @param ms Duration in milliseconds.
     */
    void setBargeInTimerIntervalMs(int ms);

  signals:
    /// Emitted when the wake word fires and a new stream starts.
    void wakeFired(const QString &streamId);
    /// Emitted when speech ends and the stream closes.
    void streamEnded(const QString &streamId);
    /// Emitted on pipeline errors.
    void pipelineError(const QString &message);

  private slots:
    void onWakeWordDetected(float score, const QByteArray &preRollBuffer);
    void onVadSpeechStarted();
    void onSpeechEnded();
    void onOpusFrameEncoded(const QByteArray &opusFrame);
    void onStreamingTimeout();
    void onInboundAudio(const QByteArray &binaryFrame);
    void onInboundAudioStreamStart(const QString &streamId);
    void onInboundStreamEnd(const QString &streamId, const QString &reason);
    void onTtsPlaybackStarted();
    /**
     * @brief Handles the natural end of TTS playback (speakingStateChanged("idle")).
     *
     * Cancels any pending barge-in timer, restores the normal VAD threshold,
     * resets VAD state, and resumes wake word listening.
     */
    void onTtsPlaybackEnded();
    /**
     * @brief Called when the barge-in minimum-duration timer expires.
     *
     * The user has been speaking for at least kBargeInMinDurationMs — this is
     * a confirmed barge-in.  Delegates to executeBargeIn().
     */
    void onBargeInTimerExpired();
    /**
     * @brief Resets the pipeline to Idle when the WebSocket connection is lost.
     *
     * Stops any in-progress stream and stops listening. Listening resumes via
     * onWsReconnected() when the session is re-established.
     */
    void onWsDisconnected();
    /**
     * @brief Resumes listening after the WebSocket session is re-established.
     * @param sessionId New session identifier (unused).
     * @param serverVersion Server version string (unused).
     */
    void onWsReconnected(const QString &sessionId, const QString &serverVersion);
    /**
     * @brief Central error handler for all pipeline component failures.
     *
     * Logs the reason, stops any active stream, transitions to Idle, and emits
     * pipelineError so the UI can surface the fault to the user.
     * @param reason Human-readable error description.
     */
    void onPipelineError(const QString &reason);

  private:
    void transitionTo(State newState);
    void endStream(const QString &reason);
    /**
     * @brief Execute a confirmed barge-in: stop playback, send BargeIn, start new stream.
     *
     * Called from onBargeInTimerExpired() once the under-breath filter is satisfied.
     * Stops TTS playback, sends the BargeIn message with the interrupted stream ID,
     * opens a new AudioStreamStart upload stream, sends pre-roll audio, and transitions
     * to Streaming so the user's interruption is captured and forwarded to the server.
     */
    void executeBargeIn();

    pairion::audio::PairionAudioCapture *m_capture;
    pairion::audio::PairionOpusEncoder *m_encoder;
    pairion::audio::PairionOpusEncoder *m_preRollEncoder;
    pairion::audio::PairionAudioPlayback *m_playback;
    pairion::wake::OpenWakewordDetector *m_wakeDetector;
    pairion::vad::SileroVad *m_vad;
    pairion::ws::PairionWebSocketClient *m_wsClient;
    pairion::state::ConnectionState *m_connState;

    State m_state = State::Idle;
    QString m_activeStreamId;
    /// Stream ID of the most recent inbound (server→client) audio stream.
    /// Stored in onInboundAudioStreamStart() and sent in the BargeIn message.
    QString m_inboundStreamId;
    QTimer m_streamingTimeout;
    QTimer m_bargeInTimer;
    /// Barge-in filter duration in ms (default kBargeInMinDurationMs; override via setBargeInTimerIntervalMs()).
    int m_bargeInTimerIntervalMs;
    /// Normal VAD threshold stored when entering PlayingBack; restored on exit.
    double m_normalVadThreshold;

    static constexpr int kStreamingTimeoutMs = 30000;
};

} // namespace pairion::pipeline
