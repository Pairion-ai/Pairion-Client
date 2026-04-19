#pragma once

/**
 * @file pairion_audio_playback.h
 * @brief QAudioSink wrapper with jitter buffer for inbound Opus audio.
 *
 * Decodes incoming Opus frames via PairionOpusDecoder, buffers the resulting
 * PCM through a lightweight jitter queue, and writes to QAudioSink (16 kHz
 * mono 16-bit). Emits speakingStateChanged("speaking") when the first PCM
 * frame arrives and speakingStateChanged("idle") after a silence timeout or
 * explicit stream-end notification.
 */

#include <QAudioSink>
#include <QIODevice>
#include <QObject>
#include <QQueue>
#include <QTimer>

namespace pairion::audio {

class PairionOpusDecoder;

/**
 * @brief QAudioSink-backed playback engine for inbound server audio.
 *
 * Lives on the main thread. Call start() to open the audio sink before the
 * first frame arrives. Frames arrive via handleOpusFrame() (from the WebSocket
 * thread via queued connection). stop() drains the jitter buffer, stops the
 * sink, and emits speakingStateChanged("idle") if currently speaking.
 */
class PairionAudioPlayback : public QObject {
    Q_OBJECT
  public:
    /**
     * @brief Construct the playback engine.
     * @param parent QObject parent.
     */
    explicit PairionAudioPlayback(QObject *parent = nullptr);

    ~PairionAudioPlayback() override;

    /**
     * @brief Open the QAudioSink and prepare for playback.
     *
     * Idempotent — safe to call multiple times.
     */
    void start();

    /**
     * @brief Stop the sink, drain the jitter buffer, and reset speaking state.
     *
     * Emits speakingStateChanged("idle") if currently speaking.
     */
    void stop();

    /**
     * @brief Pre-warm the QAudioSink buffer before the first Opus frame arrives.
     *
     * Writes one jitter-buffer worth of silence to the OS audio pipeline so that
     * the first decoded PCM frame does not underrun. Call this on AudioStreamStart.
     */
    void preparePlayback();

  public slots:
    /**
     * @brief Decode an inbound Opus frame and queue the resulting PCM.
     * @param opusFrame Compressed Opus frame bytes (no stream-ID prefix).
     */
    void handleOpusFrame(const QByteArray &opusFrame);

    /**
     * @brief Write a decoded PCM frame directly to the audio sink.
     * @param pcm Raw 16-bit signed mono PCM at 16 kHz.
     */
    void handlePcmFrame(const QByteArray &pcm);

    /**
     * @brief Notify the playback engine that the inbound stream has ended.
     * @param reason Termination reason string (e.g. "normal", "timeout").
     *
     * Calls stop(). Emits playbackError() if reason is not "normal".
     */
    void handleStreamEnd(const QString &reason);

  signals:
    /// Emitted when a playback error occurs (decode failure or abnormal stream end).
    void playbackError(const QString &message);
    /// Emitted when speaking state transitions between "speaking" and "idle".
    void speakingStateChanged(const QString &state);

  private:
    Q_DISABLE_COPY(PairionAudioPlayback)
    void initAudioSink();

    QAudioSink *m_sink = nullptr;
    QIODevice *m_audioDevice = nullptr;
    PairionOpusDecoder *m_decoder = nullptr;
    QQueue<QByteArray> m_jitterBuffer;
    QTimer *m_silenceTimer = nullptr;
    bool m_isSpeaking = false;

    static constexpr int kJitterMs = 50;
    static constexpr int kSampleRate = 48000;
};

} // namespace pairion::audio
