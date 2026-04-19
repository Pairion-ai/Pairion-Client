/**
 * @file tst_settings.cpp
 * @brief Tests for the Settings singleton: default values, persistence, signal emission,
 *        and configure() propagation to PairionAudioCapture.
 */

#include "../src/audio/pairion_audio_capture.h"
#include "../src/settings/settings.h"

#include <QBuffer>
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

    /// Verify configure() propagates audioInputDevice and audioSampleRate into PairionAudioCapture.
    void configurePropagatesToCapture() {
        Settings settings;
        settings.setAudioInputDevice(QStringLiteral("TestMic"));
        settings.setAudioSampleRate(48000);

        // Capture constructed with external QIODevice (nullptr) — test path, no real mic.
        pairion::audio::PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        capture.configure(settings.audioInputDevice(), settings.audioSampleRate());

        QCOMPARE(capture.configuredDeviceId(), QStringLiteral("TestMic"));
        QCOMPARE(capture.configuredSampleRate(), 48000);
    }

    /// Verify configure() falls back to 16000 when sampleRate <= 0.
    void configureFallbackRate() {
        pairion::audio::PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        capture.configure(QString(), 0);
        QCOMPARE(capture.configuredSampleRate(), 16000);
    }

    /// Verify the default (hardware-owning) constructor constructs without crash.
    void defaultConstructorCreated() {
        pairion::audio::PairionAudioCapture capture;
        // Hardware path — do not call start(). Just verify construction succeeds.
    }

    /// Verify start() and stop() lifecycle with a non-null QIODevice (non-owning path).
    void startStopWithBuffer() {
        QBuffer buf;
        buf.open(QIODevice::ReadOnly);
        pairion::audio::PairionAudioCapture capture(&buf);
        capture.start();
        capture.start(); // idempotent — already running
        capture.stop();
        capture.stop(); // idempotent — already stopped
    }

    /// Verify configure() triggers stop()+start() when already running.
    void configureRestartsCapture() {
        QBuffer buf;
        buf.open(QIODevice::ReadOnly);
        pairion::audio::PairionAudioCapture capture(&buf);
        capture.start(); // m_running = true
        capture.configure(QStringLiteral("NewDevice"), 48000);
        QCOMPARE(capture.configuredDeviceId(), QStringLiteral("NewDevice"));
        QCOMPARE(capture.configuredSampleRate(), 48000);
    }

    /// Verify onAudioDataReady processes data and emits audioFrameAvailable.
    void audioDataReadyEmitsFrame() {
        QBuffer buf;
        // Write 2 full PCM frames (2 × 640 bytes) so processAccumulator emits twice.
        buf.setData(QByteArray(1280, '\0'));
        buf.open(QIODevice::ReadOnly);
        pairion::audio::PairionAudioCapture capture(&buf);
        QSignalSpy frameSpy(&capture, &pairion::audio::PairionAudioCapture::audioFrameAvailable);
        capture.start();
        // Trigger onAudioDataReady via the readyRead signal
        emit buf.readyRead();
        QCOMPARE(frameSpy.count(), 2);
    }
};

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
