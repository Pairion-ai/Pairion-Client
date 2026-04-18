/**
 * @file tst_binary_frame.cpp
 * @brief Tests for the BinaryCodec: encode/decode round-trip and UUID prefix extraction.
 */

#include "../src/protocol/binary_codec.h"

#include <QTest>
#include <QUuid>

using namespace pairion::protocol;

class TestBinaryFrame : public QObject {
    Q_OBJECT

  private slots:
    /// Verify encode/decode round-trip preserves payload.
    void encodeDecodeRoundTrip() {
        QString streamId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QByteArray payload("opus_frame_data_here");

        QByteArray frame = BinaryCodec::encodeBinaryFrame(streamId, payload);
        QCOMPARE(frame.size(), BinaryCodec::kPrefixLength + payload.size());

        DecodedBinaryFrame decoded = BinaryCodec::decodeBinaryFrame(frame);
        QCOMPARE(decoded.prefix.size(), BinaryCodec::kPrefixLength);
        QCOMPARE(decoded.payload, payload);
    }

    /// Verify prefix is first 4 bytes of UUID RFC 4122 representation.
    void prefixIsFirst4BytesOfUuid() {
        QString streamId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QByteArray expected = QUuid::fromString(streamId).toRfc4122().left(4);
        QByteArray prefix = BinaryCodec::streamIdToPrefix(streamId);
        QCOMPARE(prefix, expected);
    }

    /// Verify empty payload round-trips correctly.
    void emptyPayloadRoundTrip() {
        QString streamId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QByteArray frame = BinaryCodec::encodeBinaryFrame(streamId, QByteArray());
        QCOMPARE(frame.size(), BinaryCodec::kPrefixLength);

        DecodedBinaryFrame decoded = BinaryCodec::decodeBinaryFrame(frame);
        QVERIFY(decoded.payload.isEmpty());
        QCOMPARE(decoded.prefix.size(), BinaryCodec::kPrefixLength);
    }

    /// Verify decode of frame shorter than 4 bytes returns empty result.
    void decodeTooShortFrame() {
        DecodedBinaryFrame decoded = BinaryCodec::decodeBinaryFrame(QByteArray("ab"));
        QVERIFY(decoded.prefix.isEmpty());
        QVERIFY(decoded.payload.isEmpty());
    }

    /// Verify null UUID produces zero prefix.
    void nullUuidProducesZeroPrefix() {
        QByteArray prefix = BinaryCodec::streamIdToPrefix(QStringLiteral("not-a-uuid"));
        QCOMPARE(prefix.size(), BinaryCodec::kPrefixLength);
        QCOMPARE(prefix, QByteArray(4, '\0'));
    }

    /// Verify same stream ID always produces same prefix.
    void deterministicPrefix() {
        QString id = QStringLiteral("550e8400-e29b-41d4-a716-446655440000");
        QByteArray p1 = BinaryCodec::streamIdToPrefix(id);
        QByteArray p2 = BinaryCodec::streamIdToPrefix(id);
        QCOMPARE(p1, p2);
    }
};

QTEST_GUILESS_MAIN(TestBinaryFrame)
#include "tst_binary_frame.moc"
