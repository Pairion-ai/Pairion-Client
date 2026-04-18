#pragma once

/**
 * @file pairion_opus_encoder.h
 * @brief Opus encoder wrapping libopus for 16 kHz mono voice encoding.
 *
 * Takes 20 ms PCM frames (640 bytes = 320 int16 samples at 16 kHz mono)
 * and produces variable-length Opus encoded frames. Designed for worker-thread
 * deployment via moveToThread.
 */

#include <QByteArray>
#include <QObject>

struct OpusEncoder;

namespace pairion::audio {

/**
 * @brief QObject wrapper around a libopus encoder.
 *
 * Configured for VOIP application at 28 kbps, complexity 5.
 * One encoder per stream — not thread-safe; use on a single thread.
 */
class PairionOpusEncoder : public QObject {
    Q_OBJECT
  public:
    /// @brief Construct and initialize the Opus encoder.
    explicit PairionOpusEncoder(QObject *parent = nullptr);
    ~PairionOpusEncoder() override;

    /// @brief Whether the encoder initialized successfully.
    bool isValid() const;

  public slots:
    /**
     * @brief Encode a 20 ms PCM frame to Opus.
     * @param pcm20ms 640 bytes of 16-bit signed mono PCM at 16 kHz.
     */
    void encodePcmFrame(const QByteArray &pcm20ms);

  signals:
    /// Emitted with the Opus-encoded frame after successful encoding.
    void opusFrameEncoded(const QByteArray &opusFrame);
    /// Emitted if encoding fails.
    void encoderError(const QString &reason);

  private:
    OpusEncoder *m_encoder = nullptr;
    static constexpr int kSampleRate = 16000;
    static constexpr int kChannels = 1;
    static constexpr int kFrameSamples = 320; ///< 20 ms at 16 kHz
    static constexpr int kFrameBytes = 640;   ///< 320 samples * 2 bytes
    static constexpr int kBitrate = 28000;
    static constexpr int kComplexity = 5;
    static constexpr int kMaxPacketBytes = 4000;
};

} // namespace pairion::audio
