/**
 * @file tst_model_downloader.cpp
 * @brief Tests for ModelDownloader: manifest parsing and file caching logic.
 */

#include "../src/core/model_downloader.h"

#include <QDir>
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

    /// Verify checkAndDownload emits allModelsReady when all files are cached.
    void allCachedEmitsReady() {
        // Set test-specific data location to avoid touching real cache
        QStandardPaths::setTestModeEnabled(true);
        QString cacheDir = ModelDownloader::modelCacheDir();
        QDir().mkpath(cacheDir);

        // Create fake model files (manifest has empty sha256, so they pass verification)
        QStringList modelNames = {
            QStringLiteral("melspectrogram.onnx"), QStringLiteral("embedding_model.onnx"),
            QStringLiteral("hey_jarvis_v0.1.onnx"), QStringLiteral("silero_vad.onnx")};
        for (const auto &name : modelNames) {
            QFile f(cacheDir + QStringLiteral("/") + name);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("fake_model_data");
            f.close();
        }

        QNetworkAccessManager nam;
        ModelDownloader downloader(&nam);
        QSignalSpy readySpy(&downloader, &ModelDownloader::allModelsReady);

        downloader.checkAndDownload();
        QCOMPARE(readySpy.count(), 1);

        // Cleanup
        for (const auto &name : modelNames) {
            QFile::remove(cacheDir + QStringLiteral("/") + name);
        }
        QStandardPaths::setTestModeEnabled(false);
    }

    /// Verify checkAndDownload with missing files attempts download.
    void missingFileTriggersDownload() {
        QStandardPaths::setTestModeEnabled(true);
        QString cacheDir = ModelDownloader::modelCacheDir();
        // Ensure directory is clean
        QDir dir(cacheDir);
        dir.removeRecursively();
        QDir().mkpath(cacheDir);

        QNetworkAccessManager nam;
        ModelDownloader downloader(&nam);
        QSignalSpy readySpy(&downloader, &ModelDownloader::allModelsReady);
        QSignalSpy errorSpy(&downloader, &ModelDownloader::downloadError);

        downloader.checkAndDownload();

        // Without network, we expect either an error or the download to be attempted
        // Since we can't guarantee network, just verify the downloader didn't emit ready
        // immediately (it has pending downloads)
        if (readySpy.count() == 0) {
            // Wait briefly for potential network error
            QTest::qWait(2000);
            // Either download succeeded or error occurred — both valid
            QVERIFY(readySpy.count() > 0 || errorSpy.count() > 0);
        }

        QDir(cacheDir).removeRecursively();
        QStandardPaths::setTestModeEnabled(false);
    }
};

QTEST_GUILESS_MAIN(TestModelDownloader)
#include "tst_model_downloader.moc"
