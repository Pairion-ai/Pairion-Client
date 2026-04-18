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

QString ConnectionState::transcriptPartial() const {
    return m_transcriptPartial;
}

QString ConnectionState::transcriptFinal() const {
    return m_transcriptFinal;
}

QString ConnectionState::llmResponse() const {
    return m_llmResponse;
}

QString ConnectionState::agentState() const {
    return m_agentState;
}

QString ConnectionState::voiceState() const {
    return m_voiceState;
}

void ConnectionState::setTranscriptPartial(const QString &text) {
    if (m_transcriptPartial != text) {
        m_transcriptPartial = text;
        emit transcriptPartialChanged();
    }
}

void ConnectionState::setTranscriptFinal(const QString &text) {
    if (m_transcriptFinal != text) {
        m_transcriptFinal = text;
        emit transcriptFinalChanged();
    }
}

void ConnectionState::setAgentState(const QString &state) {
    if (m_agentState != state) {
        m_agentState = state;
        emit agentStateChanged();
    }
}

void ConnectionState::setVoiceState(const QString &state) {
    if (m_voiceState != state) {
        m_voiceState = state;
        emit voiceStateChanged();
    }
}

void ConnectionState::appendLlmToken(const QString &delta) {
    m_llmResponse.append(delta);
    emit llmResponseChanged();
}

void ConnectionState::clearLlmResponse() {
    if (!m_llmResponse.isEmpty()) {
        m_llmResponse.clear();
        emit llmResponseChanged();
    }
}

} // namespace pairion::state
