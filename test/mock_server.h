#pragma once

/**
 * @file mock_server.h
 * @brief Mock WebSocket server for integration testing the Pairion client.
 *
 * Implements the server side of the Pairion AsyncAPI protocol deterministically:
 * responds to DeviceIdentify with SessionOpened, echoes HeartbeatPing as HeartbeatPong,
 * and provides methods to inject arbitrary messages for test scenarios.
 */

#include <functional>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QWebSocket>
#include <QWebSocketServer>
#include <vector>

namespace pairion::test {

/**
 * @brief A QWebSocketServer-based mock that speaks the Pairion AsyncAPI protocol.
 *
 * Listens on a random available port. Test code can retrieve the URL via url().
 * By default, responds to DeviceIdentify with SessionOpened and echoes heartbeats.
 */
class MockServer : public QObject {
    Q_OBJECT
  public:
    /**
     * @brief Construct and start listening on a random port.
     * @param parent QObject parent.
     */
    explicit MockServer(QObject *parent = nullptr);

    ~MockServer() override;

    /**
     * @brief The WebSocket URL to connect to (e.g. "ws://localhost:12345").
     */
    QUrl url() const;

    /**
     * @brief The port the mock server is listening on.
     */
    quint16 port() const;

    /**
     * @brief Send a raw text message to the first connected client.
     * @param json JSON string to send.
     */
    void sendToClient(const QString &json);

    /**
     * @brief Send a JSON object to the first connected client.
     * @param obj JSON object to send.
     */
    void sendToClient(const QJsonObject &obj);

    /**
     * @brief Send a binary message to the first connected client.
     * @param data Binary data.
     */
    void sendBinaryToClient(const QByteArray &data);

    /**
     * @brief Close the connection to the first connected client.
     */
    void disconnectClient();

    /**
     * @brief All text messages received from the client, in order.
     */
    const std::vector<QString> &receivedMessages() const;

    /**
     * @brief All binary messages received from the client, in order.
     */
    const std::vector<QByteArray> &receivedBinaryMessages() const;

    /**
     * @brief Whether a client is currently connected.
     */
    bool hasClient() const;

    /**
     * @brief Set a custom handler for incoming text messages (replaces default behavior).
     * @param handler Function receiving the message text and the client socket.
     */
    void setMessageHandler(std::function<void(const QString &, QWebSocket *)> handler);

    /**
     * @brief Stop the server and close all connections.
     */
    void stop();

  signals:
    /// Emitted when a client connects.
    void clientConnected();
    /// Emitted when a client disconnects.
    void clientDisconnected();
    /// Emitted when a text message is received.
    void messageReceived(const QString &message);

  private slots:
    void onNewConnection();

  private:
    void defaultMessageHandler(const QString &message, QWebSocket *client);

    QWebSocketServer m_server;
    QWebSocket *m_client = nullptr;
    std::vector<QString> m_received;
    std::vector<QByteArray> m_receivedBinary;
    std::function<void(const QString &, QWebSocket *)> m_handler;
};

} // namespace pairion::test
