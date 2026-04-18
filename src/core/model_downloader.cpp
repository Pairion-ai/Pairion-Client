/**
 * @file model_downloader.cpp
 * @brief Implementation of the ONNX model downloader with SHA-256 verification.
 */

#include "model_downloader.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcModelDl, "pairion.model")

namespace pairion::core {

ModelDownloader::ModelDownloader(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent), m_nam(nam) {}

QString ModelDownloader::modelCacheDir() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base + QStringLiteral("/models");
}

QString ModelDownloader::modelPath(const QString &name) {
    return modelCacheDir() + QStringLiteral("/") + name;
}

void ModelDownloader::checkAndDownload() {
    QDir().mkpath(modelCacheDir());

    auto manifest = loadManifest();
    m_totalCount = manifest.size();
    m_completedCount = 0;
    m_pending.clear();

    for (const auto &entry : manifest) {
        QString path = modelPath(entry.name);
        if (QFile::exists(path) && verifyFile(path, entry.sha256)) {
            m_completedCount++;
            qCInfo(lcModelDl) << "Model cached and verified:" << entry.name;
        } else {
            m_pending.append(entry);
        }
    }

    if (m_pending.isEmpty()) {
        emit allModelsReady();
        return;
    }

    qCInfo(lcModelDl) << "Downloading" << m_pending.size() << "model(s)";
    downloadNext();
}

QList<ModelDownloader::ModelEntry> ModelDownloader::loadManifest() {
    QFile file(QStringLiteral(":/resources/model_manifest.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcModelDl) << "Failed to open model manifest resource";
        return {};
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        qCWarning(lcModelDl) << "Manifest parse error:" << err.errorString();
        return {};
    }

    QList<ModelEntry> entries;
    for (const auto &val : doc.array()) {
        QJsonObject obj = val.toObject();
        entries.append({obj[QStringLiteral("name")].toString(),
                        QUrl(obj[QStringLiteral("url")].toString()),
                        obj[QStringLiteral("sha256")].toString().toLatin1()});
    }
    return entries;
}

bool ModelDownloader::verifyFile(const QString &path, const QByteArray &expectedSha256) {
    if (expectedSha256.isEmpty()) {
        return true; // No hash to verify — accept cached file
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    QByteArray actualHex = hash.result().toHex();
    return actualHex == expectedSha256;
}

void ModelDownloader::downloadNext() {
    if (m_pending.isEmpty()) {
        emit allModelsReady();
        return;
    }

    const ModelEntry &entry = m_pending.first();
    qCInfo(lcModelDl) << "Downloading:" << entry.name << "from" << entry.url.toString();

    QNetworkReply *reply = m_nam->get(QNetworkRequest(entry.url));
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            int fileProgress = static_cast<int>((received * 100) / total);
            int overall = ((m_completedCount * 100) + fileProgress) / m_totalCount;
            emit downloadProgress(overall);
        }
    });
    connect(reply, &QNetworkReply::finished, this, &ModelDownloader::onDownloadFinished);
}

void ModelDownloader::onDownloadFinished() {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply == nullptr) {
        return;
    }
    reply->deleteLater();

    if (m_pending.isEmpty()) {
        return;
    }

    const ModelEntry entry = m_pending.takeFirst();

    if (reply->error() != QNetworkReply::NoError) {
        emit downloadError(
            QStringLiteral("Download failed for %1: %2").arg(entry.name, reply->errorString()));
        return;
    }

    QByteArray data = reply->readAll();
    QString path = modelPath(entry.name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        emit downloadError(QStringLiteral("Cannot write model file: %1").arg(path));
        return;
    }
    file.write(data);
    file.close();

    if (!verifyFile(path, entry.sha256)) {
        QFile::remove(path);
        emit downloadError(QStringLiteral("SHA-256 mismatch for %1").arg(entry.name));
        return;
    }

    m_completedCount++;
    qCInfo(lcModelDl) << "Model downloaded and verified:" << entry.name;
    emit downloadProgress((m_completedCount * 100) / m_totalCount);
    downloadNext();
}

} // namespace pairion::core
