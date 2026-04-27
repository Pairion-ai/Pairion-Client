#pragma once

/**
 * @file memory_browser_model.h
 * @brief QML-exposed singleton model for the memory browser panel.
 *
 * Exposes episodes, turns, and preferences to QML via Q_PROPERTY bindings.
 * Delegates HTTP communication to MemoryClient. Registered in QML as the
 * singleton "MemoryBrowserModel" in the Pairion module.
 */

#include <QObject>
#include <QString>
#include <QVariantList>

namespace pairion::memory {
class MemoryClient;
} // namespace pairion::memory

namespace pairion::memory {

/**
 * @brief Singleton model backing the memory browser panel.
 *
 * Wraps MemoryClient with QML-friendly Q_PROPERTY state. Tracks in-flight
 * request count so loading becomes false only after ALL pending requests
 * complete. Timestamps are formatted from ISO 8601 to a human-readable
 * locale string before being stored.
 */
class MemoryBrowserModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList episodes READ episodes NOTIFY episodesChanged)
    Q_PROPERTY(QVariantList selectedEpisodeTurns READ selectedEpisodeTurns
                   NOTIFY selectedEpisodeTurnsChanged)
    Q_PROPERTY(QVariantList preferences READ preferences NOTIFY preferencesChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString selectedEpisodeId READ selectedEpisodeId NOTIFY selectedEpisodeIdChanged)
    Q_PROPERTY(bool hasMemory READ hasMemory NOTIFY hasMemoryChanged)

  public:
    /**
     * @brief Construct the model.
     * @param client MemoryClient for HTTP requests (not owned). Must outlive this object.
     * @param parent QObject parent.
     */
    explicit MemoryBrowserModel(MemoryClient *client, QObject *parent = nullptr);

    /**
     * @brief List of episode objects, each with keys: id, title, startTime, turnCount.
     * @return Current episode list.
     */
    QVariantList episodes() const;

    /**
     * @brief Turn objects for the selected episode, each with keys: role, content, timestamp.
     * @return Turns for the currently selected episode, or empty if none selected.
     */
    QVariantList selectedEpisodeTurns() const;

    /**
     * @brief Preference objects, each with keys: key, value, updatedAt.
     * @return Current preference list.
     */
    QVariantList preferences() const;

    /**
     * @brief True while any HTTP request is in-flight.
     * @return Current loading state.
     */
    bool loading() const;

    /**
     * @brief Human-readable description of the last fetch error, or empty string.
     * @return Current error message.
     */
    QString errorMessage() const;

    /**
     * @brief The episode ID currently selected for turn display, or empty string.
     * @return Currently selected episode ID.
     */
    QString selectedEpisodeId() const;

    /**
     * @brief True when the episodes list is non-empty.
     * @return Whether any episodes are loaded.
     */
    bool hasMemory() const;

  public slots:
    /**
     * @brief Refresh episodes and preferences from the server.
     *
     * Clears selectedEpisodeId and turns before issuing the two parallel requests.
     * Sets loading to true until both complete.
     */
    void refresh();

    /**
     * @brief Select an episode and fetch its turns.
     *
     * Sets selectedEpisodeId and clears turns immediately; sets loading until
     * turnsReceived or fetchError arrives.
     * @param episodeId Episode identifier to select.
     */
    void selectEpisode(const QString &episodeId);

    /**
     * @brief Clear the selected episode ID and its turn list.
     */
    void clearSelection();

  signals:
    /// @brief Emitted when the episodes list changes.
    void episodesChanged();
    /// @brief Emitted when the selected episode's turn list changes.
    void selectedEpisodeTurnsChanged();
    /// @brief Emitted when the preferences list changes.
    void preferencesChanged();
    /// @brief Emitted when the loading state changes.
    void loadingChanged();
    /// @brief Emitted when errorMessage changes.
    void errorMessageChanged();
    /// @brief Emitted when selectedEpisodeId changes.
    void selectedEpisodeIdChanged();
    /// @brief Emitted when hasMemory changes.
    void hasMemoryChanged();

  private:
    /**
     * @brief Format an ISO 8601 timestamp to a human-readable local-time string.
     *
     * E.g. "2024-01-15T10:30:00Z" → "Jan 15, 10:30 AM". Returns the original
     * string unchanged if it cannot be parsed.
     * @param iso ISO 8601 timestamp string.
     * @return Formatted display string.
     */
    static QString formatTimestamp(const QString &iso);

    /**
     * @brief Set loading and emit loadingChanged if the value changed.
     * @param val New loading state.
     */
    void setLoading(bool val);

    /**
     * @brief Set errorMessage and emit errorMessageChanged if the value changed.
     * @param msg New error message.
     */
    void setErrorMessage(const QString &msg);

    /**
     * @brief Decrement the pending-request counter; set loading false when it reaches zero.
     */
    void decrementPending();

    MemoryClient *m_client;                 ///< HTTP client (not owned).
    QVariantList m_episodes;                ///< Cached episode list.
    QVariantList m_selectedEpisodeTurns;    ///< Turns for the selected episode.
    QVariantList m_preferences;             ///< Cached preference list.
    bool m_loading = false;                 ///< True while requests are in-flight.
    QString m_errorMessage;                 ///< Last error, or empty.
    QString m_selectedEpisodeId;            ///< Currently selected episode ID.
    int m_pendingRequests = 0;              ///< Count of in-flight requests.
};

} // namespace pairion::memory
