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
     * ConversationWaiting is entered after speech ends while conversation mode is active.
     * The VAD remains listening; the next speechStarted re-opens the stream without a wake word.
     */
    enum class State { Idle, AwaitingWake, Streaming, EndingSpeech, ConversationWaiting };

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

  signals:
    /// Emitted when the wake word fires and a new stream starts.
    void wakeFired(const QString &streamId);
    /// Emitted when speech ends and the stream closes.
    void streamEnded(const QString &streamId);
    /// Emitted on pipeline errors.
    void pipelineError(const QString &message);

  private slots:
    void onWakeWordDetected(float score, const QByteArray &preRollBuffer);
    void onSpeechStarted();
    void onSpeechEnded();
    void onOpusFrameEncoded(const QByteArray &opusFrame);
    void onStreamingTimeout();
    void onConversationEnded();
    void onConversationIdleTimeout();
    void onInboundAudio(const QByteArray &binaryFrame);
    void onInboundAudioStreamStart(const QString &streamId);
    void onInboundStreamEnd(const QString &streamId, const QString &reason);
    void onTtsPlaybackStarted();

  private:
    void transitionTo(State newState);
    void endStream(const QString &reason);

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
    QTimer m_streamingTimeout;
    bool m_conversationActive = false;
    QTimer m_conversationIdleTimer;

    static constexpr int kStreamingTimeoutMs = 30000;
    static constexpr int kConversationIdleTimeoutMs = 30000;
};

} // namespace pairion::pipeline
