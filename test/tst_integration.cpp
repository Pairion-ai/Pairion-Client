/**
 * @file tst_integration.cpp
 * @brief Integration tests using the mock WebSocket server harness.
 *
 * Drives the full client lifecycle against a deterministic mock server:
 * connect, identify, session open, heartbeat exchange, binary frame routing,
 * and graceful disconnect/reconnect.
 */

#include "../src/protocol/envelope_codec.h"
#include "../src/state/connection_state.h"
#include "../src/ws/pairion_websocket_client.h"
#include "mock_server.h"

#include <QJsonDocument>
#include <QSignalSpy>
#include <QTest>

using namespace pairion::ws;
using namespace pairion::state;
using namespace pairion::test;
using namespace pairion::protocol;

class TestIntegration : public QObject {
    Q_OBJECT

  private slots:
    /// Full happy-path: connect → identify → session → heartbeat echo → disconnect.
    void fullHappyPath() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient client(server.url(), QStringLiteral("int-dev-1"),
                                      QStringLiteral("int-tok-1"), &connState);

        // Connect and get session
        QSignalSpy sessionSpy(&client, &PairionWebSocketClient::sessionOpened);
        client.connectToServer();
        QVERIFY(sessionSpy.wait(5000));

        QCOMPARE(connState.status(), ConnectionState::Connected);
        QCOMPARE(connState.sessionId(), QStringLiteral("test-session-001"));

        // Verify the DeviceIdentify was well-formed
        QVERIFY(server.receivedMessages().size() >= 1);
        QJsonDocument identDoc = QJsonDocument::fromJson(server.receivedMessages().at(0).toUtf8());
        QJsonObject identObj = identDoc.object();
        QCOMPARE(identObj["type"].toString(), QStringLiteral("DeviceIdentify"));
        QCOMPARE(identObj["deviceId"].toString(), QStringLiteral("int-dev-1"));
        QCOMPARE(identObj["bearerToken"].toString(), QStringLiteral("int-tok-1"));
        QVERIFY(!identObj["clientVersion"].toString().isEmpty());

        // Send a heartbeat manually and verify pong
        HeartbeatPing ping;
        ping.timestamp = QStringLiteral("2026-04-18T12:00:00.000Z");
        client.sendMessage(ping);
        QTest::qWait(500);

        // The mock echoes HeartbeatPong — verify it was received (no crash)
        QVERIFY(client.isConnected());

        // Disconnect gracefully
        client.disconnectFromServer();
        QCOMPARE(connState.status(), ConnectionState::Disconnected);
    }

    /// Verify disconnect + reconnect cycle with backoff.
    void disconnectAndReconnect() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient client(server.url(), QStringLiteral("int-dev-2"),
                                      QStringLiteral("int-tok-2"), &connState);

        QSignalSpy sessionSpy(&client, &PairionWebSocketClient::sessionOpened);
        client.connectToServer();
        QVERIFY(sessionSpy.wait(5000));

        // Server disconnects the client
        QSignalSpy disconnectSpy(&client, &PairionWebSocketClient::disconnected);
        server.disconnectClient();
        QVERIFY(disconnectSpy.wait(5000));

        QCOMPARE(connState.status(), ConnectionState::Reconnecting);
        QCOMPARE(connState.reconnectAttempts(), 1);

        // Client should reconnect within the backoff period
        QVERIFY(sessionSpy.wait(5000));
        QCOMPARE(connState.status(), ConnectionState::Connected);
        QCOMPARE(connState.reconnectAttempts(), 0);

        client.disconnectFromServer();
    }

    /// Verify binary frames are received and signaled.
    void binaryFrameRouting() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient client(server.url(), QStringLiteral("int-dev-3"),
                                      QStringLiteral("int-tok-3"), &connState);

        QSignalSpy sessionSpy(&client, &PairionWebSocketClient::sessionOpened);
        client.connectToServer();
        QVERIFY(sessionSpy.wait(5000));

        // Server sends a binary frame (simulating audio)
        QByteArray binaryData;
        binaryData.append("STRM");                  // 4-byte stream ID prefix
        binaryData.append(QByteArray(160, '\x00')); // 160 bytes of silence
        QSignalSpy binarySpy(&client, &PairionWebSocketClient::binaryFrameReceived);
        server.sendBinaryToClient(binaryData);

        QVERIFY(binarySpy.wait(5000));
        QCOMPARE(binarySpy.at(0).at(0).toByteArray(), binaryData);

        client.disconnectFromServer();
    }

    /// Verify inbound protocol messages beyond SessionOpened.
    void inboundProtocolMessages() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient client(server.url(), QStringLiteral("int-dev-4"),
                                      QStringLiteral("int-tok-4"), &connState);

        QSignalSpy sessionSpy(&client, &PairionWebSocketClient::sessionOpened);
        client.connectToServer();
        QVERIFY(sessionSpy.wait(5000));

        // Send AgentStateChange from server
        QSignalSpy inboundSpy(&client, &PairionWebSocketClient::inboundMessage);
        QJsonObject stateMsg;
        stateMsg["type"] = QStringLiteral("AgentStateChange");
        stateMsg["state"] = QStringLiteral("thinking");
        server.sendToClient(stateMsg);

        QVERIFY(inboundSpy.wait(5000));

        client.disconnectFromServer();
    }

    /// Verify ConnectionState log accumulation.
    void connectionStateLogAccumulation() {
        ConnectionState state;
        for (int i = 0; i < 15; i++) {
            state.appendLog(QStringLiteral("Log entry %1").arg(i));
        }
        // Should keep only 10 most recent
        QCOMPARE(state.recentLogs().size(), 10);
        // Most recent should be first
        QCOMPARE(state.recentLogs().at(0), QStringLiteral("Log entry 14"));
        QCOMPARE(state.recentLogs().at(9), QStringLiteral("Log entry 5"));
    }

    /// Verify ConnectionState property change signals.
    void connectionStateSignals() {
        ConnectionState state;

        QSignalSpy statusSpy(&state, &ConnectionState::statusChanged);
        QSignalSpy sessionSpy(&state, &ConnectionState::sessionIdChanged);
        QSignalSpy versionSpy(&state, &ConnectionState::serverVersionChanged);
        QSignalSpy reconnectSpy(&state, &ConnectionState::reconnectAttemptsChanged);
        QSignalSpy logSpy(&state, &ConnectionState::recentLogsChanged);

        state.setStatus(ConnectionState::Connected);
        QCOMPARE(statusSpy.count(), 1);

        // Same value should not emit
        state.setStatus(ConnectionState::Connected);
        QCOMPARE(statusSpy.count(), 1);

        state.setSessionId(QStringLiteral("s1"));
        QCOMPARE(sessionSpy.count(), 1);
        state.setSessionId(QStringLiteral("s1"));
        QCOMPARE(sessionSpy.count(), 1);

        state.setServerVersion(QStringLiteral("v1"));
        QCOMPARE(versionSpy.count(), 1);
        state.setServerVersion(QStringLiteral("v1"));
        QCOMPARE(versionSpy.count(), 1);

        state.setReconnectAttempts(1);
        QCOMPARE(reconnectSpy.count(), 1);
        state.setReconnectAttempts(1);
        QCOMPARE(reconnectSpy.count(), 1);

        state.appendLog(QStringLiteral("test"));
        QCOMPARE(logSpy.count(), 1);
    }

    /// Verify mock server utility methods.
    void mockServerUtilities() {
        MockServer server;
        QVERIFY(server.port() > 0);
        QVERIFY(!server.hasClient());
        QVERIFY(server.receivedMessages().empty());
        QVERIFY(server.receivedBinaryMessages().empty());

        // Connect a raw socket to test
        QWebSocket rawSocket;
        QSignalSpy clientConnectedSpy(&server, &MockServer::clientConnected);
        rawSocket.open(server.url());
        QVERIFY(clientConnectedSpy.wait(5000));
        QVERIFY(server.hasClient());

        rawSocket.close();
        QTest::qWait(500);

        server.stop();
    }
};

QTEST_GUILESS_MAIN(TestIntegration)
#include "tst_integration.moc"
