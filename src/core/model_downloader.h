#pragma once

/**
 * @file model_downloader.h
 * @brief Downloads ONNX model files, verifies SHA-256, and caches to AppDataLocation.
 *
 * Reads model URLs and expected hashes from a committed model_manifest.json
 * resource. Downloads only what's missing or has mismatched hashes.
 */

#include <QByteArray>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>

namespace pairion::core {

/**
 * @brief Downloads and caches ONNX model files for wake word and VAD.
 *
 * On checkAndDownload(), reads the manifest, checks cached files, and downloads
 * any missing or corrupt models. Emits allModelsReady() when all models are
 * verified and available.
 */
class ModelDownloader : public QObject {
    Q_OBJECT
  public:
    /**
     * @brief Construct the downloader.
     * @param nam QNetworkAccessManager for HTTP requests (not owned).
     * @param parent QObject parent.
     */
    explicit ModelDownloader(QNetworkAccessManager *nam, QObject *parent = nullptr);

    /**
     * @brief Check cached models and download any missing files.
     */
    void checkAndDownload();

    /**
     * @brief Get the cache directory where models are stored.
     * @return Absolute path to the models cache directory.
     */
    static QString modelCacheDir();

    /**
     * @brief Get the path to a specific cached model file.
     * @param name Model filename (e.g. "melspectrogram.onnx").
     * @return Absolute path to the cached file.
     */
    static QString modelPath(const QString &name);

  signals:
    /// Emitted with download progress (0-100).
    void downloadProgress(int percent);
    /// Emitted when all models are verified and ready.
    void allModelsReady();
    /// Emitted if a download or verification fails.
    void downloadError(const QString &reason);

  private:
    struct ModelEntry {
        QString name;
        QUrl url;
        QByteArray sha256;
    };

    QList<ModelEntry> loadManifest();
    bool verifyFile(const QString &path, const QByteArray &expectedSha256);
    void downloadNext();
    void onDownloadFinished();

    QNetworkAccessManager *m_nam;
    QList<ModelEntry> m_pending;
    int m_totalCount = 0;
    int m_completedCount = 0;
};

} // namespace pairion::core
