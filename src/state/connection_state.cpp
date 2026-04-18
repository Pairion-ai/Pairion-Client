/**
 * @file connection_state.cpp
 * @brief Implementation of the ConnectionState QML singleton.
 */

#include "connection_state.h"

namespace pairion::state {

ConnectionState::ConnectionState(QObject *parent) : QObject(parent) {}

ConnectionState::Status ConnectionState::status() const {
    return m_status;
}

QString ConnectionState::sessionId() const {
    return m_sessionId;
}

QString ConnectionState::serverVersion() const {
    return m_serverVersion;
}

int ConnectionState::reconnectAttempts() const {
    return m_reconnectAttempts;
}

QStringList ConnectionState::recentLogs() const {
    return m_recentLogs;
}

void ConnectionState::setStatus(Status s) {
    if (m_status != s) {
        m_status = s;
        emit statusChanged();
    }
}

void ConnectionState::setSessionId(const QString &id) {
    if (m_sessionId != id) {
        m_sessionId = id;
        emit sessionIdChanged();
    }
}

void ConnectionState::setServerVersion(const QString &version) {
    if (m_serverVersion != version) {
        m_serverVersion = version;
        emit serverVersionChanged();
    }
}

void ConnectionState::setReconnectAttempts(int count) {
    if (m_reconnectAttempts != count) {
        m_reconnectAttempts = count;
        emit reconnectAttemptsChanged();
    }
}

void ConnectionState::appendLog(const QString &entry) {
    m_recentLogs.prepend(entry);
    while (m_recentLogs.size() > 10) {
        m_recentLogs.removeLast();
    }
    emit recentLogsChanged();
}

} // namespace pairion::state
