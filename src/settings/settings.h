#pragma once

/**
 * @file settings.h
 * @brief QSettings-backed singleton exposing runtime-tunable parameters to QML.
 *
 * Provides configurable values for wake word threshold, VAD silence duration,
 * audio input device, and sample rate. Values persist via QSettings and are
 * readable/writable from QML property bindings.
 */

#include <QObject>
#include <QString>

namespace pairion::settings {

/**
 * @brief Singleton QObject exposing application settings to QML.
 *
 * Reads defaults from QSettings on construction. Setters write back immediately.
 * No settings UI at M1; values editable by modifying the QSettings file manually.
 */
class Settings : public QObject {
    Q_OBJECT

    Q_PROPERTY(
        double wakeThreshold READ wakeThreshold WRITE setWakeThreshold NOTIFY wakeThresholdChanged)
    Q_PROPERTY(int vadSilenceEndMs READ vadSilenceEndMs WRITE setVadSilenceEndMs NOTIFY
                   vadSilenceEndMsChanged)
    Q_PROPERTY(
        double vadThreshold READ vadThreshold WRITE setVadThreshold NOTIFY vadThresholdChanged)
    Q_PROPERTY(QString audioInputDevice READ audioInputDevice WRITE setAudioInputDevice NOTIFY
                   audioInputDeviceChanged)
    Q_PROPERTY(int audioSampleRate READ audioSampleRate WRITE setAudioSampleRate NOTIFY
                   audioSampleRateChanged)

  public:
    /// @brief Construct and load values from QSettings.
    explicit Settings(QObject *parent = nullptr);

    double wakeThreshold() const;
    int vadSilenceEndMs() const;
    double vadThreshold() const;
    QString audioInputDevice() const;
    int audioSampleRate() const;

  public slots:
    void setWakeThreshold(double v);
    void setVadSilenceEndMs(int v);
    void setVadThreshold(double v);
    void setAudioInputDevice(const QString &v);
    void setAudioSampleRate(int v);

  signals:
    void wakeThresholdChanged();
    void vadSilenceEndMsChanged();
    void vadThresholdChanged();
    void audioInputDeviceChanged();
    void audioSampleRateChanged();

  private:
    double m_wakeThreshold;
    int m_vadSilenceEndMs;
    double m_vadThreshold;
    QString m_audioInputDevice;
    int m_audioSampleRate;
};

} // namespace pairion::settings
