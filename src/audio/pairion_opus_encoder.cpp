/**
 * @file pairion_opus_encoder.cpp
 * @brief Implementation of the Opus encoder wrapper.
 */

#include "pairion_opus_encoder.h"

#include <opus/opus.h>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcOpusEnc, "pairion.audio.opus.enc")

namespace pairion::audio {

PairionOpusEncoder::PairionOpusEncoder(QObject *parent) : QObject(parent) {
    int err = 0;
    m_encoder = opus_encoder_create(kSampleRate, kChannels, OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK || m_encoder == nullptr) {
        // LCOV_EXCL_START — only reachable if libopus itself is broken
        qCCritical(lcOpusEnc) << "Failed to create Opus encoder:" << opus_strerror(err);
        return;
        // LCOV_EXCL_STOP
    }
    opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(kBitrate));
    opus_encoder_ctl(m_encoder, OPUS_SET_COMPLEXITY(kComplexity));
    opus_encoder_ctl(m_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    qCInfo(lcOpusEnc) << "Opus encoder created:" << kBitrate << "bps, complexity" << kComplexity;
}

PairionOpusEncoder::~PairionOpusEncoder() {
    if (m_encoder != nullptr) {
        opus_encoder_destroy(m_encoder);
    }
}

bool PairionOpusEncoder::isValid() const {
    return m_encoder != nullptr;
}

void PairionOpusEncoder::encodePcmFrame(const QByteArray &pcm20ms) {
    // LCOV_EXCL_START — only reachable if constructor failed (libopus broken); always valid in tests
    if (m_encoder == nullptr) {
        emit encoderError(QStringLiteral("Encoder not initialized"));
        return;
    }
    // LCOV_EXCL_STOP
    if (pcm20ms.size() != kFrameBytes) {
        emit encoderError(QStringLiteral("Invalid PCM frame size: %1 (expected %2)")
                              .arg(pcm20ms.size())
                              .arg(kFrameBytes));
        return;
    }

    const auto *pcmData = reinterpret_cast<const opus_int16 *>(pcm20ms.constData());
    unsigned char packet[kMaxPacketBytes];
    opus_int32 bytesWritten =
        opus_encode(m_encoder, pcmData, kFrameSamples, packet, kMaxPacketBytes);

    if (bytesWritten < 0) {
        // LCOV_EXCL_START — encoding errors on valid input shouldn't happen
        emit encoderError(QStringLiteral("Opus encode error: %1")
                              .arg(QString::fromUtf8(opus_strerror(bytesWritten))));
        return;
        // LCOV_EXCL_STOP
    }

    emit opusFrameEncoded(QByteArray(reinterpret_cast<const char *>(packet), bytesWritten));
}

} // namespace pairion::audio
