#pragma once

/**
 * @file logger.h
 * @brief Centralized logging with QLoggingCategory, structured JSON formatting,
 *        and batched forwarding to the Pairion Server via POST /v1/logs.
 *
 * Installs a custom Qt message handler that captures all qDebug/qInfo/qWarning/qCritical
 * output, formats it as structured JSON, and batches records for periodic forwarding.
 */

#include <functional>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>

namespace pairion::state {
class ConnectionState;
} // namespace pairion::state

namespace pairion::util {

/**
 * @brief Structured log record matching the ClientLogRecord schema from openapi.yaml.
 */
struct LogRecord {
    QString timestamp;
    QString level;
    QString message;
    QString sessionId;
};

/**
 * @brief Centralized logger that batches log records and forwards them to the server.
 *
 * Installs itself as the Qt message handler. Records are buffered in memory and
 * flushed to POST /v1/logs every 30 seconds. Also provides a callback for feeding
 * log entries into the ConnectionState for the debug panel display.
 */
class Logger : public QObject {
    Q_OBJECT
  public:
    /**
     * @brief Construct the logger.
     * @param nam QNetworkAccessManager for HTTP POSTs (not owned).
     * @param baseUrl REST API base URL (e.g. "http://localhost:18789/v1").
     * @param parent QObject parent.
     */
    explicit Logger(QNetworkAccessManager *nam, const QString &baseUrl, QObject *parent = nullptr);

    ~Logger() override;

    /**
     * @brief Install the Qt message handler and start the flush timer.
     */
    void install();

    /**
     * @brief Set the session ID to include in log records.
     * @param sessionId Current session identifier.
     */
    void setSessionId(const QString &sessionId);

    /**
     * @brief Set a callback invoked for each log record (used to feed the debug panel).
     * @param cb Callback receiving a formatted log string.
     */
    void setLogCallback(std::function<void(const QString &)> cb);

    /**
     * @brief Flush all buffered records immediately via HTTP POST.
     */
    void flush();

    /**
     * @brief Get the current batch of pending log records (for testing).
     * @return List of pending log record JSON objects.
     */
    QJsonArray pendingRecords() const;

    /**
     * @brief Get the count of pending log records.
     * @return Number of records awaiting flush.
     */
    int pendingCount() const;

    /**
     * @brief Static message handler installed via qInstallMessageHandler.
     */
    static void messageHandler(QtMsgType type, const QMessageLogContext &context,
                               const QString &msg);

  private:
    /**
     * @brief Convert a QtMsgType to a level string matching the openapi.yaml enum.
     * @param type Qt message type.
     * @return Level string (TRACE, DEBUG, INFO, WARN, ERROR).
     */
    static QString levelString(QtMsgType type);

    /**
     * @brief Enqueue a log record into the batch buffer.
     * @param record The log record to buffer.
     */
    void enqueue(const LogRecord &record);

    QNetworkAccessManager *m_nam;
    QUrl m_logUrl;
    QTimer m_flushTimer;
    mutable QMutex m_mutex;
    QList<LogRecord> m_buffer;
    QString m_sessionId;
    std::function<void(const QString &)> m_logCallback;

    static Logger *s_instance;
};

} // namespace pairion::util
