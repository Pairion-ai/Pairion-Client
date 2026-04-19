#pragma once

/**
 * @file pairion_websocket_client.h
 * @brief WebSocket client for the Pairion server protocol.
 *
 * Wraps QWebSocket to manage the full client lifecycle: connect, identify,
 * heartbeat, reconnect with exponential backoff, and inbound message dispatch.
 */

#include "../protocol/messages.h"
#include "../state/connection_state.h"

#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

namespace pairion::ws {

/**
 * @brief QWebSocket-based client implementing the Pairion AsyncAPI protocol.
 *
 * On connection, sends DeviceIdentify with the device credentials. Handles
 * SessionOpened, HeartbeatPong, Error, and SessionClosed inbound messages.
 * Maintains a 15-second heartbeat cadence. On disconnect, reconnects with
 * exponential backoff (1s → 2s → 4s → 8s → 15s → 30s cap).
 *
 * Threading: lives on the main thread event loop. All signal/slot connections
 * use auto-connection.
 */
class PairionWebSocketClient : public QObject {
    Q_OBJECT
  public:
    /**
     * @brief Construct the WebSocket client.
     * @param serverUrl WebSocket server URL (e.g. "ws://localhost:18789/ws/v1").
     * @param deviceId Device unique identifier.
     * @param bearerToken Device bearer token.
     * @param connectionState Shared connection state object for QML binding.
     * @param parent QObject parent.
     */
    PairionWebSocketClient(const QUrl &serverUrl, const QString &deviceId,
                           const QString &bearerToken,
                           pairion::state::ConnectionState *connectionState,
                           QObject *parent = nullptr);

    /**
     * @brief Initiate connection to the server.
     */
    void connectToServer();

    /**
     * @brief Disconnect and stop reconnection attempts.
     */
    void disconnectFromServer();

    /**
     * @brief Send an outbound text-frame message.
     * @param msg The message variant to serialize and send.
     */
    void sendMessage(const protocol::OutboundMessage &msg);

    /**
     * @brief Whether the WebSocket is currently connected.
     */
    bool isConnected() const;

    /**
     * @brief Override the heartbeat interval (for testing).
     * @param ms Interval in milliseconds.
     */
    void setHeartbeatIntervalMs(int ms);

    /**
     * @brief Send a raw binary frame over the WebSocket.
     * @param data Binary data (4-byte stream ID prefix + payload).
     */
    void sendBinaryFrame(const QByteArray &data);

  signals:
    /// Emitted when a session is successfully opened.
    void sessionOpened(const QString &sessionId, const QString &serverVersion);
    /// Emitted when the server closes the session.
    void sessionClosed(const QString &reason);
    /// Emitted when the server sends an error.
    void serverError(const QString &code, const QString &message);
    /// Emitted when connection is lost.
    void disconnected();
    /// Emitted for any inbound message (for extensibility).
    void inboundMessage(const protocol::InboundMessage &msg);
    /// Emitted when a binary frame arrives (log + discard at M0).
    void binaryFrameReceived(const QByteArray &data);
    /// Emitted when a TranscriptPartial message arrives.
    void transcriptPartialReceived(const QString &text);
    /// Emitted when a TranscriptFinal message arrives.
    void transcriptFinalReceived(const QString &text);
    /// Emitted when an AgentStateChange message arrives.
    void agentStateReceived(const QString &state);
    /// Emitted when an LlmTokenStream message arrives.
    void llmTokenReceived(const QString &delta);
    /// Emitted when an AudioStreamStart message arrives from the server.
    void audioStreamStartOutReceived(const QString &streamId);
    /// Emitted when an AudioStreamEnd message arrives from the server.
    void audioStreamEndOutReceived(const QString &streamId, const QString &reason);

  private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void onBinaryMessageReceived(const QByteArray &data);
    void onHeartbeatTick();
    void onReconnectTick();

  private:
    void startHeartbeat();
    void stopHeartbeat();
    void scheduleReconnect();
    int currentBackoffMs() const;

    QWebSocket m_socket;
    QUrl m_serverUrl;
    QString m_deviceId;
    QString m_bearerToken;
    pairion::state::ConnectionState *m_connState;

    QTimer m_heartbeatTimer;
    QTimer m_reconnectTimer;
    int m_reconnectAttempt = 0;
    bool m_intentionalDisconnect = false;
};

} // namespace pairion::ws
