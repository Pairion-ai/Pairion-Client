/**
 * @file pairion_audio_capture.cpp
 * @brief Implementation of the audio capture module.
 */

#include "pairion_audio_capture.h"

#include <QAudioFormat>
#include <QLoggingCategory>
#include <QMediaDevices>

Q_LOGGING_CATEGORY(lcCapture, "pairion.audio.capture")

namespace pairion::audio {

PairionAudioCapture::PairionAudioCapture(QObject *parent) : QObject(parent), m_ownsSource(true) {}

PairionAudioCapture::PairionAudioCapture(QIODevice *audioDevice, QObject *parent)
    : QObject(parent), m_ioDevice(audioDevice), m_ownsSource(false) {}

PairionAudioCapture::~PairionAudioCapture() {
    stop();
}

void PairionAudioCapture::start() {
    if (m_running) {
        return;
    }

    if (m_ownsSource) {
        QAudioFormat format;
        format.setSampleRate(kSampleRate);
        format.setChannelCount(kChannels);
        format.setSampleFormat(QAudioFormat::Int16);

        QAudioDevice defaultDevice = QMediaDevices::defaultAudioInput();
        if (!defaultDevice.isFormatSupported(format)) {
            emit captureError(QStringLiteral("Audio format not supported by default device"));
            return;
        }

        m_audioSource = new QAudioSource(defaultDevice, format, this);
        m_ioDevice = m_audioSource->start();
        if (m_ioDevice == nullptr) {
            emit captureError(QStringLiteral("Failed to start audio source"));
            return;
        }
    }

    connect(m_ioDevice, &QIODevice::readyRead, this, &PairionAudioCapture::onAudioDataReady);
    m_running = true;
    qCInfo(lcCapture) << "Audio capture started:" << kSampleRate << "Hz," << kChannels << "ch";
}

void PairionAudioCapture::stop() {
    if (!m_running) {
        return;
    }
    m_running = false;

    if (m_ioDevice != nullptr) {
        disconnect(m_ioDevice, &QIODevice::readyRead, this, &PairionAudioCapture::onAudioDataReady);
    }

    if (m_ownsSource && m_audioSource != nullptr) {
        m_audioSource->stop();
        delete m_audioSource;
        m_audioSource = nullptr;
        m_ioDevice = nullptr;
    }

    m_accumulator.clear();
    qCInfo(lcCapture) << "Audio capture stopped";
}

void PairionAudioCapture::onAudioDataReady() {
    if (m_ioDevice == nullptr) {
        return;
    }
    QByteArray data = m_ioDevice->readAll();
    if (data.isEmpty()) {
        return;
    }
    m_accumulator.append(data);
    processAccumulator();
}

void PairionAudioCapture::processAccumulator() {
    while (m_accumulator.size() >= kFrameBytes) {
        QByteArray frame = m_accumulator.left(kFrameBytes);
        m_accumulator.remove(0, kFrameBytes);
        emit audioFrameAvailable(frame);
    }
}

} // namespace pairion::audio
