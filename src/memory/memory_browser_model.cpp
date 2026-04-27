/**
 * @file memory_browser_model.cpp
 * @brief QML-exposed singleton model for the memory browser panel.
 */

#include "memory/memory_browser_model.h"
#include "memory/memory_client.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QVariantMap>

Q_LOGGING_CATEGORY(lcMemoryModel, "pairion.memory.model")

namespace pairion::memory {

MemoryBrowserModel::MemoryBrowserModel(MemoryClient *client, QObject *parent)
    : QObject(parent), m_client(client) {

    connect(m_client, &MemoryClient::episodesReceived, this, [this](const QJsonArray &arr) {
        bool hadMemory = hasMemory();
        m_episodes.clear();
        for (const QJsonValue &v : arr) {
            QJsonObject obj = v.toObject();
            QVariantMap map;
            map[QStringLiteral("id")] = obj.value(QStringLiteral("id")).toString();
            map[QStringLiteral("title")] = obj.value(QStringLiteral("title")).toString();
            map[QStringLiteral("startTime")] =
                formatTimestamp(obj.value(QStringLiteral("startTime")).toString());
            map[QStringLiteral("turnCount")] = obj.value(QStringLiteral("turnCount")).toInt();
            m_episodes.append(map);
        }
        emit episodesChanged();
        if (hasMemory() != hadMemory)
            emit hasMemoryChanged();
        decrementPending();
    });

    connect(m_client, &MemoryClient::turnsReceived, this, [this](const QJsonArray &arr) {
        m_selectedEpisodeTurns.clear();
        for (const QJsonValue &v : arr) {
            QJsonObject obj = v.toObject();
            QVariantMap map;
            map[QStringLiteral("role")] = obj.value(QStringLiteral("role")).toString();
            map[QStringLiteral("content")] = obj.value(QStringLiteral("content")).toString();
            map[QStringLiteral("timestamp")] =
                formatTimestamp(obj.value(QStringLiteral("timestamp")).toString());
            m_selectedEpisodeTurns.append(map);
        }
        emit selectedEpisodeTurnsChanged();
        decrementPending();
    });

    connect(m_client, &MemoryClient::preferencesReceived, this, [this](const QJsonArray &arr) {
        m_preferences.clear();
        for (const QJsonValue &v : arr) {
            QJsonObject obj = v.toObject();
            QVariantMap map;
            map[QStringLiteral("key")] = obj.value(QStringLiteral("key")).toString();
            map[QStringLiteral("value")] = obj.value(QStringLiteral("value")).toString();
            map[QStringLiteral("updatedAt")] =
                formatTimestamp(obj.value(QStringLiteral("updatedAt")).toString());
            m_preferences.append(map);
        }
        emit preferencesChanged();
        decrementPending();
    });

    connect(m_client, &MemoryClient::fetchError, this,
            [this](const QString &op, const QString &msg) {
                qCWarning(lcMemoryModel) << "fetch error" << op << ":" << msg;
                setErrorMessage(QStringLiteral("%1: %2").arg(op, msg));
                decrementPending();
            });
}

QVariantList MemoryBrowserModel::episodes() const { return m_episodes; }
QVariantList MemoryBrowserModel::selectedEpisodeTurns() const { return m_selectedEpisodeTurns; }
QVariantList MemoryBrowserModel::preferences() const { return m_preferences; }
bool MemoryBrowserModel::loading() const { return m_loading; }
QString MemoryBrowserModel::errorMessage() const { return m_errorMessage; }
QString MemoryBrowserModel::selectedEpisodeId() const { return m_selectedEpisodeId; }
bool MemoryBrowserModel::hasMemory() const { return !m_episodes.isEmpty(); }

void MemoryBrowserModel::refresh() {
    setErrorMessage(QString{});

    if (!m_selectedEpisodeId.isEmpty()) {
        m_selectedEpisodeId.clear();
        emit selectedEpisodeIdChanged();
    }
    if (!m_selectedEpisodeTurns.isEmpty()) {
        m_selectedEpisodeTurns.clear();
        emit selectedEpisodeTurnsChanged();
    }

    m_pendingRequests = 2;
    setLoading(true);
    m_client->fetchEpisodes();
    m_client->fetchPreferences();
}

void MemoryBrowserModel::selectEpisode(const QString &episodeId) {
    if (m_selectedEpisodeId != episodeId) {
        m_selectedEpisodeId = episodeId;
        emit selectedEpisodeIdChanged();
    }
    if (!m_selectedEpisodeTurns.isEmpty()) {
        m_selectedEpisodeTurns.clear();
        emit selectedEpisodeTurnsChanged();
    }
    m_pendingRequests++;
    setLoading(true);
    m_client->fetchTurns(episodeId);
}

void MemoryBrowserModel::clearSelection() {
    if (!m_selectedEpisodeId.isEmpty()) {
        m_selectedEpisodeId.clear();
        emit selectedEpisodeIdChanged();
    }
    if (!m_selectedEpisodeTurns.isEmpty()) {
        m_selectedEpisodeTurns.clear();
        emit selectedEpisodeTurnsChanged();
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────

void MemoryBrowserModel::setLoading(bool val) {
    if (m_loading != val) {
        m_loading = val;
        emit loadingChanged();
    }
}

void MemoryBrowserModel::setErrorMessage(const QString &msg) {
    if (m_errorMessage != msg) {
        m_errorMessage = msg;
        emit errorMessageChanged();
    }
}

void MemoryBrowserModel::decrementPending() {
    if (m_pendingRequests > 0)
        m_pendingRequests--;
    if (m_pendingRequests == 0)
        setLoading(false);
}

QString MemoryBrowserModel::formatTimestamp(const QString &iso) {
    if (iso.isEmpty())
        return iso;
    QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid())
        dt = QDateTime::fromString(iso, Qt::ISODateWithMs);
    if (!dt.isValid())
        return iso;
    return dt.toLocalTime().toString(QStringLiteral("MMM d, h:mm AP"));
}

} // namespace pairion::memory
