/**
 * @file pairion_audio_playback.cpp
 * @brief Implementation of the inbound audio playback engine.
 */

#include "pairion_audio_playback.h"
#include "pairion_opus_decoder.h"
#include <QAudioFormat>
#include <QMediaDevices>

namespace pairion::audio {

PairionAudioPlayback::PairionAudioPlayback(QObject *parent) : QObject(parent) {
    m_decoder = new PairionOpusDecoder(this);
    connect(m_decoder, &PairionOpusDecoder::pcmFrameDecoded, this, &PairionAudioPlayback::handlePcmFrame);
    connect(m_decoder, &PairionOpusDecoder::decoderError, this, &PairionAudioPlayback::playbackError);
    initAudioSink();
    m_silenceTimer = new QTimer(this);
    m_silenceTimer->setSingleShot(true);
    connect(m_silenceTimer, &QTimer::timeout, this, [this]() {
        if (m_isSpeaking) {
            m_isSpeaking = false;
            emit speakingStateChanged("idle");
        }
    });
}

PairionAudioPlayback::~PairionAudioPlayback() {
    stop();
}

void PairionAudioPlayback::initAudioSink() {
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    m_sink = new QAudioSink(QMediaDevices::defaultAudioOutput(), format, this);
    m_audioDevice = m_sink->start();
}

void PairionAudioPlayback::start() {
    if (m_sink) m_sink->start();
}

void PairionAudioPlayback::preparePlayback() {
    constexpr int kWarmupBytes = kSampleRate * sizeof(int16_t) * kJitterMs / 1000;
    if (m_audioDevice) {
        m_audioDevice->write(QByteArray(kWarmupBytes, '\0'));
    }
}

void PairionAudioPlayback::stop() {
    if (m_sink) m_sink->stop();
    m_jitterBuffer.clear();
    if (m_silenceTimer) m_silenceTimer->stop();
    if (m_isSpeaking) {
        m_isSpeaking = false;
        emit speakingStateChanged("idle");
    }
}

void PairionAudioPlayback::handleOpusFrame(const QByteArray &opusFrame) {
    if (m_decoder) m_decoder->decodeOpusFrame(opusFrame);
}

void PairionAudioPlayback::handlePcmFrame(const QByteArray &pcm) {
    m_jitterBuffer.enqueue(pcm);
    if (!m_isSpeaking) {
        m_isSpeaking = true;
        emit speakingStateChanged("speaking");
    }
    m_silenceTimer->start(500);
    while (!m_jitterBuffer.isEmpty() && m_audioDevice) {
        QByteArray data = m_jitterBuffer.dequeue();
        m_audioDevice->write(data);
    }
}

void PairionAudioPlayback::handleStreamEnd(const QString &reason) {
    stop();
    if (reason != "normal") {
        emit playbackError("Stream ended with reason: " + reason);
    }
}

} // namespace pairion::audio