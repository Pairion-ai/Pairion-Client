/**
 * @file tst_opus_codec.cpp
 * @brief Tests for PairionOpusEncoder and PairionOpusDecoder: round-trip encoding.
 */

#include "../src/audio/pairion_opus_decoder.h"
#include "../src/audio/pairion_opus_encoder.h"

#include <QSignalSpy>
#include <QTest>

using namespace pairion::audio;

class TestOpusCodec : public QObject {
    Q_OBJECT

  private slots:
    /// Verify encoder initializes successfully.
    void encoderInitializes() {
        PairionOpusEncoder encoder;
        QVERIFY(encoder.isValid());
    }

    /// Verify decoder initializes successfully.
    void decoderInitializes() {
        PairionOpusDecoder decoder;
        QVERIFY(decoder.isValid());
    }

    /// Verify encoding 640 bytes of silence produces non-empty output.
    void encodeProducesOutput() {
        PairionOpusEncoder encoder;
        QSignalSpy frameSpy(&encoder, &PairionOpusEncoder::opusFrameEncoded);
        QSignalSpy errorSpy(&encoder, &PairionOpusEncoder::encoderError);

        QByteArray silence(640, '\0');
        encoder.encodePcmFrame(silence);

        QCOMPARE(errorSpy.count(), 0);
        QCOMPARE(frameSpy.count(), 1);
        QByteArray opusFrame = frameSpy.at(0).at(0).toByteArray();
        QVERIFY(!opusFrame.isEmpty());
    }

    /// Verify decode produces correct sample count for a 20ms frame.
    void decodeProducesCorrectSampleCount() {
        PairionOpusEncoder encoder;
        PairionOpusDecoder decoder;

        QSignalSpy encSpy(&encoder, &PairionOpusEncoder::opusFrameEncoded);
        QByteArray silence(640, '\0');
        encoder.encodePcmFrame(silence);
        QCOMPARE(encSpy.count(), 1);

        QSignalSpy decSpy(&decoder, &PairionOpusDecoder::pcmFrameDecoded);
        QSignalSpy decErrSpy(&decoder, &PairionOpusDecoder::decoderError);
        decoder.decodeOpusFrame(encSpy.at(0).at(0).toByteArray());

        QCOMPARE(decErrSpy.count(), 0);
        QCOMPARE(decSpy.count(), 1);
        QByteArray pcm = decSpy.at(0).at(0).toByteArray();
        // 320 samples * 2 bytes = 640 bytes
        QCOMPARE(pcm.size(), 640);
    }

    /// Verify round-trip preserves audio length.
    void roundTripPreservesLength() {
        PairionOpusEncoder encoder;
        PairionOpusDecoder decoder;

        // Generate a simple sine-like pattern
        QByteArray pcm(640, '\0');
        auto *samples = reinterpret_cast<int16_t *>(pcm.data());
        for (int i = 0; i < 320; ++i) {
            samples[i] = static_cast<int16_t>(1000 * (i % 50 - 25));
        }

        QSignalSpy encSpy(&encoder, &PairionOpusEncoder::opusFrameEncoded);
        encoder.encodePcmFrame(pcm);
        QCOMPARE(encSpy.count(), 1);

        QSignalSpy decSpy(&decoder, &PairionOpusDecoder::pcmFrameDecoded);
        decoder.decodeOpusFrame(encSpy.at(0).at(0).toByteArray());
        QCOMPARE(decSpy.count(), 1);
        QCOMPARE(decSpy.at(0).at(0).toByteArray().size(), 640);
    }

    /// Verify encoder rejects wrong-size input.
    void encodeRejectsWrongSize() {
        PairionOpusEncoder encoder;
        QSignalSpy errorSpy(&encoder, &PairionOpusEncoder::encoderError);

        encoder.encodePcmFrame(QByteArray(100, '\0'));
        QCOMPARE(errorSpy.count(), 1);
    }

    /// Verify decoder rejects empty input.
    void decodeRejectsEmpty() {
        PairionOpusDecoder decoder;
        QSignalSpy errorSpy(&decoder, &PairionOpusDecoder::decoderError);

        decoder.decodeOpusFrame(QByteArray());
        QCOMPARE(errorSpy.count(), 1);
    }

    /// Verify decoder handles corrupt data gracefully.
    void decodeHandlesCorruptData() {
        PairionOpusDecoder decoder;
        QSignalSpy errorSpy(&decoder, &PairionOpusDecoder::decoderError);

        decoder.decodeOpusFrame(QByteArray("not_opus_data"));
        QCOMPARE(errorSpy.count(), 1);
    }
};

QTEST_GUILESS_MAIN(TestOpusCodec)
#include "tst_opus_codec.moc"
