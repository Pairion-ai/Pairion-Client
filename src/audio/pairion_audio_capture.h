#pragma once

/**
 * @file pairion_audio_capture.h
 * @brief Microphone capture at 16 kHz mono 16-bit via QAudioSource.
 *
 * Wraps QAudioSource and emits 20 ms PCM frames (640 bytes) through the
 * audioFrameAvailable signal. Uses an internal accumulator to produce
 * exact-size frames from QAudioSource's variable-size push cadence.
 *
 * Threading: this object MUST live on the main thread because QAudioSource
 * and QMediaDevices require main-thread access on macOS (AVFoundation
 * backend). Downstream consumers (encoder, wake, VAD) on worker threads
 * receive frames via Qt::QueuedConnection automatically.
 */

#include <QAudioSource>
#include <QByteArray>
#include <QIODevice>
#include <QObject>

namespace pairion::audio {

/**
 * @brief Audio capture QObject producing 20 ms PCM frames.
 *
 * Must live on the main thread. Call start()/stop() directly or via signal/slot.
 * Downstream consumers on worker threads receive audioFrameAvailable via
 * queued connections.
 */
class PairionAudioCapture : public QObject {
    Q_OBJECT
  public:
    /// @brief Construct with default system audio input.
    explicit PairionAudioCapture(QObject *parent = nullptr);

    /**
     * @brief Construct with an externally-provided audio device (for testing).
     * @param audioDevice QIODevice to read PCM data from. Not owned.
     */
    explicit PairionAudioCapture(QIODevice *audioDevice, QObject *parent = nullptr);

    ~PairionAudioCapture() override;

  public slots:
    /// @brief Start capturing audio from the microphone.
    void start();
    /// @brief Stop capturing audio.
    void stop();

  signals:
    /// Emitted for each 20 ms PCM frame (always 640 bytes).
    void audioFrameAvailable(const QByteArray &pcm20ms);
    /// Emitted on microphone access failure or device disconnection.
    void captureError(const QString &reason);

  private slots:
    void onAudioDataReady();

  private:
    void processAccumulator();

    QAudioSource *m_audioSource = nullptr;
    QIODevice *m_ioDevice = nullptr;
    QByteArray m_accumulator;
    bool m_ownsSource = false;
    bool m_running = false;

    static constexpr int kSampleRate = 16000;
    static constexpr int kChannels = 1;
    static constexpr int kSampleSize = 16;
    static constexpr int kFrameBytes = 640; ///< 320 samples * 2 bytes
};

} // namespace pairion::audio
