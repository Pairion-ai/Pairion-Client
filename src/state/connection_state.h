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
    Q_ENUM(Status)

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

  private:
    Status m_status = Disconnected;
    QString m_sessionId;
    QString m_serverVersion;
    int m_reconnectAttempts = 0;
    QStringList m_recentLogs;
};

} // namespace pairion::state
