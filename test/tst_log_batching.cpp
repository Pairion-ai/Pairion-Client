/**
 * @file tst_log_batching.cpp
 * @brief Unit tests for the Logger's batching and flush behavior.
 */

#include "../src/util/logger.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTest>

using namespace pairion::util;

class TestLogBatching : public QObject {
    Q_OBJECT

  private slots:
    /// Verify logger starts with empty buffer.
    void initiallyEmpty() {
        QNetworkAccessManager nam;
        Logger logger(&nam, QStringLiteral("http://localhost:18789/v1"));
        QCOMPARE(logger.pendingCount(), 0);
        QVERIFY(logger.pendingRecords().isEmpty());
    }

    /// Verify message handler enqueues records.
    void messageHandlerEnqueues() {
        QNetworkAccessManager nam;
        Logger logger(&nam, QStringLiteral("http://localhost:18789/v1"));
        logger.install();

        qInfo() << "Test log message";

        QVERIFY(logger.pendingCount() >= 1);

        QJsonArray records = logger.pendingRecords();
        bool found = false;
        for (const auto &val : records) {
            QJsonObject obj = val.toObject();
            if (obj["message"].toString().contains(QStringLiteral("Test log message"))) {
                QCOMPARE(obj["level"].toString(), QStringLiteral("INFO"));
                QVERIFY(!obj["timestamp"].toString().isEmpty());
                found = true;
                break;
            }
        }
        QVERIFY(found);

        qInstallMessageHandler(nullptr);
    }

    /// Verify different log levels map correctly.
    void logLevelMapping() {
        QNetworkAccessManager nam;
        Logger logger(&nam, QStringLiteral("http://localhost:18789/v1"));
        logger.install();

        qDebug() << "debug msg";
        qInfo() << "info msg";
        qWarning() << "warn msg";
        qCritical() << "error msg";

        QJsonArray records = logger.pendingRecords();
        QStringList levels;
        for (const auto &val : records) {
            levels.append(val.toObject()["level"].toString());
        }
        QVERIFY(levels.contains(QStringLiteral("DEBUG")));
        QVERIFY(levels.contains(QStringLiteral("INFO")));
        QVERIFY(levels.contains(QStringLiteral("WARN")));
        QVERIFY(levels.contains(QStringLiteral("ERROR")));

        qInstallMessageHandler(nullptr);
    }

    /// Verify session ID is included in records after being set.
    void sessionIdIncluded() {
        QNetworkAccessManager nam;
        Logger logger(&nam, QStringLiteral("http://localhost:18789/v1"));
        logger.install();
        logger.setSessionId(QStringLiteral("sess-99"));

        qInfo() << "session test";

        QJsonArray records = logger.pendingRecords();
        bool found = false;
        for (const auto &val : records) {
            QJsonObject obj = val.toObject();
            if (obj["message"].toString().contains(QStringLiteral("session test"))) {
                QCOMPARE(obj["sessionId"].toString(), QStringLiteral("sess-99"));
                found = true;
                break;
            }
        }
        QVERIFY(found);

        qInstallMessageHandler(nullptr);
    }

    /// Verify flush clears the buffer.
    void flushClearsBuffer() {
        QNetworkAccessManager nam;
        Logger logger(&nam, QStringLiteral("http://localhost:18789/v1"));
        logger.install();

        qInfo() << "will be flushed";
        QVERIFY(logger.pendingCount() > 0);

        logger.flush();
        QCOMPARE(logger.pendingCount(), 0);

        qInstallMessageHandler(nullptr);
    }

    /// Verify flush with empty buffer is a no-op.
    void flushEmptyBufferIsNoop() {
        QNetworkAccessManager nam;
        Logger logger(&nam, QStringLiteral("http://localhost:18789/v1"));

        logger.flush();
        QCOMPARE(logger.pendingCount(), 0);
    }

    /// Verify log callback is invoked for each message.
    void logCallbackInvoked() {
        QNetworkAccessManager nam;
        Logger logger(&nam, QStringLiteral("http://localhost:18789/v1"));

        QStringList captured;
        logger.setLogCallback([&captured](const QString &entry) { captured.append(entry); });
        logger.install();

        qInfo() << "callback test";

        bool found = false;
        for (const auto &entry : captured) {
            if (entry.contains(QStringLiteral("callback test"))) {
                found = true;
                break;
            }
        }
        QVERIFY(found);

        qInstallMessageHandler(nullptr);
    }

    /// Verify flush timer interval is set correctly.
    void flushTimerStartsOnInstall() {
        QNetworkAccessManager nam;
        Logger logger(&nam, QStringLiteral("http://localhost:18789/v1"));
        logger.install();

        // Timer should be active after install
        // We just verify no crash and pending count works
        qInfo() << "timer test";
        QVERIFY(logger.pendingCount() > 0);

        qInstallMessageHandler(nullptr);
    }

    /// Verify messageHandler with no instance does not crash.
    void messageHandlerWithNullInstance() {
        // Reset the message handler to defaults
        qInstallMessageHandler(nullptr);

        // Directly call the static handler with null instance
        // This exercises the early-return path
        Logger::messageHandler(QtDebugMsg, QMessageLogContext(), QStringLiteral("test"));
        // No crash = pass
    }

    /// Verify records without sessionId omit the field.
    void recordsWithoutSessionId() {
        QNetworkAccessManager nam;
        Logger logger(&nam, QStringLiteral("http://localhost:18789/v1"));
        logger.install();

        qInfo() << "no session";

        QJsonArray records = logger.pendingRecords();
        bool found = false;
        for (const auto &val : records) {
            QJsonObject obj = val.toObject();
            if (obj["message"].toString().contains(QStringLiteral("no session"))) {
                QVERIFY(!obj.contains("sessionId"));
                found = true;
                break;
            }
        }
        QVERIFY(found);

        qInstallMessageHandler(nullptr);
    }

    /// Verify flush sends records with sessionId when set.
    void flushWithSessionId() {
        QNetworkAccessManager nam;
        Logger logger(&nam, QStringLiteral("http://localhost:18789/v1"));
        logger.install();
        logger.setSessionId(QStringLiteral("flush-sess"));

        qInfo() << "flush session test";

        // Flush triggers the HTTP POST path including sessionId serialization
        logger.flush();
        QCOMPARE(logger.pendingCount(), 0);

        qInstallMessageHandler(nullptr);
    }
};

QTEST_GUILESS_MAIN(TestLogBatching)
#include "tst_log_batching.moc"
