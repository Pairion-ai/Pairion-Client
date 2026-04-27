/**
 * @file memory_client.cpp
 * @brief HTTP client for the Pairion memory REST endpoints.
 */

#include "memory/memory_client.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(lcMemoryClient, "pairion.memory.client")

namespace pairion::memory {

MemoryClient::MemoryClient(QNetworkAccessManager *nam, const QString &baseUrl, QObject *parent)
    : QObject(parent), m_nam(nam), m_baseUrl(baseUrl) {}

void MemoryClient::fetchEpisodes() {
    QUrl url(m_baseUrl.toString() + QStringLiteral("/memory/episodes"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), QStringLiteral("default-user"));
    url.setQuery(query);
    get(url, QStringLiteral("fetchEpisodes"), [this](const QJsonArray &arr) {
        emit episodesReceived(arr);
    });
}

void MemoryClient::fetchTurns(const QString &episodeId) {
    QUrl url(m_baseUrl.toString() + QStringLiteral("/memory/episodes/") + episodeId +
             QStringLiteral("/turns"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), QStringLiteral("default-user"));
    url.setQuery(query);
    get(url, QStringLiteral("fetchTurns"), [this](const QJsonArray &arr) {
        emit turnsReceived(arr);
    });
}

void MemoryClient::fetchPreferences() {
    QUrl url(m_baseUrl.toString() + QStringLiteral("/memory/preferences"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), QStringLiteral("default-user"));
    url.setQuery(query);
    get(url, QStringLiteral("fetchPreferences"), [this](const QJsonArray &arr) {
        emit preferencesReceived(arr);
    });
}

void MemoryClient::get(const QUrl &url, const QString &operation,
                       std::function<void(const QJsonArray &)> onSuccess) {
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply *reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, operation, onSuccess]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcMemoryClient) << operation << "failed:" << reply->errorString();
            emit fetchError(operation, reply->errorString());
            return;
        }

        QByteArray body = reply->readAll();
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(body, &err);
        if (err.error != QJsonParseError::NoError) {
            qCWarning(lcMemoryClient) << operation << "JSON parse error:" << err.errorString();
            emit fetchError(operation,
                            QStringLiteral("JSON parse error: ") + err.errorString());
            return;
        }

        if (doc.isArray()) {
            onSuccess(doc.array());
        } else {
            // Some endpoints wrap the list in {"items": [...]}.
            onSuccess(doc.object().value(QStringLiteral("items")).toArray());
        }
    });
}

} // namespace pairion::memory
