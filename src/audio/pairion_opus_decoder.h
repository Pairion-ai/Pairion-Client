#pragma once

/**
 * @file pairion_opus_decoder.h
 * @brief Opus decoder wrapping libopus for 16 kHz mono voice decoding.
 *
 * Takes Opus-encoded frames and produces 16-bit signed mono PCM at 16 kHz.
 * Used for round-trip testing at M1 and real playback at PC-003.
 */

#include <QByteArray>
#include <QObject>

struct OpusDecoder;

namespace pairion::audio {

/**
 * @brief QObject wrapper around a libopus decoder.
 *
 * Decodes Opus frames to 16 kHz mono 16-bit PCM.
 */
class PairionOpusDecoder : public QObject {
    Q_OBJECT
  public:
    /// @brief Construct and initialize the Opus decoder.
    explicit PairionOpusDecoder(QObject *parent = nullptr);
    ~PairionOpusDecoder() override;

    /// @brief Whether the decoder initialized successfully.
    bool isValid() const;

  public slots:
    /**
     * @brief Decode an Opus frame to PCM.
     * @param opusFrame Opus-encoded frame bytes.
     */
    void decodeOpusFrame(const QByteArray &opusFrame);

  signals:
    /// Emitted with decoded PCM data.
    void pcmFrameDecoded(const QByteArray &pcm);
    /// Emitted if decoding fails.
    void decoderError(const QString &reason);

  private:
    OpusDecoder *m_decoder = nullptr;
    static constexpr int kSampleRate = 48000;
    static constexpr int kChannels = 1;
    static constexpr int kMaxFrameSamples = 5760; ///< 120 ms max at 48 kHz
};

} // namespace pairion::audio
