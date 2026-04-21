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
    while (m_recentLogs.size() > 50) {
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

QString ConnectionState::backgroundContext() const {
    return m_backgroundContext;
}

void ConnectionState::setBackgroundContext(const QString &context) {
    if (m_backgroundContext != context) {
        m_backgroundContext = context;
        emit backgroundContextChanged();
    }
}

bool ConnectionState::mapFocusActive() const {
    return m_mapFocusActive;
}

double ConnectionState::mapFocusLat() const {
    return m_mapFocusLat;
}

double ConnectionState::mapFocusLon() const {
    return m_mapFocusLon;
}

QString ConnectionState::mapFocusLabel() const {
    return m_mapFocusLabel;
}

QString ConnectionState::mapFocusZoom() const {
    return m_mapFocusZoom;
}

void ConnectionState::setMapFocus(double lat, double lon, const QString &label,
                                   const QString &zoom) {
    m_mapFocusLat = lat;
    m_mapFocusLon = lon;
    m_mapFocusLabel = label;
    m_mapFocusZoom = zoom;
    m_mapFocusActive = true;
    emit mapFocusChanged();
}

void ConnectionState::clearMapFocus() {
    if (m_mapFocusActive) {
        m_mapFocusActive = false;
        emit mapFocusChanged();
    }
}

} // namespace pairion::state
