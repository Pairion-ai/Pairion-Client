/**
 * @file logger.cpp
 * @brief Implementation of the centralized Pairion logger with batched forwarding.
 */

#include "logger.h"

#include "../core/constants.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace pairion::util {

Logger *Logger::s_instance = nullptr;

Logger::Logger(QNetworkAccessManager *nam, const QString &baseUrl, QObject *parent)
    : QObject(parent), m_nam(nam), m_logUrl(baseUrl + QStringLiteral("/logs")) {
    s_instance = this;
    m_flushTimer.setInterval(pairion::kLogFlushIntervalMs);
    connect(&m_flushTimer, &QTimer::timeout, this, &Logger::flush);
}

Logger::~Logger() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void Logger::install() {
    qInstallMessageHandler(Logger::messageHandler);
    m_flushTimer.start();
}

void Logger::setSessionId(const QString &sessionId) {
    QMutexLocker lock(&m_mutex);
    m_sessionId = sessionId;
}

void Logger::setLogCallback(std::function<void(const QString &)> cb) {
    m_logCallback = cb;
}

void Logger::flush() {
    QList<LogRecord> batch;
    {
        QMutexLocker lock(&m_mutex);
        if (m_buffer.isEmpty()) {
            return;
        }
        batch = std::move(m_buffer);
        m_buffer.clear();
    }

    QJsonArray arr;
    for (const auto &rec : batch) {
        QJsonObject obj;
        obj[QStringLiteral("timestamp")] = rec.timestamp;
        obj[QStringLiteral("level")] = rec.level;
        obj[QStringLiteral("message")] = rec.message;
        if (!rec.sessionId.isEmpty()) {
            obj[QStringLiteral("sessionId")] = rec.sessionId;
        }
        arr.append(obj);
    }

    QNetworkRequest req(m_logUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply *reply = m_nam->post(req, QJsonDocument(arr).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

QJsonArray Logger::pendingRecords() const {
    QMutexLocker lock(&m_mutex);
    QJsonArray arr;
    for (const auto &rec : m_buffer) {
        QJsonObject obj;
        obj[QStringLiteral("timestamp")] = rec.timestamp;
        obj[QStringLiteral("level")] = rec.level;
        obj[QStringLiteral("message")] = rec.message;
        if (!rec.sessionId.isEmpty()) {
            obj[QStringLiteral("sessionId")] = rec.sessionId;
        }
        arr.append(obj);
    }
    return arr;
}

int Logger::pendingCount() const {
    QMutexLocker lock(&m_mutex);
    return m_buffer.size();
}

void Logger::messageHandler(QtMsgType type, const QMessageLogContext & /*context*/,
                            const QString &msg) {
    if (s_instance == nullptr) {
        return;
    }

    LogRecord record;
    record.timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    record.level = levelString(type);
    record.message = msg;

    {
        QMutexLocker lock(&s_instance->m_mutex);
        record.sessionId = s_instance->m_sessionId;
    }

    s_instance->enqueue(record);

    if (s_instance->m_logCallback) {
        QString formatted =
            QStringLiteral("[%1] %2: %3").arg(record.timestamp, record.level, record.message);
        s_instance->m_logCallback(formatted);
    }
}

QString Logger::levelString(QtMsgType type) {
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO");
    case QtWarningMsg:
        return QStringLiteral("WARN");
    case QtCriticalMsg:
        return QStringLiteral("ERROR");
    case QtFatalMsg:
        return QStringLiteral("ERROR");
    }
    return QStringLiteral("DEBUG");
}

void Logger::enqueue(const LogRecord &record) {
    QMutexLocker lock(&m_mutex);
    m_buffer.append(record);
}

} // namespace pairion::util
