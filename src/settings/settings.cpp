/**
 * @file settings.cpp
 * @brief Implementation of the Settings singleton with QSettings persistence.
 */

#include "settings.h"

#include <QSettings>

namespace pairion::settings {

Settings::Settings(QObject *parent) : QObject(parent) {
    QSettings s;
    m_wakeThreshold = s.value(QStringLiteral("wake/threshold"), 0.3).toDouble();
    m_vadSilenceEndMs = s.value(QStringLiteral("vad/silence_end_ms"), 800).toInt();
    m_vadThreshold = s.value(QStringLiteral("vad/threshold"), 0.5).toDouble();
    m_audioInputDevice = s.value(QStringLiteral("audio/input_device"), QString()).toString();
    m_audioSampleRate = s.value(QStringLiteral("audio/sample_rate"), 16000).toInt();
}

double Settings::wakeThreshold() const {
    return m_wakeThreshold;
}
int Settings::vadSilenceEndMs() const {
    return m_vadSilenceEndMs;
}
double Settings::vadThreshold() const {
    return m_vadThreshold;
}
QString Settings::audioInputDevice() const {
    return m_audioInputDevice;
}
int Settings::audioSampleRate() const {
    return m_audioSampleRate;
}

void Settings::setWakeThreshold(double v) {
    if (m_wakeThreshold != v) {
        m_wakeThreshold = v;
        QSettings().setValue(QStringLiteral("wake/threshold"), v);
        emit wakeThresholdChanged();
    }
}

void Settings::setVadSilenceEndMs(int v) {
    if (m_vadSilenceEndMs != v) {
        m_vadSilenceEndMs = v;
        QSettings().setValue(QStringLiteral("vad/silence_end_ms"), v);
        emit vadSilenceEndMsChanged();
    }
}

void Settings::setVadThreshold(double v) {
    if (m_vadThreshold != v) {
        m_vadThreshold = v;
        QSettings().setValue(QStringLiteral("vad/threshold"), v);
        emit vadThresholdChanged();
    }
}

void Settings::setAudioInputDevice(const QString &v) {
    if (m_audioInputDevice != v) {
        m_audioInputDevice = v;
        QSettings().setValue(QStringLiteral("audio/input_device"), v);
        emit audioInputDeviceChanged();
    }
}

void Settings::setAudioSampleRate(int v) {
    if (m_audioSampleRate != v) {
        m_audioSampleRate = v;
        QSettings().setValue(QStringLiteral("audio/sample_rate"), v);
        emit audioSampleRateChanged();
    }
}

} // namespace pairion::settings
