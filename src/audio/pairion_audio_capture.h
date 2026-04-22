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

    /// @brief Configured device ID (empty = system default).
    QString configuredDeviceId() const { return m_configuredDeviceId; }
    /// @brief Configured sample rate in Hz.
    int configuredSampleRate() const { return m_configuredSampleRate; }

  public slots:
    /// @brief Start capturing audio from the microphone.
    void start();
    /// @brief Stop capturing audio.
    void stop();
    /**
     * @brief Configure the capture device and sample rate before start().
     * @param deviceId Device name string from QMediaDevices::audioInputs().
     *        Empty string selects the system default input.
     * @param sampleRate Target sample rate in Hz. Falls back to 16000 if the
     *        device does not support the requested rate.
     *
     * If capture is already running, it is stopped and restarted immediately.
     */
    void configure(const QString &deviceId, int sampleRate);

  signals:
    /// Emitted for each 20 ms PCM frame (always 640 bytes).
    void audioFrameAvailable(const QByteArray &pcm20ms);
    /// Emitted on microphone access failure or device disconnection.
    void captureError(const QString &reason);

  private slots:
    void onAudioDataReady();
    /**
     * @brief Reacts to QAudioSource state transitions during active capture.
     *
     * Emits captureError if the device unexpectedly stops with a non-zero error code.
     * No-ops during intentional stop (m_running is false by that point).
     * @param state New QAudioSource state.
     */
    void onAudioSourceStateChanged(QAudio::State state);

  private:
    void processAccumulator();

    QAudioSource *m_audioSource = nullptr;
    QIODevice *m_ioDevice = nullptr;
    QByteArray m_accumulator;
    bool m_ownsSource = false;
    bool m_running = false;

    QString m_configuredDeviceId;
    int m_configuredSampleRate = 16000;

    static constexpr int kSampleRate = 16000;
    static constexpr int kChannels = 1;
    static constexpr int kSampleSize = 16;
    static constexpr int kFrameBytes = 640; ///< 320 samples * 2 bytes
};

} // namespace pairion::audio
