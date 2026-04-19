/**
 * @file tst_settings.cpp
 * @brief Tests for the Settings singleton: default values, persistence, signal emission.
 */

#include "../src/settings/settings.h"

#include <QSettings>
#include <QSignalSpy>
#include <QTest>

using namespace pairion::settings;

class TestSettings : public QObject {
    Q_OBJECT

  private slots:
    /// Clear all settings before each test.
    void init() {
        QSettings s;
        s.remove(QStringLiteral("wake"));
        s.remove(QStringLiteral("vad"));
        s.remove(QStringLiteral("audio"));
    }

    /// Verify defaults when no QSettings values exist.
    void defaultValues() {
        Settings settings;
        QCOMPARE(settings.wakeThreshold(), 0.3);
        QCOMPARE(settings.vadSilenceEndMs(), 800);
        QCOMPARE(settings.vadThreshold(), 0.5);
        QVERIFY(settings.audioInputDevice().isEmpty());
        QCOMPARE(settings.audioSampleRate(), 16000);
    }

    /// Verify setter writes to QSettings.
    void setterPersists() {
        {
            Settings settings;
            settings.setWakeThreshold(0.7);
            settings.setVadSilenceEndMs(1000);
            settings.setVadThreshold(0.6);
            settings.setAudioInputDevice(QStringLiteral("TestMic"));
            settings.setAudioSampleRate(48000);
        }
        // New instance should read persisted values
        Settings settings2;
        QCOMPARE(settings2.wakeThreshold(), 0.7);
        QCOMPARE(settings2.vadSilenceEndMs(), 1000);
        QCOMPARE(settings2.vadThreshold(), 0.6);
        QCOMPARE(settings2.audioInputDevice(), QStringLiteral("TestMic"));
        QCOMPARE(settings2.audioSampleRate(), 48000);
    }

    /// Verify setters emit changed signals.
    void settersEmitSignals() {
        Settings settings;
        QSignalSpy wakeSpy(&settings, &Settings::wakeThresholdChanged);
        QSignalSpy vadSilSpy(&settings, &Settings::vadSilenceEndMsChanged);
        QSignalSpy vadThSpy(&settings, &Settings::vadThresholdChanged);
        QSignalSpy devSpy(&settings, &Settings::audioInputDeviceChanged);
        QSignalSpy rateSpy(&settings, &Settings::audioSampleRateChanged);

        settings.setWakeThreshold(0.8);
        settings.setVadSilenceEndMs(500);
        settings.setVadThreshold(0.3);
        settings.setAudioInputDevice(QStringLiteral("Mic2"));
        settings.setAudioSampleRate(8000);

        QCOMPARE(wakeSpy.count(), 1);
        QCOMPARE(vadSilSpy.count(), 1);
        QCOMPARE(vadThSpy.count(), 1);
        QCOMPARE(devSpy.count(), 1);
        QCOMPARE(rateSpy.count(), 1);
    }

    /// Verify setters do not emit when value is unchanged.
    void noEmitOnSameValue() {
        Settings settings;
        QSignalSpy spy(&settings, &Settings::wakeThresholdChanged);
        settings.setWakeThreshold(0.3);
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
