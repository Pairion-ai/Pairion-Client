/**
 * @file pairion_websocket_client.cpp
 * @brief Implementation of the Pairion WebSocket client.
 */

#include "pairion_websocket_client.h"

#include "../core/constants.h"
#include "../protocol/envelope_codec.h"

#include <QDateTime>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcWs, "pairion.ws")

namespace pairion::ws {

PairionWebSocketClient::PairionWebSocketClient(const QUrl &serverUrl, const QString &deviceId,
                                               const QString &bearerToken,
                                               pairion::state::ConnectionState *connectionState,
                                               QObject *parent)
    : QObject(parent), m_serverUrl(serverUrl), m_deviceId(deviceId), m_bearerToken(bearerToken),
      m_connState(connectionState) {

    connect(&m_socket, &QWebSocket::connected, this, &PairionWebSocketClient::onConnected);
    connect(&m_socket, &QWebSocket::disconnected, this, &PairionWebSocketClient::onDisconnected);
    connect(&m_socket, &QWebSocket::textMessageReceived, this,
            &PairionWebSocketClient::onTextMessageReceived);
    connect(&m_socket, &QWebSocket::binaryMessageReceived, this,
            &PairionWebSocketClient::onBinaryMessageReceived);

    m_heartbeatTimer.setInterval(pairion::kHeartbeatIntervalMs);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &PairionWebSocketClient::onHeartbeatTick);

    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &PairionWebSocketClient::onReconnectTick);
}

void PairionWebSocketClient::connectToServer() {
    m_intentionalDisconnect = false;
    m_connState->setStatus(state::ConnectionState::Connecting);
    qCInfo(lcWs) << "Connecting to" << m_serverUrl.toString();
    m_socket.open(m_serverUrl);
}

void PairionWebSocketClient::disconnectFromServer() {
    m_intentionalDisconnect = true;
    stopHeartbeat();
    m_reconnectTimer.stop();
    m_socket.close();
    m_connState->setStatus(state::ConnectionState::Disconnected);
    m_connState->setSessionId(QString());
    m_connState->setServerVersion(QString());
    m_connState->setReconnectAttempts(0);
}

void PairionWebSocketClient::sendMessage(const protocol::OutboundMessage &msg) {
    QString json = protocol::EnvelopeCodec::serializeToString(msg);
    m_socket.sendTextMessage(json);
}

bool PairionWebSocketClient::isConnected() const {
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

void PairionWebSocketClient::setHeartbeatIntervalMs(int ms) {
    m_heartbeatTimer.setInterval(ms);
}

void PairionWebSocketClient::sendBinaryFrame(const QByteArray &data) {
    m_socket.sendBinaryMessage(data);
}

void PairionWebSocketClient::onConnected() {
    qCInfo(lcWs) << "Connected, sending DeviceIdentify";
    m_reconnectAttempt = 0;
    m_connState->setReconnectAttempts(0);

    protocol::DeviceIdentify identify;
    identify.deviceId = m_deviceId;
    identify.bearerToken = m_bearerToken;
    identify.clientVersion = QString::fromUtf8(pairion::kClientVersion);
    sendMessage(identify);

    startHeartbeat();
}

void PairionWebSocketClient::onDisconnected() {
    qCInfo(lcWs) << "Disconnected";
    stopHeartbeat();
    m_connState->setSessionId(QString());
    m_connState->setServerVersion(QString());
    emit disconnected();

    if (!m_intentionalDisconnect) {
        scheduleReconnect();
    } else {
        m_connState->setStatus(state::ConnectionState::Disconnected);
    }
}

void PairionWebSocketClient::onTextMessageReceived(const QString &message) {
    auto parsed = protocol::EnvelopeCodec::deserializeFromString(message);
    if (!parsed.has_value()) {
        qCWarning(lcWs) << "Unknown or malformed message:" << message;
        return;
    }

    const auto &msg = parsed.value();
    emit inboundMessage(msg);

    std::visit(
        [this](const auto &m) {
            using T = std::decay_t<decltype(m)>;

            if constexpr (std::is_same_v<T, protocol::SessionOpened>) {
                qCInfo(lcWs) << "Session opened:" << m.sessionId << "server:" << m.serverVersion;
                m_connState->setStatus(state::ConnectionState::Connected);
                m_connState->setSessionId(m.sessionId);
                m_connState->setServerVersion(m.serverVersion);
                emit sessionOpened(m.sessionId, m.serverVersion);
            } else if constexpr (std::is_same_v<T, protocol::SessionClosed>) {
                qCInfo(lcWs) << "Session closed:" << m.reason;
                emit sessionClosed(m.reason);
            } else if constexpr (std::is_same_v<T, protocol::HeartbeatPong>) {
                qCDebug(lcWs) << "Heartbeat pong received";
            } else if constexpr (std::is_same_v<T, protocol::ErrorMessage>) {
                qCWarning(lcWs) << "Server error:" << m.code << m.message;
                emit serverError(m.code, m.message);
            } else if constexpr (std::is_same_v<T, protocol::TranscriptPartial>) {
                m_connState->setTranscriptPartial(m.text);
                emit transcriptPartialReceived(m.text);
            } else if constexpr (std::is_same_v<T, protocol::TranscriptFinal>) {
                qCInfo(lcWs) << "TranscriptFinal received:" << m.text;
                m_connState->setTranscriptFinal(m.text);
                emit transcriptFinalReceived(m.text);
            } else if constexpr (std::is_same_v<T, protocol::AgentStateChange>) {
                if (m.state == QLatin1String("thinking")) {
                    m_connState->clearLlmResponse();
                }
                m_connState->setAgentState(m.state);
                emit agentStateReceived(m.state);
            } else if constexpr (std::is_same_v<T, protocol::LlmTokenStream>) {
                m_connState->appendLlmToken(m.delta);
                emit llmTokenReceived(m.delta);
            } else if constexpr (std::is_same_v<T, protocol::AudioStreamStartOut>) {
                qCInfo(lcWs) << "Inbound audio stream start:" << m.streamId;
                emit audioStreamStartOutReceived(m.streamId);
            } else if constexpr (std::is_same_v<T, protocol::AudioStreamEndOut>) {
                qCInfo(lcWs) << "Inbound audio stream end:" << m.streamId << m.reason;
                emit audioStreamEndOutReceived(m.streamId, m.reason);
            } else if constexpr (std::is_same_v<T, protocol::MapFocus>) {
                qCInfo(lcWs) << "Map focus:" << m.label << "zoom:" << m.zoom;
                m_connState->setMapFocus(m.lat, m.lon, m.label, m.zoom);
            } else if constexpr (std::is_same_v<T, protocol::MapClear>) {
                qCInfo(lcWs) << "Map clear";
                m_connState->clearMapFocus();
            } else if constexpr (std::is_same_v<T, protocol::BackgroundChange>) {
                qCInfo(lcWs) << "Background change: backgroundId=" << m.backgroundId
                              << "transition=" << m.transition;
                m_connState->setBackground(m.backgroundId, m.params, m.transition);
            } else if constexpr (std::is_same_v<T, protocol::OverlayAdd>) {
                qCInfo(lcWs) << "Overlay add: overlayId=" << m.overlayId;
                m_connState->addOverlay(m.overlayId, m.params);
            } else if constexpr (std::is_same_v<T, protocol::OverlayRemove>) {
                qCInfo(lcWs) << "Overlay remove: overlayId=" << m.overlayId;
                m_connState->removeOverlay(m.overlayId);
            } else if constexpr (std::is_same_v<T, protocol::OverlayClear>) {
                qCInfo(lcWs) << "Overlay clear";
                m_connState->clearOverlays();
            } else if constexpr (std::is_same_v<T, protocol::SceneDataPush>) {
                qCInfo(lcWs) << "Scene data push: modelId=" << m.modelId;
                m_connState->setSceneData(m.modelId, m.data.toVariant());
            }
        },
        msg);
}

void PairionWebSocketClient::onBinaryMessageReceived(const QByteArray &data) {
    qCDebug(lcWs) << "Binary frame received, size:" << data.size() << "(discarding at M0)";
    emit binaryFrameReceived(data);
}

void PairionWebSocketClient::onHeartbeatTick() {
    protocol::HeartbeatPing ping;
    ping.timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    sendMessage(ping);
    qCDebug(lcWs) << "Heartbeat ping sent";
}

void PairionWebSocketClient::onReconnectTick() {
    connectToServer();
}

void PairionWebSocketClient::startHeartbeat() {
    m_heartbeatTimer.start();
}

void PairionWebSocketClient::stopHeartbeat() {
    m_heartbeatTimer.stop();
}

void PairionWebSocketClient::scheduleReconnect() {
    m_connState->setStatus(state::ConnectionState::Reconnecting);
    m_reconnectAttempt++;
    m_connState->setReconnectAttempts(m_reconnectAttempt);
    int backoff = currentBackoffMs();
    qCInfo(lcWs) << "Reconnecting in" << backoff << "ms (attempt" << m_reconnectAttempt << ")";
    m_reconnectTimer.start(backoff);
}

int PairionWebSocketClient::currentBackoffMs() const {
    int idx = m_reconnectAttempt - 1;
    // LCOV_EXCL_START
    // Unreachable: currentBackoffMs() is only called from scheduleReconnect() which
    // increments m_reconnectAttempt before calling, guaranteeing idx >= 0.
    // Guard retained to prevent array OOB if calling context changes in future refactors.
    if (idx < 0) {
        idx = 0;
    }
    // Unreachable via current test suite: would require 7+ consecutive reconnection
    // failures without the mock server accepting the connection. Guard retained to
    // cap the backoff array index and prevent OOB access.
    if (idx >= pairion::kReconnectBackoffSteps) {
        idx = pairion::kReconnectBackoffSteps - 1;
    }
    // LCOV_EXCL_STOP
    return pairion::kReconnectBackoffMs[idx];
}

} // namespace pairion::ws
