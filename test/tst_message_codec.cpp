/**
 * @file tst_message_codec.cpp
 * @brief Unit tests for the EnvelopeCodec: round-trip serialization of every message type.
 */

#include "../src/protocol/envelope_codec.h"
#include "../src/protocol/messages.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using namespace pairion::protocol;

class TestMessageCodec : public QObject {
    Q_OBJECT

  private slots:
    /// Verify DeviceIdentify serializes all required fields.
    void serializeDeviceIdentify() {
        DeviceIdentify msg;
        msg.deviceId = QStringLiteral("dev-123");
        msg.bearerToken = QStringLiteral("tok-abc");
        msg.clientVersion = QStringLiteral("0.1.0");

        QJsonObject obj = EnvelopeCodec::serialize(msg);
        QCOMPARE(obj["type"].toString(), QStringLiteral("DeviceIdentify"));
        QCOMPARE(obj["deviceId"].toString(), QStringLiteral("dev-123"));
        QCOMPARE(obj["bearerToken"].toString(), QStringLiteral("tok-abc"));
        QCOMPARE(obj["clientVersion"].toString(), QStringLiteral("0.1.0"));
    }

    /// Verify HeartbeatPing serializes correctly.
    void serializeHeartbeatPing() {
        HeartbeatPing msg;
        msg.timestamp = QStringLiteral("2026-04-18T12:00:00.000Z");

        QJsonObject obj = EnvelopeCodec::serialize(msg);
        QCOMPARE(obj["type"].toString(), QStringLiteral("HeartbeatPing"));
        QCOMPARE(obj["timestamp"].toString(), QStringLiteral("2026-04-18T12:00:00.000Z"));
    }

    /// Verify WakeWordDetected serializes with optional confidence.
    void serializeWakeWordDetected() {
        WakeWordDetected msg;
        msg.timestamp = QStringLiteral("2026-04-18T12:00:00.000Z");
        msg.confidence = 0.85;

        QJsonObject obj = EnvelopeCodec::serialize(msg);
        QCOMPARE(obj["type"].toString(), QStringLiteral("WakeWordDetected"));
        QCOMPARE(obj["timestamp"].toString(), QStringLiteral("2026-04-18T12:00:00.000Z"));
        QVERIFY(obj.contains("confidence"));
        QCOMPARE(obj["confidence"].toDouble(), 0.85);
    }

    /// Verify WakeWordDetected without confidence omits the field.
    void serializeWakeWordDetectedNoConfidence() {
        WakeWordDetected msg;
        msg.timestamp = QStringLiteral("2026-04-18T12:00:00.000Z");

        QJsonObject obj = EnvelopeCodec::serialize(msg);
        QCOMPARE(obj["type"].toString(), QStringLiteral("WakeWordDetected"));
        QVERIFY(!obj.contains("confidence"));
    }

    /// Verify AudioStreamStartIn serializes with correct type discriminator.
    void serializeAudioStreamStartIn() {
        AudioStreamStartIn msg;
        msg.streamId = QStringLiteral("stream-1");

        QJsonObject obj = EnvelopeCodec::serialize(msg);
        QCOMPARE(obj["type"].toString(), QStringLiteral("AudioStreamStart"));
        QCOMPARE(obj["streamId"].toString(), QStringLiteral("stream-1"));
        QCOMPARE(obj["codec"].toString(), QStringLiteral("opus"));
        QCOMPARE(obj["sampleRate"].toInt(), 16000);
    }

    /// Verify SpeechEnded serializes correctly.
    void serializeSpeechEnded() {
        SpeechEnded msg;
        msg.streamId = QStringLiteral("stream-1");

        QJsonObject obj = EnvelopeCodec::serialize(msg);
        QCOMPARE(obj["type"].toString(), QStringLiteral("SpeechEnded"));
        QCOMPARE(obj["streamId"].toString(), QStringLiteral("stream-1"));
    }

    /// Verify AudioStreamEndIn serializes with correct type discriminator.
    void serializeAudioStreamEndIn() {
        AudioStreamEndIn msg;
        msg.streamId = QStringLiteral("stream-1");
        msg.reason = QStringLiteral("normal");

        QJsonObject obj = EnvelopeCodec::serialize(msg);
        QCOMPARE(obj["type"].toString(), QStringLiteral("AudioStreamEnd"));
        QCOMPARE(obj["streamId"].toString(), QStringLiteral("stream-1"));
        QCOMPARE(obj["reason"].toString(), QStringLiteral("normal"));
    }

    /// Verify TextMessage serializes correctly.
    void serializeTextMessage() {
        TextMessage msg;
        msg.text = QStringLiteral("Hello Pairion");

        QJsonObject obj = EnvelopeCodec::serialize(msg);
        QCOMPARE(obj["type"].toString(), QStringLiteral("TextMessage"));
        QCOMPARE(obj["text"].toString(), QStringLiteral("Hello Pairion"));
    }

    /// Verify BargeIn serializes type and interruptedStreamId.
    void serializeBargeIn() {
        BargeIn msg;
        msg.interruptedStreamId = QStringLiteral("out-stream-42");

        QJsonObject obj = EnvelopeCodec::serialize(msg);
        QCOMPARE(obj["type"].toString(), QStringLiteral("BargeIn"));
        QCOMPARE(obj["interruptedStreamId"].toString(), QStringLiteral("out-stream-42"));
    }

    /// Verify BargeIn with empty interruptedStreamId still serializes the field.
    void serializeBargeInEmptyStreamId() {
        BargeIn msg;
        msg.interruptedStreamId = QStringLiteral("");

        QJsonObject obj = EnvelopeCodec::serialize(msg);
        QCOMPARE(obj["type"].toString(), QStringLiteral("BargeIn"));
        QVERIFY(obj.contains(QStringLiteral("interruptedStreamId")));
        QCOMPARE(obj["interruptedStreamId"].toString(), QStringLiteral(""));
    }

    /// Verify serializeToString produces valid compact JSON.
    void serializeToStringProducesValidJson() {
        HeartbeatPing ping;
        ping.timestamp = QStringLiteral("2026-04-18T12:00:00.000Z");

        QString json = EnvelopeCodec::serializeToString(ping);
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        QVERIFY(!doc.isNull());
        QVERIFY(doc.isObject());
        QCOMPARE(doc.object()["type"].toString(), QStringLiteral("HeartbeatPing"));
    }

    /// Verify SessionOpened deserializes correctly.
    void deserializeSessionOpened() {
        QJsonObject obj;
        obj["type"] = QStringLiteral("SessionOpened");
        obj["sessionId"] = QStringLiteral("sess-42");
        obj["serverVersion"] = QStringLiteral("2.0.0");

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<SessionOpened>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->sessionId, QStringLiteral("sess-42"));
        QCOMPARE(msg->serverVersion, QStringLiteral("2.0.0"));
    }

    /// Verify SessionClosed deserializes correctly.
    void deserializeSessionClosed() {
        QJsonObject obj;
        obj["type"] = QStringLiteral("SessionClosed");
        obj["reason"] = QStringLiteral("server_shutdown");

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<SessionClosed>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->reason, QStringLiteral("server_shutdown"));
    }

    /// Verify HeartbeatPong deserializes correctly.
    void deserializeHeartbeatPong() {
        QJsonObject obj;
        obj["type"] = QStringLiteral("HeartbeatPong");
        obj["timestamp"] = QStringLiteral("2026-04-18T12:00:00.000Z");

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<HeartbeatPong>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->timestamp, QStringLiteral("2026-04-18T12:00:00.000Z"));
    }

    /// Verify Error message deserializes correctly.
    void deserializeError() {
        QJsonObject obj;
        obj["type"] = QStringLiteral("Error");
        obj["code"] = QStringLiteral("AUTH_FAILED");
        obj["message"] = QStringLiteral("Invalid token");

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<ErrorMessage>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->code, QStringLiteral("AUTH_FAILED"));
        QCOMPARE(msg->message, QStringLiteral("Invalid token"));
    }

    /// Verify AgentStateChange deserializes correctly.
    void deserializeAgentStateChange() {
        QJsonObject obj;
        obj["type"] = QStringLiteral("AgentStateChange");
        obj["state"] = QStringLiteral("thinking");

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<AgentStateChange>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->state, QStringLiteral("thinking"));
    }

    /// Verify AgentStateChange with "INTERRUPTED" state passes through as a plain string.
    /// The state field is stringly typed so no enum change is needed for new server states.
    void deserializeAgentStateChangeInterrupted() {
        QJsonObject obj;
        obj["type"] = QStringLiteral("AgentStateChange");
        obj["state"] = QStringLiteral("INTERRUPTED");

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<AgentStateChange>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->state, QStringLiteral("INTERRUPTED"));
    }

    /// Verify TranscriptPartial deserializes correctly.
    void deserializeTranscriptPartial() {
        QJsonObject obj;
        obj["type"] = QStringLiteral("TranscriptPartial");
        obj["text"] = QStringLiteral("Hello wor");

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<TranscriptPartial>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->text, QStringLiteral("Hello wor"));
    }

    /// Verify TranscriptFinal deserializes correctly.
    void deserializeTranscriptFinal() {
        QJsonObject obj;
        obj["type"] = QStringLiteral("TranscriptFinal");
        obj["text"] = QStringLiteral("Hello world");

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<TranscriptFinal>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->text, QStringLiteral("Hello world"));
    }

    /// Verify LlmTokenStream deserializes correctly.
    void deserializeLlmTokenStream() {
        QJsonObject obj;
        obj["type"] = QStringLiteral("LlmTokenStream");
        obj["delta"] = QStringLiteral("The ");

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<LlmTokenStream>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->delta, QStringLiteral("The "));
    }

    /// Verify ToolCallStarted deserializes with nested input object.
    void deserializeToolCallStarted() {
        QJsonObject input;
        input["query"] = QStringLiteral("weather");
        QJsonObject obj;
        obj["type"] = QStringLiteral("ToolCallStarted");
        obj["toolCallId"] = QStringLiteral("tc-1");
        obj["toolName"] = QStringLiteral("search");
        obj["input"] = input;

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<ToolCallStarted>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->toolCallId, QStringLiteral("tc-1"));
        QCOMPARE(msg->toolName, QStringLiteral("search"));
        QCOMPARE(msg->input["query"].toString(), QStringLiteral("weather"));
    }

    /// Verify ToolCallCompleted deserializes with nested output object.
    void deserializeToolCallCompleted() {
        QJsonObject output;
        output["result"] = QStringLiteral("sunny");
        QJsonObject obj;
        obj["type"] = QStringLiteral("ToolCallCompleted");
        obj["toolCallId"] = QStringLiteral("tc-1");
        obj["output"] = output;

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<ToolCallCompleted>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->toolCallId, QStringLiteral("tc-1"));
        QCOMPARE(msg->output["result"].toString(), QStringLiteral("sunny"));
    }

    /// Verify AudioStreamStartOut deserializes with shared "AudioStreamStart" type.
    void deserializeAudioStreamStartOut() {
        QJsonObject obj;
        obj["type"] = QStringLiteral("AudioStreamStart");
        obj["streamId"] = QStringLiteral("out-1");
        obj["codec"] = QStringLiteral("opus");
        obj["sampleRate"] = 24000;

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<AudioStreamStartOut>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->streamId, QStringLiteral("out-1"));
        QCOMPARE(msg->codec, QStringLiteral("opus"));
        QCOMPARE(msg->sampleRate, 24000);
    }

    /// Verify AudioStreamEndOut deserializes with shared "AudioStreamEnd" type.
    void deserializeAudioStreamEndOut() {
        QJsonObject obj;
        obj["type"] = QStringLiteral("AudioStreamEnd");
        obj["streamId"] = QStringLiteral("out-1");
        obj["reason"] = QStringLiteral("interrupted");

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<AudioStreamEndOut>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->streamId, QStringLiteral("out-1"));
        QCOMPARE(msg->reason, QStringLiteral("interrupted"));
    }

    /// Verify UnderBreathAck deserializes with optional acknowledgementType.
    void deserializeUnderBreathAck() {
        QJsonObject obj;
        obj["type"] = QStringLiteral("UnderBreathAck");
        obj["acknowledgementType"] = QStringLiteral("mm");

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<UnderBreathAck>(&result.value());
        QVERIFY(msg != nullptr);
        QVERIFY(msg->acknowledgementType.has_value());
        QCOMPARE(msg->acknowledgementType.value(), QStringLiteral("mm"));
    }

    /// Verify UnderBreathAck deserializes without optional acknowledgementType.
    void deserializeUnderBreathAckNoType() {
        QJsonObject obj;
        obj["type"] = QStringLiteral("UnderBreathAck");

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<UnderBreathAck>(&result.value());
        QVERIFY(msg != nullptr);
        QVERIFY(!msg->acknowledgementType.has_value());
    }

    /// Verify MapFocus deserializes all fields correctly.
    void deserializeMapFocus() {
        QJsonObject obj;
        obj["type"] = QStringLiteral("MapFocus");
        obj["lat"]   = 35.6762;
        obj["lon"]   = 139.6503;
        obj["label"] = QStringLiteral("Tokyo, Japan");
        obj["zoom"]  = QStringLiteral("city");

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<MapFocus>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->lat, 35.6762);
        QCOMPARE(msg->lon, 139.6503);
        QCOMPARE(msg->label, QStringLiteral("Tokyo, Japan"));
        QCOMPARE(msg->zoom, QStringLiteral("city"));
    }

    /// Verify MapClear deserializes.
    void deserializeMapClear() {
        QJsonObject obj;
        obj["type"] = QStringLiteral("MapClear");

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<MapClear>(&result.value());
        QVERIFY(msg != nullptr);
    }

    /// Verify BackgroundChange deserializes all fields correctly.
    void deserializeBackgroundChange() {
        QJsonObject params;
        params["center_lat"] = 33.814;
        params["center_lon"] = -96.582;
        QJsonObject obj;
        obj["type"]         = QStringLiteral("BackgroundChange");
        obj["backgroundId"] = QStringLiteral("globe");
        obj["params"]       = params;
        obj["transition"]   = QStringLiteral("crossfade");

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<BackgroundChange>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->backgroundId, QStringLiteral("globe"));
        QCOMPARE(msg->transition, QStringLiteral("crossfade"));
        QCOMPARE(msg->params[QStringLiteral("center_lat")].toDouble(), 33.814);
    }

    /// Verify OverlayAdd deserializes overlayId and params.
    void deserializeOverlayAdd() {
        QJsonObject params;
        params["radius_nm"] = 8;
        QJsonObject obj;
        obj["type"]      = QStringLiteral("OverlayAdd");
        obj["overlayId"] = QStringLiteral("adsb");
        obj["params"]    = params;

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<OverlayAdd>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->overlayId, QStringLiteral("adsb"));
        QCOMPARE(msg->params[QStringLiteral("radius_nm")].toInt(), 8);
    }

    /// Verify OverlayRemove deserializes overlayId correctly.
    void deserializeOverlayRemove() {
        QJsonObject obj;
        obj["type"]      = QStringLiteral("OverlayRemove");
        obj["overlayId"] = QStringLiteral("adsb");

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<OverlayRemove>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->overlayId, QStringLiteral("adsb"));
    }

    /// Verify SceneDataPush deserializes modelId and object data correctly.
    void deserializeSceneDataPush() {
        QJsonObject data;
        data["headline"] = QStringLiteral("Market Volatility");
        QJsonObject obj;
        obj["type"]    = QStringLiteral("SceneDataPush");
        obj["modelId"] = QStringLiteral("news");
        obj["data"]    = data;

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<SceneDataPush>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->modelId, QStringLiteral("news"));
        QCOMPARE(msg->data[QStringLiteral("headline")].toString(),
                 QStringLiteral("Market Volatility"));
    }

    /// Verify SceneDataPush preserves a JSON array payload (e.g. ADS-B aircraft list).
    void deserializeSceneDataPushWithArray() {
        QJsonObject aircraft;
        aircraft["icao24"]   = QStringLiteral("abc123");
        aircraft["callsign"] = QStringLiteral("UAL123");
        QJsonArray array;
        array.append(aircraft);

        QJsonObject obj;
        obj["type"]    = QStringLiteral("SceneDataPush");
        obj["modelId"] = QStringLiteral("adsb");
        obj["data"]    = array;

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<SceneDataPush>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->modelId, QStringLiteral("adsb"));
        QVERIFY(msg->data.isArray());
        QJsonArray stored = msg->data.toArray();
        QCOMPARE(stored.size(), 1);
        QCOMPARE(stored[0].toObject()[QStringLiteral("icao24")].toString(),
                 QStringLiteral("abc123"));
    }

    /// Verify OverlayClear deserializes.
    void deserializeOverlayClear() {
        QJsonObject obj;
        obj["type"] = QStringLiteral("OverlayClear");

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<OverlayClear>(&result.value());
        QVERIFY(msg != nullptr);
    }

    /// Verify unknown type returns nullopt.
    void deserializeUnknownType() {
        QJsonObject obj;
        obj["type"] = QStringLiteral("SomeFutureType");

        auto result = EnvelopeCodec::deserialize(obj);
        QVERIFY(!result.has_value());
    }

    /// Verify malformed JSON string returns nullopt.
    void deserializeMalformedJson() {
        auto result = EnvelopeCodec::deserializeFromString(QStringLiteral("{invalid json"));
        QVERIFY(!result.has_value());
    }

    /// Verify non-object JSON returns nullopt.
    void deserializeNonObjectJson() {
        auto result = EnvelopeCodec::deserializeFromString(QStringLiteral("[1,2,3]"));
        QVERIFY(!result.has_value());
    }

    /// Verify deserializeFromString with valid JSON works end-to-end.
    void deserializeFromStringValid() {
        QString json =
            QStringLiteral(R"({"type":"SessionOpened","sessionId":"s1","serverVersion":"1.0"})");
        auto result = EnvelopeCodec::deserializeFromString(json);
        QVERIFY(result.has_value());
        auto *msg = std::get_if<SessionOpened>(&result.value());
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->sessionId, QStringLiteral("s1"));
    }
};

QTEST_GUILESS_MAIN(TestMessageCodec)
#include "tst_message_codec.moc"
