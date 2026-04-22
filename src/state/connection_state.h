#pragma once

/**
 * @file connection_state.h
 * @brief QML-exposed singleton tracking WebSocket connection state.
 *
 * ConnectionState is registered as a QML singleton so that the debug panel
 * (and later the HUD) can bind to connection status, session ID, server version,
 * and reconnection metadata via Qt property bindings.
 */

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace pairion::state {

/**
 * @brief Singleton QObject exposing WebSocket connection state to QML.
 *
 * Properties update via signals whenever the WebSocket client reports
 * state transitions. QML components bind to these properties declaratively.
 */
class ConnectionState : public QObject {
    Q_OBJECT

    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString sessionId READ sessionId NOTIFY sessionIdChanged)
    Q_PROPERTY(QString serverVersion READ serverVersion NOTIFY serverVersionChanged)
    Q_PROPERTY(int reconnectAttempts READ reconnectAttempts NOTIFY reconnectAttemptsChanged)
    Q_PROPERTY(QStringList recentLogs READ recentLogs NOTIFY recentLogsChanged)
    Q_PROPERTY(QString transcriptPartial READ transcriptPartial NOTIFY transcriptPartialChanged)
    Q_PROPERTY(QString transcriptFinal READ transcriptFinal NOTIFY transcriptFinalChanged)
    Q_PROPERTY(QString llmResponse READ llmResponse NOTIFY llmResponseChanged)
    Q_PROPERTY(QString agentState READ agentState NOTIFY agentStateChanged)
    Q_PROPERTY(QString voiceState READ voiceState NOTIFY voiceStateChanged)
    Q_PROPERTY(QString backgroundContext READ backgroundContext NOTIFY backgroundContextChanged)
    Q_PROPERTY(bool mapFocusActive READ mapFocusActive NOTIFY mapFocusChanged)
    Q_PROPERTY(double mapFocusLat READ mapFocusLat NOTIFY mapFocusChanged)
    Q_PROPERTY(double mapFocusLon READ mapFocusLon NOTIFY mapFocusChanged)
    Q_PROPERTY(QString mapFocusLabel READ mapFocusLabel NOTIFY mapFocusChanged)
    Q_PROPERTY(QString mapFocusZoom READ mapFocusZoom NOTIFY mapFocusChanged)
    Q_PROPERTY(QString activeSceneId READ activeSceneId NOTIFY activeSceneIdChanged)
    Q_PROPERTY(QVariantMap sceneData READ sceneData NOTIFY sceneDataChanged)
    Q_PROPERTY(QVariantMap sceneParams READ sceneParams NOTIFY sceneParamsChanged)
    Q_PROPERTY(QString sceneTransition READ sceneTransition NOTIFY sceneTransitionChanged)

  public:
    /**
     * @brief Connection status enumeration exposed to QML.
     */
    enum Status {
        Disconnected = 0,
        Connecting,
        Connected,
        Reconnecting,
    };
    // Unreachable: Q_ENUM macro generates qt_getEnumName/qt_getEnumMetaObject introspection
    // functions that are only invoked by Qt's internal meta-object system, not by application code
    Q_ENUM(Status) // LCOV_EXCL_LINE

    /**
     * @brief Construct with optional parent.
     * @param parent QObject parent for lifetime management.
     */
    explicit ConnectionState(QObject *parent = nullptr);

    /**
     * @brief Current connection status.
     */
    Status status() const;

    /**
     * @brief Active session ID from the server, empty if not connected.
     */
    QString sessionId() const;

    /**
     * @brief Server version string from SessionOpened, empty if not connected.
     */
    QString serverVersion() const;

    /**
     * @brief Number of reconnection attempts since last successful connection.
     */
    int reconnectAttempts() const;

    /**
     * @brief The most recent log records (up to 10) for the debug panel.
     */
    QStringList recentLogs() const;

    /// Partial transcript from STT (in progress).
    QString transcriptPartial() const;
    /// Final transcript from STT.
    QString transcriptFinal() const;
    /// Accumulated LLM response tokens.
    QString llmResponse() const;
    /// Current agent state (idle, listening, thinking, speaking).
    QString agentState() const;
    /// Current voice pipeline state (idle, awaiting_wake, streaming, ending_speech).
    QString voiceState() const;
    /**
     * @brief Active background context driving the HUD scene behind the globe.
     * Values: "earth" (default globe view) or "space" (star-field scene).
     * Additional contexts can be added by extending ContextBackground.qml.
     */
    QString backgroundContext() const;

    /// @brief True when a MapFocus is active (server has focused the map on a location).
    bool mapFocusActive() const;
    /// @brief Latitude of the active map focus in decimal degrees.
    double mapFocusLat() const;
    /// @brief Longitude of the active map focus in decimal degrees.
    double mapFocusLon() const;
    /// @brief Human-readable label of the active map focus (e.g. "Tokyo, Japan").
    QString mapFocusLabel() const;
    /// @brief Zoom level of the active map focus: continent, country, region, or city.
    QString mapFocusZoom() const;
    /// @brief Active scene plugin identifier (e.g. "globe", "space", "dashboard").
    QString activeSceneId() const;
    /// @brief Accumulated scene model data keyed by modelId, updated via SceneDataPush.
    QVariantMap sceneData() const;
    /// @brief Scene parameters from the most recent SceneChange command.
    QVariantMap sceneParams() const;
    /// @brief Transition style from the most recent SceneChange command.
    QString sceneTransition() const;

  public slots:
    /**
     * @brief Update the connection status.
     * @param s New status value.
     */
    void setStatus(Status s);

    /**
     * @brief Store session ID from a SessionOpened message.
     * @param id Session identifier.
     */
    void setSessionId(const QString &id);

    /**
     * @brief Store server version from a SessionOpened message.
     * @param version Server version string.
     */
    void setServerVersion(const QString &version);

    /**
     * @brief Update the reconnection attempt counter.
     * @param count Current attempt number.
     */
    void setReconnectAttempts(int count);

    /**
     * @brief Append a log entry, keeping the list trimmed to 10 records.
     * @param entry Formatted log string.
     */
    void appendLog(const QString &entry);

    void setTranscriptPartial(const QString &text);
    void setTranscriptFinal(const QString &text);
    void setAgentState(const QString &state);
    void setVoiceState(const QString &state);
    /**
     * @brief Set the background context, emitting backgroundContextChanged if changed.
     * @param context Context string, e.g. "earth" or "space".
     */
    void setBackgroundContext(const QString &context);
    /// @brief Append an LLM token delta to the response accumulator.
    void appendLlmToken(const QString &delta);
    /// @brief Clear the accumulated LLM response.
    void clearLlmResponse();
    /**
     * @brief Activate a server-driven map focus on a geographic location.
     * @param lat latitude in decimal degrees
     * @param lon longitude in decimal degrees
     * @param label human-readable display label
     * @param zoom zoom level string: continent, country, region, or city
     */
    void setMapFocus(double lat, double lon, const QString &label, const QString &zoom);
    /// @brief Clear the active map focus and resume globe auto-scroll.
    void clearMapFocus();
    /**
     * @brief Set the active scene plugin identifier, emitting activeSceneIdChanged if changed.
     * @param sceneId Scene identifier, e.g. "globe", "space", or "dashboard".
     */
    void setActiveSceneId(const QString &sceneId);
    /**
     * @brief Accumulate model data for the given modelId, emitting sceneDataChanged.
     *
     * Accepts a QVariant so that both JSON-object payloads (QVariantMap) and
     * JSON-array payloads (QVariantList) are stored correctly. The ADS-B scene,
     * for example, pushes its aircraft list as a JSON array.
     *
     * @param modelId Key identifying the data model (e.g. "adsb", "news").
     * @param data Variant payload pushed by the server (QVariantMap or QVariantList).
     */
    void setSceneData(const QString &modelId, const QVariant &data);
    /// @brief Clear all accumulated scene data and emit sceneDataChanged.
    void clearSceneData();
    /**
     * @brief Replace the current scene parameters, emitting sceneParamsChanged if changed.
     * @param params Variant map of parameters from the SceneChange command.
     */
    void setSceneParams(const QVariantMap &params);
    /**
     * @brief Set the active scene transition style, emitting sceneTransitionChanged if changed.
     * @param transition Transition name: "crossfade", "instant", or "slide".
     */
    void setSceneTransition(const QString &transition);

  signals:
    /// Emitted when connection status changes.
    void statusChanged();
    /// Emitted when session ID changes.
    void sessionIdChanged();
    /// Emitted when server version changes.
    void serverVersionChanged();
    /// Emitted when reconnect attempt count changes.
    void reconnectAttemptsChanged();
    /// Emitted when the recent-logs list changes.
    void recentLogsChanged();
    void transcriptPartialChanged();
    void transcriptFinalChanged();
    void llmResponseChanged();
    void agentStateChanged();
    void voiceStateChanged();
    /// Emitted when the background context changes.
    void backgroundContextChanged();
    /// Emitted when the map focus state changes (set or cleared).
    void mapFocusChanged();
    /// Emitted when the active scene plugin identifier changes.
    void activeSceneIdChanged();
    /// Emitted when scene model data is updated (SceneDataPush or SceneClear).
    void sceneDataChanged();
    /// Emitted when scene parameters are updated (SceneChange).
    void sceneParamsChanged();
    /// Emitted when the scene transition style changes.
    void sceneTransitionChanged();

  private:
    Status m_status = Disconnected;
    QString m_sessionId;
    QString m_serverVersion;
    int m_reconnectAttempts = 0;
    QStringList m_recentLogs;
    QString m_transcriptPartial;
    QString m_transcriptFinal;
    QString m_llmResponse;
    QString m_agentState;
    QString m_voiceState;
    QString m_backgroundContext = QStringLiteral("earth");
    bool m_mapFocusActive = false;
    double m_mapFocusLat = 0.0;
    double m_mapFocusLon = 0.0;
    QString m_mapFocusLabel;
    QString m_mapFocusZoom;
    QString m_activeSceneId = QStringLiteral("globe");
    QVariantMap m_sceneData;
    QVariantMap m_sceneParams;
    QString m_sceneTransition = QStringLiteral("crossfade");
};

} // namespace pairion::state
