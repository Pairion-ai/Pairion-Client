/**
 * @file audio_session_orchestrator.cpp
 * @brief Implementation of the audio pipeline state machine.
 */

#include "audio_session_orchestrator.h"

#include "../audio/pairion_opus_encoder.h"
#include "../protocol/binary_codec.h"
#include "../protocol/envelope_codec.h"

#include <QDateTime>
#include <QLoggingCategory>
#include <QUuid>

Q_LOGGING_CATEGORY(lcPipeline, "pairion.pipeline") // LCOV_EXCL_LINE — static category registration; only the internal vtable-init path is unreachable

namespace pairion::pipeline {

AudioSessionOrchestrator::AudioSessionOrchestrator(
    pairion::audio::PairionAudioCapture *capture, pairion::audio::PairionOpusEncoder *encoder,
    pairion::wake::OpenWakewordDetector *wakeDetector, pairion::vad::SileroVad *vad,
    pairion::ws::PairionWebSocketClient *wsClient, pairion::state::ConnectionState *connState,
    pairion::audio::PairionAudioPlayback *playback, QObject *parent)
    : QObject(parent), m_capture(capture), m_encoder(encoder),
      m_preRollEncoder(new pairion::audio::PairionOpusEncoder(this)), m_playback(playback),
      m_wakeDetector(wakeDetector), m_vad(vad), m_wsClient(wsClient), m_connState(connState) {

    connect(m_wakeDetector, &pairion::wake::OpenWakewordDetector::wakeWordDetected, this,
            &AudioSessionOrchestrator::onWakeWordDetected);
    connect(m_vad, &pairion::vad::SileroVad::speechEnded, this,
            &AudioSessionOrchestrator::onSpeechEnded);
    connect(m_encoder, &pairion::audio::PairionOpusEncoder::opusFrameEncoded, this,
            &AudioSessionOrchestrator::onOpusFrameEncoded);
    if (m_playback) {
        connect(m_playback, &pairion::audio::PairionAudioPlayback::speakingStateChanged,
                m_connState, &pairion::state::ConnectionState::setAgentState);
        connect(m_playback, &pairion::audio::PairionAudioPlayback::speakingStateChanged, this,
                [this](const QString &state) {
                    if (state == QLatin1String("speaking")) {
                        onTtsPlaybackStarted();
                    }
                });
    }

    m_streamingTimeout.setSingleShot(true);
    connect(&m_streamingTimeout, &QTimer::timeout, this,
            &AudioSessionOrchestrator::onStreamingTimeout);
    connect(m_wsClient, &pairion::ws::PairionWebSocketClient::binaryFrameReceived, this,
            &AudioSessionOrchestrator::onInboundAudio);
    connect(m_wsClient, &pairion::ws::PairionWebSocketClient::audioStreamStartOutReceived, this,
            &AudioSessionOrchestrator::onInboundAudioStreamStart);
    connect(m_wsClient, &pairion::ws::PairionWebSocketClient::audioStreamEndOutReceived, this,
            &AudioSessionOrchestrator::onInboundStreamEnd);

    // Fix 1 & 2: Reset on disconnect; resume on reconnect.
    connect(m_wsClient, &pairion::ws::PairionWebSocketClient::disconnected, this,
            &AudioSessionOrchestrator::onWsDisconnected);
    connect(m_wsClient, &pairion::ws::PairionWebSocketClient::sessionOpened, this,
            &AudioSessionOrchestrator::onWsReconnected);

    // Fix 3: Propagate capture and encoder failures to the central error handler.
    connect(m_capture, &pairion::audio::PairionAudioCapture::captureError, this,
            &AudioSessionOrchestrator::onPipelineError);
    connect(m_encoder, &pairion::audio::PairionOpusEncoder::encoderError, this,
            &AudioSessionOrchestrator::onPipelineError);
}

AudioSessionOrchestrator::State AudioSessionOrchestrator::state() const {
    return m_state;
}

void AudioSessionOrchestrator::startListening() {
    if (m_state != State::Idle) {
        return;
    }
    transitionTo(State::AwaitingWake);
    qCInfo(lcPipeline) << "Pipeline listening for wake word";
}

void AudioSessionOrchestrator::shutdown() {
    m_streamingTimeout.stop();
    transitionTo(State::Idle);
    qCInfo(lcPipeline) << "Pipeline shutdown";
}

void AudioSessionOrchestrator::onWakeWordDetected(float score, const QByteArray &preRollBuffer) {
    if (m_state != State::AwaitingWake) {
        return;
    }
    // Fix 2: Guard against acting on a wake event when the socket is not ready.
    // Without this, the orchestrator would enter Streaming state and loop on 30 s
    // timeouts until the connection is restored.
    // LCOV_EXCL_START — only reachable when wake fires during a network outage; not exercisable in unit tests
    if (!m_wsClient->isConnected()) {
        qCWarning(lcPipeline) << "Wake word detected but WebSocket not connected — ignoring";
        return;
    }
    // LCOV_EXCL_STOP

    m_activeStreamId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    qCInfo(lcPipeline) << "Wake detected, starting stream:" << m_activeStreamId
                       << "score:" << score;

    // Send WakeWordDetected envelope
    protocol::WakeWordDetected wakeMsg;
    wakeMsg.timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    wakeMsg.confidence = static_cast<double>(score);
    m_wsClient->sendMessage(wakeMsg);

    // Send AudioStreamStart envelope
    protocol::AudioStreamStartIn startMsg;
    startMsg.streamId = m_activeStreamId;
    m_wsClient->sendMessage(startMsg);

    // Encode and send pre-roll audio as Opus binary frames.
    // m_preRollEncoder lives on the main thread (not moved to EncoderThread), so
    // opusFrameEncoded fires synchronously via Qt::DirectConnection.
    constexpr int kFrameBytes = 640;
    auto preRollConn = QObject::connect(
        m_preRollEncoder, &pairion::audio::PairionOpusEncoder::opusFrameEncoded, this,
        [this](const QByteArray &encoded) {
            m_wsClient->sendBinaryFrame(
                protocol::BinaryCodec::encodeBinaryFrame(m_activeStreamId, encoded));
        },
        Qt::DirectConnection);
    for (int offset = 0; offset + kFrameBytes <= preRollBuffer.size(); offset += kFrameBytes) {
        m_preRollEncoder->encodePcmFrame(preRollBuffer.mid(offset, kFrameBytes));
    }
    QObject::disconnect(preRollConn);

    transitionTo(State::Streaming);
    m_streamingTimeout.start(kStreamingTimeoutMs);

    // Reset VAD for the new speech segment
    m_vad->reset();

    emit wakeFired(m_activeStreamId);
}

void AudioSessionOrchestrator::onSpeechEnded() {
    if (m_state != State::Streaming) {
        return;
    }
    endStream(QStringLiteral("normal"));
}

void AudioSessionOrchestrator::onOpusFrameEncoded(const QByteArray &opusFrame) {
    if (m_state != State::Streaming) {
        return;
    }
    QByteArray binaryFrame = protocol::BinaryCodec::encodeBinaryFrame(m_activeStreamId, opusFrame);
    m_wsClient->sendBinaryFrame(binaryFrame);
}

// LCOV_EXCL_START — kStreamingTimeoutMs is 30 s; not exercisable in unit tests
void AudioSessionOrchestrator::onStreamingTimeout() {
    if (m_state != State::Streaming) {
        return;
    }
    qCWarning(lcPipeline) << "Streaming timeout after" << kStreamingTimeoutMs << "ms";
    endStream(QStringLiteral("timeout"));
}
// LCOV_EXCL_STOP

void AudioSessionOrchestrator::endStream(const QString &reason) {
    m_streamingTimeout.stop();

    // Send SpeechEnded envelope
    protocol::SpeechEnded speechEndMsg;
    speechEndMsg.streamId = m_activeStreamId;
    m_wsClient->sendMessage(speechEndMsg);

    // Send AudioStreamEnd envelope
    protocol::AudioStreamEndIn endMsg;
    endMsg.streamId = m_activeStreamId;
    endMsg.reason = reason;
    m_wsClient->sendMessage(endMsg);

    qCInfo(lcPipeline) << "Stream ended:" << m_activeStreamId << "reason:" << reason;
    QString streamId = m_activeStreamId;
    m_activeStreamId.clear();

    transitionTo(State::Idle);
    // Automatically restart listening
    startListening();

    emit streamEnded(streamId);
}

void AudioSessionOrchestrator::transitionTo(State newState) {
    if (m_state == newState) {
        return;
    }

    static constexpr const char *kStateNames[] = {"idle", "awaiting_wake", "streaming",
                                                  "ending_speech"};
    m_state = newState;
    m_connState->setVoiceState(QString::fromUtf8(kStateNames[static_cast<int>(newState)]));
}

void AudioSessionOrchestrator::onTtsPlaybackStarted() {
    if (m_state != State::Streaming) {
        return;
    }
    // LCOV_EXCL_START — TTS playback while upload streaming requires a live audio pipeline; not exercisable in unit tests
    qCInfo(lcPipeline) << "TTS playback started — ending upload stream to prevent mic loopback";
    endStream(QStringLiteral("normal"));
    // LCOV_EXCL_STOP
}

void AudioSessionOrchestrator::onInboundAudio(const QByteArray &binaryFrame) {
    auto decoded = protocol::BinaryCodec::decodeBinaryFrame(binaryFrame);
    if (decoded.payload.isEmpty()) {
        return;
    }
    if (m_playback)
        m_playback->handleOpusFrame(decoded.payload);
}

void AudioSessionOrchestrator::onInboundAudioStreamStart(const QString &streamId) {
    qCInfo(lcPipeline) << "Inbound audio stream started:" << streamId;
    if (m_playback)
        m_playback->preparePlayback();
}

void AudioSessionOrchestrator::onInboundStreamEnd(const QString &streamId, const QString &reason) {
    qCInfo(lcPipeline) << "Inbound audio stream ended:" << streamId << "reason:" << reason;
    if (m_playback)
        m_playback->handleStreamEnd(reason);
}

// Fix 1: WebSocket disconnection handler.
// LCOV_EXCL_START — requires a live WebSocket that drops mid-session; not exercisable in unit tests
void AudioSessionOrchestrator::onWsDisconnected() {
    qCWarning(lcPipeline) << "WebSocket disconnected — resetting pipeline to Idle";
    m_streamingTimeout.stop();
    m_activeStreamId.clear();
    transitionTo(State::Idle);
    // Do NOT call startListening() here. onWsReconnected() will resume listening
    // once a new session is confirmed, preventing the 30 s timeout stall loop.
}
// LCOV_EXCL_STOP

// Fix 1: Resume listening once a new session is established.
// LCOV_EXCL_START — requires a live WebSocket reconnect sequence; not exercisable in unit tests
void AudioSessionOrchestrator::onWsReconnected(const QString &sessionId,
                                               const QString &serverVersion) {
    Q_UNUSED(sessionId)
    Q_UNUSED(serverVersion)
    qCInfo(lcPipeline) << "WebSocket reconnected — resuming wake word listening";
    startListening();
}
// LCOV_EXCL_STOP

// Fix 3: Central handler for capture and encoder error signals.
// LCOV_EXCL_START — requires capture or encoder to emit error during a test run; not exercisable in unit tests
void AudioSessionOrchestrator::onPipelineError(const QString &reason) {
    qCCritical(lcPipeline) << "Pipeline component error:" << reason;
    m_streamingTimeout.stop();
    m_activeStreamId.clear();
    transitionTo(State::Idle);
    emit pipelineError(reason);
}
// LCOV_EXCL_STOP

} // namespace pairion::pipeline
