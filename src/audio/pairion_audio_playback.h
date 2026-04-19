#pragma once

/**
 * @file pairion_audio_playback.h
 * @brief QAudioSink wrapper with jitter buffer for inbound Opus audio.
 */

#include <QAudioSink>
#include <QIODevice>
#include <QObject>
#include <QQueue>
#include <QTimer>

namespace pairion::audio {

class PairionOpusDecoder;

class PairionAudioPlayback : public QObject {
    Q_OBJECT
  public:
    explicit PairionAudioPlayback(QObject *parent = nullptr);
    ~PairionAudioPlayback() override;

    void start();
    void stop();

  public slots:
    void handleOpusFrame(const QByteArray &opusFrame);
    void handlePcmFrame(const QByteArray &pcm);
    void handleStreamEnd(const QString &reason);

  signals:
    void playbackError(const QString &message);
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
    static constexpr int kSampleRate = 16000;
};

} // namespace pairion::audio