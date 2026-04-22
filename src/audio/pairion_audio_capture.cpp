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

void PairionAudioCapture::configure(const QString &deviceId, int sampleRate) {
    m_configuredDeviceId = deviceId;
    m_configuredSampleRate = (sampleRate > 0) ? sampleRate : kSampleRate;
    qCInfo(lcCapture) << "Audio capture configured: device=" << m_configuredDeviceId
                      << "rate=" << m_configuredSampleRate;
    if (m_running) {
        stop();
        start();
    }
}

void PairionAudioCapture::start() {
    if (m_running) {
        return;
    }

    // LCOV_EXCL_START — requires real audio hardware; not exercisable in guiless unit tests
    if (m_ownsSource) {
        QAudioFormat format;
        format.setSampleRate(m_configuredSampleRate > 0 ? m_configuredSampleRate : kSampleRate);
        format.setChannelCount(kChannels);
        format.setSampleFormat(QAudioFormat::Int16);

        // Select device: use configured device ID if provided, else system default.
        QAudioDevice selectedDevice = QMediaDevices::defaultAudioInput();
        if (!m_configuredDeviceId.isEmpty()) {
            const auto devices = QMediaDevices::audioInputs();
            for (const auto &dev : devices) {
                if (dev.description() == m_configuredDeviceId) {
                    selectedDevice = dev;
                    break;
                }
            }
        }

        if (!selectedDevice.isFormatSupported(format)) {
            // Fall back to 16 kHz if the configured rate isn't supported.
            format.setSampleRate(kSampleRate);
            if (!selectedDevice.isFormatSupported(format)) {
                emit captureError(QStringLiteral("Audio format not supported by device: %1")
                                      .arg(selectedDevice.description()));
                return;
            }
            qCWarning(lcCapture) << "Requested sample rate not supported; using 16000 Hz";
        }

        m_audioSource = new QAudioSource(selectedDevice, format, this);
        connect(m_audioSource, &QAudioSource::stateChanged, this,
                &PairionAudioCapture::onAudioSourceStateChanged);
        m_ioDevice = m_audioSource->start();
        if (m_ioDevice == nullptr) {
            emit captureError(QStringLiteral("Failed to start audio source"));
            return;
        }
    }
    // LCOV_EXCL_STOP

    connect(m_ioDevice, &QIODevice::readyRead, this, &PairionAudioCapture::onAudioDataReady);
    m_running = true;
    qCInfo(lcCapture) << "Audio capture started:" << m_configuredSampleRate << "Hz," << kChannels
                      << "ch";
}

void PairionAudioCapture::stop() {
    if (!m_running) {
        return;
    }
    m_running = false;

    if (m_ioDevice != nullptr) {
        disconnect(m_ioDevice, &QIODevice::readyRead, this, &PairionAudioCapture::onAudioDataReady);
    }

    // LCOV_EXCL_START — only reachable when m_ownsSource=true (hardware path)
    if (m_ownsSource && m_audioSource != nullptr) {
        m_audioSource->stop();
        delete m_audioSource;
        m_audioSource = nullptr;
        m_ioDevice = nullptr;
    }
    // LCOV_EXCL_STOP

    m_accumulator.clear();
    qCInfo(lcCapture) << "Audio capture stopped";
}

void PairionAudioCapture::onAudioDataReady() {
    // LCOV_EXCL_START — m_ioDevice is always valid when connected (hardware path nulls it)
    if (m_ioDevice == nullptr) {
        return;
    }
    // LCOV_EXCL_STOP
    QByteArray data = m_ioDevice->readAll();
    // LCOV_EXCL_START — readAll() on a connected device with pending data is never empty here
    if (data.isEmpty()) {
        return;
    }
    // LCOV_EXCL_STOP
    m_accumulator.append(data);
    processAccumulator();
}

// LCOV_EXCL_START — requires real audio hardware; not exercisable in guiless unit tests
void PairionAudioCapture::onAudioSourceStateChanged(QAudio::State state) {
    if (!m_running) {
        return; // Intentional stop — ignore.
    }
    if (state == QAudio::StoppedState && m_audioSource != nullptr
        && m_audioSource->error() != QAudio::NoError) {
        QString reason = QStringLiteral("Audio device stopped unexpectedly (error %1)")
                             .arg(static_cast<int>(m_audioSource->error()));
        qCCritical(lcCapture) << reason;
        m_running = false;
        emit captureError(reason);
    }
}
// LCOV_EXCL_STOP

void PairionAudioCapture::processAccumulator() {
    while (m_accumulator.size() >= kFrameBytes) {
        QByteArray frame = m_accumulator.left(kFrameBytes);
        m_accumulator.remove(0, kFrameBytes);
        emit audioFrameAvailable(frame);
    }
}

} // namespace pairion::audio
