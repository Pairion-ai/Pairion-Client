/**
 * @file pairion_opus_decoder.cpp
 * @brief Implementation of the Opus decoder wrapper.
 */

#include "pairion_opus_decoder.h"

#include <opus/opus.h>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcOpusDec, "pairion.audio.opus.dec")

namespace pairion::audio {

PairionOpusDecoder::PairionOpusDecoder(QObject *parent) : QObject(parent) {
    int err = 0;
    m_decoder = opus_decoder_create(kSampleRate, kChannels, &err);
    if (err != OPUS_OK || m_decoder == nullptr) {
        // LCOV_EXCL_START — only reachable if libopus itself is broken
        qCCritical(lcOpusDec) << "Failed to create Opus decoder:" << opus_strerror(err);
        return;
        // LCOV_EXCL_STOP
    }
    qCInfo(lcOpusDec) << "Opus decoder created:" << kSampleRate << "Hz," << kChannels << "ch";
}

PairionOpusDecoder::~PairionOpusDecoder() {
    if (m_decoder != nullptr) {
        opus_decoder_destroy(m_decoder);
    }
}

bool PairionOpusDecoder::isValid() const {
    return m_decoder != nullptr;
}

void PairionOpusDecoder::decodeOpusFrame(const QByteArray &opusFrame) {
    if (m_decoder == nullptr) {
        emit decoderError(QStringLiteral("Decoder not initialized"));
        return;
    }
    if (opusFrame.isEmpty()) {
        emit decoderError(QStringLiteral("Empty Opus frame"));
        return;
    }

    opus_int16 decoded[kMaxFrameSamples];
    int samplesDecoded =
        opus_decode(m_decoder, reinterpret_cast<const unsigned char *>(opusFrame.constData()),
                    opusFrame.size(), decoded, kMaxFrameSamples, 0);

    if (samplesDecoded < 0) {
        emit decoderError(QStringLiteral("Opus decode error: %1")
                              .arg(QString::fromUtf8(opus_strerror(samplesDecoded))));
        return;
    }

    int byteCount = samplesDecoded * static_cast<int>(sizeof(opus_int16));
    emit pcmFrameDecoded(QByteArray(reinterpret_cast<const char *>(decoded), byteCount));
}

} // namespace pairion::audio
