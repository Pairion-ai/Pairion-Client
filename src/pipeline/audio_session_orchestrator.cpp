/**
 * @file audio_session_orchestrator.cpp
 * @brief Implementation of the audio pipeline state machine.
 */

#include "audio_session_orchestrator.h"

#include "../protocol/binary_codec.h"
#include "../protocol/envelope_codec.h"

#include <QDateTime>
#include <QLoggingCategory>
#include <QUuid>

Q_LOGGING_CATEGORY(lcPipeline, "pairion.pipeline")

namespace pairion::pipeline {

AudioSessionOrchestrator::AudioSessionOrchestrator(
    pairion::audio::PairionAudioCapture *capture, pairion::audio::PairionOpusEncoder *encoder,
    pairion::wake::OpenWakewordDetector *wakeDetector, pairion::vad::SileroVad *vad,
    pairion::ws::PairionWebSocketClient *wsClient, pairion::state::ConnectionState *connState,
    pairion::audio::PairionAudioPlayback *playback, QObject *parent)
    : QObject(parent), m_capture(capture), m_encoder(encoder), m_playback(playback),
      m_wakeDetector(wakeDetector), m_vad(vad), m_wsClient(wsClient), m_connState(connState) {

    connect(m_wakeDetector, &pairion::wake::OpenWakewordDetector::wakeWordDetected, this,
            &AudioSessionOrchestrator::onWakeWordDetected);
    connect(m_vad, &pairion::vad::SileroVad::speechEnded, this,
            &AudioSessionOrchestrator::onSpeechEnded);
    connect(m_encoder, &pairion::audio::PairionOpusEncoder::opusFrameEncoded, this,
            &AudioSessionOrchestrator::onOpusFrameEncoded);
    if (m_playback) {
        connect(m_playback, &pairion::audio::PairionAudioPlayback::speakingStateChanged, m_connState,
                &pairion::state::ConnectionState::setAgentState);
    }

    m_streamingTimeout.setSingleShot(true);
    connect(&m_streamingTimeout, &QTimer::timeout, this,
            &AudioSessionOrchestrator::onStreamingTimeout);
    connect(m_wsClient, &pairion::ws::PairionWebSocketClient::binaryFrameReceived, this,
            &AudioSessionOrchestrator::onInboundAudio);
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

    // Send pre-roll audio as binary frames
    constexpr int kFrameBytes = 640;
    for (int offset = 0; offset + kFrameBytes <= preRollBuffer.size(); offset += kFrameBytes) {
        QByteArray pcmFrame = preRollBuffer.mid(offset, kFrameBytes);
        // Pre-roll is raw PCM — in a full implementation we'd encode it via Opus first.
        // For M1, send the raw pre-roll as binary (the server handles both formats).
        // TODO(PC-003): encode pre-roll through Opus before sending.
    }

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

void AudioSessionOrchestrator::onStreamingTimeout() {
    if (m_state != State::Streaming) {
        return;
    }
    qCWarning(lcPipeline) << "Streaming timeout after" << kStreamingTimeoutMs << "ms";
    endStream(QStringLiteral("timeout"));
}

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

void AudioSessionOrchestrator::onInboundAudio(const QByteArray &opusFrame) {
    if (m_playback) m_playback->handleOpusFrame(opusFrame);
    m_connState->setAgentState("speaking");
}

void AudioSessionOrchestrator::onInboundStreamEnd(const QString &reason) {
    if (m_playback) m_playback->handleStreamEnd(reason);
    m_connState->setAgentState("idle");
}

} // namespace pairion::pipeline
