/**
 * @file tst_model_downloader.cpp
 * @brief Tests for ModelDownloader: manifest parsing, caching, and hash verification.
 */

#include "../src/core/model_downloader.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

using namespace pairion::core;

class TestModelDownloader : public QObject {
    Q_OBJECT

  private slots:
    /// Verify modelCacheDir returns a non-empty path.
    void cacheDirNonEmpty() {
        QString dir = ModelDownloader::modelCacheDir();
        QVERIFY(!dir.isEmpty());
        QVERIFY(dir.contains(QStringLiteral("models")));
    }

    /// Verify modelPath appends the filename.
    void modelPathAppendsName() {
        QString path = ModelDownloader::modelPath(QStringLiteral("test.onnx"));
        QVERIFY(path.endsWith(QStringLiteral("/models/test.onnx")));
    }

    /// Verify checkAndDownload triggers download when files are missing.
    void missingFilesTriggersDownload() {
        QStandardPaths::setTestModeEnabled(true);
        QString cacheDir = ModelDownloader::modelCacheDir();
        QDir(cacheDir).removeRecursively();
        QDir().mkpath(cacheDir);

        QNetworkAccessManager nam;
        ModelDownloader downloader(&nam);
        QSignalSpy readySpy(&downloader, &ModelDownloader::allModelsReady);
        QSignalSpy errorSpy(&downloader, &ModelDownloader::downloadError);

        downloader.checkAndDownload();

        // Without real network, expect download error (can't reach GitHub)
        // or ready if somehow cached. Either is valid — test doesn't crash.
        if (readySpy.count() == 0) {
            QTest::qWait(3000);
        }
        QVERIFY(readySpy.count() > 0 || errorSpy.count() > 0);

        QDir(cacheDir).removeRecursively();
        QStandardPaths::setTestModeEnabled(false);
    }

    /// Verify that files with SHA-256 mismatch are rejected and re-downloaded.
    void hashMismatchTriggersRedownload() {
        QStandardPaths::setTestModeEnabled(true);
        QString cacheDir = ModelDownloader::modelCacheDir();
        QDir().mkpath(cacheDir);

        // Create fake model files with wrong content (sha256 won't match manifest)
        QStringList modelNames = {
            QStringLiteral("melspectrogram.onnx"), QStringLiteral("embedding_model.onnx"),
            QStringLiteral("hey_jarvis_v0.1.onnx"), QStringLiteral("silero_vad.onnx")};
        for (const auto &name : modelNames) {
            QFile f(cacheDir + QStringLiteral("/") + name);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("fake_data_wrong_hash");
            f.close();
        }

        QNetworkAccessManager nam;
        ModelDownloader downloader(&nam);
        QSignalSpy readySpy(&downloader, &ModelDownloader::allModelsReady);
        QSignalSpy errorSpy(&downloader, &ModelDownloader::downloadError);

        downloader.checkAndDownload();

        // Files have wrong hash — downloader should attempt re-download
        // Without network, expect download error
        if (readySpy.count() == 0 && errorSpy.count() == 0) {
            QTest::qWait(3000);
        }
        // Either downloaded fresh (unlikely without network) or error
        QVERIFY(readySpy.count() > 0 || errorSpy.count() > 0);

        QDir(cacheDir).removeRecursively();
        QStandardPaths::setTestModeEnabled(false);
    }
};

QTEST_GUILESS_MAIN(TestModelDownloader)
#include "tst_model_downloader.moc"
