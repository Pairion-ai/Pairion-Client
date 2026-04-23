/**
 * @file tst_pipeline_health_monitor.cpp
 * @brief Unit tests for PipelineHealthMonitor.
 *
 * Tests cover construction, start(), onAudioFrame(), and the testable branches of
 * performHealthCheck(): WebSocket disconnected health surfacing and per-component
 * failure logging (capture stopped, encoder thread stopped, inference thread stopped).
 *
 * The LCOV_EXCL restart paths inside performHealthCheck() DO execute during testing —
 * they are marked only for coverage exclusion, not conditional compilation. Because
 * the restart paths can start QThreads, the Fixture destructor always quits and waits
 * for any running threads before destruction to prevent SIGABRT.
 */

#include "../src/pipeline/pipeline_health_monitor.h"
#include "../src/state/connection_state.h"
#include "mock_server.h"

#include <QSignalSpy>
#include <QTest>
#include <QThread>

using namespace pairion::pipeline;
using namespace pairion::state;
using namespace pairion::ws;
using namespace pairion::test;

class TestPipelineHealthMonitor : public QObject {
    Q_OBJECT

  private:
    /// Minimal fixture with proper lifecycle management.
    ///
    /// The LCOV_EXCL restart paths in performHealthCheck() may start encoderThread
    /// and inferenceThread during a test. The destructor always quits and waits for
    /// any running threads to prevent "QThread destroyed while running" SIGABRT.
    struct Fixture {
        ConnectionState cs;
        MockServer server;
        PairionWebSocketClient wsClient;
        pairion::audio::PairionAudioCapture capture;
        QThread encoderThread;
        QThread inferenceThread;

        Fixture()
            : wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"), &cs),
              capture(static_cast<QIODevice *>(nullptr)) {
            encoderThread.setObjectName(QStringLiteral("encoderThread"));
            inferenceThread.setObjectName(QStringLiteral("inferenceThread"));
        }

        ~Fixture() {
            // Threads may have been started by the restart path inside performHealthCheck().
            // Use quit()+wait() with a timeout; fall back to terminate() to prevent SIGABRT.
            if (encoderThread.isRunning()) {
                encoderThread.quit();
                if (!encoderThread.wait(3000))
                    encoderThread.terminate();
                encoderThread.wait();
            }
            if (inferenceThread.isRunning()) {
                inferenceThread.quit();
                if (!inferenceThread.wait(3000))
                    inferenceThread.terminate();
                inferenceThread.wait();
            }
        }

        PipelineHealthMonitor makeMonitor() {
            return PipelineHealthMonitor(&wsClient, &capture, &encoderThread, &inferenceThread,
                                        nullptr, &cs);
        }
    };

  private slots:

    /// Construction succeeds without crashing.
    void constructionSucceeds() {
        Fixture f;
        auto monitor = f.makeMonitor();
        Q_UNUSED(monitor)
    }

    /// start() does not crash; it connects the audio frame signal and schedules the timer.
    void startDoesNotCrash() {
        Fixture f;
        auto monitor = f.makeMonitor();
        monitor.start();
        QCoreApplication::processEvents();
    }

    /// onAudioFrame() sets the frame-timer-running flag without crashing.
    /// Invoked via meta-object system since the slot is private.
    void onAudioFrameUpdatesTimerFlag() {
        Fixture f;
        auto monitor = f.makeMonitor();
        monitor.start();
        // Trigger the private onAudioFrame slot through Qt's meta-object system.
        QMetaObject::invokeMethod(&monitor, "onAudioFrame", Qt::DirectConnection);
        // Slot completes without crash — both lines (restart timer + set flag) are covered.
    }

    /// performHealthCheck() with health="connecting" and wsClient disconnected
    /// leaves pipelineHealth unchanged (inner guard prevents overriding startup states).
    void performHealthCheckWsDisconnectedHealthConnecting() {
        Fixture f;
        QCOMPARE(f.cs.pipelineHealth(), QStringLiteral("connecting"));

        auto monitor = f.makeMonitor();
        monitor.performHealthCheck();

        QCOMPARE(f.cs.pipelineHealth(), QStringLiteral("connecting"));
    }

    /// performHealthCheck() with health="ready" and wsClient disconnected
    /// sets pipelineHealth to "server_disconnected".
    void performHealthCheckWsDisconnectedHealthReady() {
        Fixture f;
        f.cs.setPipelineHealth(QStringLiteral("ready"));

        QSignalSpy spy(&f.cs, &ConnectionState::pipelineHealthChanged);

        auto monitor = f.makeMonitor();
        monitor.performHealthCheck();

        QCOMPARE(f.cs.pipelineHealth(), QStringLiteral("server_disconnected"));
        // At least one signal for the "server_disconnected" transition.
        QVERIFY(spy.count() >= 1);
    }

    /// healthChanged signal is emitted with "server_disconnected" when wsClient is
    /// disconnected and health was "ready".
    void performHealthCheckEmitsHealthChanged() {
        Fixture f;
        f.cs.setPipelineHealth(QStringLiteral("ready"));

        auto monitor = f.makeMonitor();
        QSignalSpy spy(&monitor, &PipelineHealthMonitor::healthChanged);

        monitor.performHealthCheck();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("server_disconnected"));
    }

    /// performHealthCheck() with capture not running appends a warning log entry.
    void performHealthCheckCaptureNotRunningLogsWarning() {
        Fixture f;
        QCOMPARE(f.capture.isRunning(), false);

        auto monitor = f.makeMonitor();
        monitor.performHealthCheck();

        bool found = false;
        for (const auto &entry : f.cs.recentLogs()) {
            if (entry.contains(QStringLiteral("microphone"), Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "Expected a log entry containing 'microphone' for stopped capture");
    }

    /// performHealthCheck() with encoder thread not running appends a critical log entry.
    void performHealthCheckEncoderThreadNotRunningLogs() {
        Fixture f;
        QCOMPARE(f.encoderThread.isRunning(), false);

        auto monitor = f.makeMonitor();
        monitor.performHealthCheck();

        bool found = false;
        for (const auto &entry : f.cs.recentLogs()) {
            if (entry.contains(QStringLiteral("encoder"), Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "Expected a log entry containing 'encoder' for stopped encoder thread");
    }

    /// performHealthCheck() with inference thread not running appends a critical log entry.
    void performHealthCheckInferenceThreadNotRunningLogs() {
        Fixture f;
        QCOMPARE(f.inferenceThread.isRunning(), false);

        auto monitor = f.makeMonitor();
        monitor.performHealthCheck();

        bool found = false;
        for (const auto &entry : f.cs.recentLogs()) {
            if (entry.contains(QStringLiteral("inference"), Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        QVERIFY2(found,
                 "Expected a log entry containing 'inference' for stopped inference thread");
    }

    /// healthChanged signal carries the expected health string value.
    void healthChangedSignalCarriesValue() {
        Fixture f;
        f.cs.setPipelineHealth(QStringLiteral("ready"));

        auto monitor = f.makeMonitor();
        QString lastHealth;
        QObject::connect(&monitor, &PipelineHealthMonitor::healthChanged,
                         [&lastHealth](const QString &h) { lastHealth = h; });

        monitor.performHealthCheck();

        QCOMPARE(lastHealth, QStringLiteral("server_disconnected"));
    }
};

QTEST_GUILESS_MAIN(TestPipelineHealthMonitor)
#include "tst_pipeline_health_monitor.moc"
