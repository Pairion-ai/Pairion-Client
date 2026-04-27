#pragma once

/**
 * @file memory_client.h
 * @brief HTTP client for the Pairion memory REST endpoints.
 *
 * Issues non-blocking GET requests to /v1/memory/episodes,
 * /v1/memory/episodes/{id}/turns, and /v1/memory/preferences.
 * All results are delivered via signals; errors via fetchError.
 */

#include <functional>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>

namespace pairion::memory {

/**
 * @brief HTTP client for fetching episodes, turns, and preferences from the memory REST API.
 *
 * Uses a shared QNetworkAccessManager injected at construction time (not owned).
 * All requests are non-blocking; results are delivered via signals on the thread
 * that created the object. Errors (network or parse) are delivered via fetchError.
 */
class MemoryClient : public QObject {
    Q_OBJECT

  public:
    /**
     * @brief Construct the memory client.
     * @param nam QNetworkAccessManager for HTTP GETs (not owned).
     * @param baseUrl REST API base URL (e.g. "http://localhost:18789/v1").
     * @param parent QObject parent.
     */
    explicit MemoryClient(QNetworkAccessManager *nam, const QString &baseUrl,
                          QObject *parent = nullptr);

    /**
     * @brief Fetch all memory episodes for the default user.
     *
     * Issues GET /v1/memory/episodes?userId=default-user.
     * Emits episodesReceived on success, fetchError on failure.
     */
    void fetchEpisodes();

    /**
     * @brief Fetch conversation turns for a specific episode.
     *
     * Issues GET /v1/memory/episodes/{episodeId}/turns?userId=default-user.
     * Emits turnsReceived on success, fetchError on failure.
     * @param episodeId The episode identifier.
     */
    void fetchTurns(const QString &episodeId);

    /**
     * @brief Fetch user preferences.
     *
     * Issues GET /v1/memory/preferences?userId=default-user.
     * Emits preferencesReceived on success, fetchError on failure.
     */
    void fetchPreferences();

  signals:
    /**
     * @brief Emitted when episodes have been successfully fetched.
     * @param episodes Array of episode objects from the server.
     */
    void episodesReceived(const QJsonArray &episodes);

    /**
     * @brief Emitted when turns for an episode have been successfully fetched.
     * @param turns Array of turn objects from the server.
     */
    void turnsReceived(const QJsonArray &turns);

    /**
     * @brief Emitted when preferences have been successfully fetched.
     * @param preferences Array of preference objects from the server.
     */
    void preferencesReceived(const QJsonArray &preferences);

    /**
     * @brief Emitted when an HTTP request fails (network error or JSON parse error).
     * @param operation Human-readable operation name (e.g. "fetchEpisodes").
     * @param message Error description.
     */
    void fetchError(const QString &operation, const QString &message);

  private:
    /**
     * @brief Issue a GET request and deliver the parsed result via callback.
     *
     * On success, calls onSuccess with the parsed QJsonArray.
     * Arrays and objects with an "items" key are both handled.
     * On network error or JSON parse failure, emits fetchError.
     *
     * @param url The fully-qualified request URL.
     * @param operation Operation label for error reporting.
     * @param onSuccess Callback invoked with the parsed array on success.
     */
    void get(const QUrl &url, const QString &operation,
             std::function<void(const QJsonArray &)> onSuccess);

    QNetworkAccessManager *m_nam;  ///< Shared network access manager (not owned).
    QUrl m_baseUrl;                ///< REST API base URL.
};

} // namespace pairion::memory
