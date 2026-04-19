/**
 * @file tst_ws_client_state_machine.cpp
 * @brief Tests for the PairionWebSocketClient state machine: connect, identify,
 *        heartbeat, disconnect, and reconnect scenarios.
 */

#include "../src/state/connection_state.h"
#include "../src/ws/pairion_websocket_client.h"
#include "mock_server.h"

#include <QSignalSpy>
#include <QTest>

using namespace pairion::ws;
using namespace pairion::state;
using namespace pairion::test;

class TestWsClientStateMachine : public QObject {
    Q_OBJECT

  private slots:
    /// Happy path: connect, identify, receive SessionOpened.
    void connectAndIdentify() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient client(server.url(), QStringLiteral("dev-1"),
                                      QStringLiteral("tok-1"), &connState);

        QSignalSpy sessionSpy(&client, &PairionWebSocketClient::sessionOpened);
        client.connectToServer();

        QVERIFY(sessionSpy.wait(5000));
        QCOMPARE(sessionSpy.count(), 1);
        QCOMPARE(sessionSpy.at(0).at(0).toString(), QStringLiteral("test-session-001"));
        QCOMPARE(sessionSpy.at(0).at(1).toString(), QStringLiteral("2.0.0-test"));

        QCOMPARE(connState.status(), ConnectionState::Connected);
        QCOMPARE(connState.sessionId(), QStringLiteral("test-session-001"));
        QCOMPARE(connState.serverVersion(), QStringLiteral("2.0.0-test"));
        QCOMPARE(connState.reconnectAttempts(), 0);

        // Verify DeviceIdentify was sent
        QVERIFY(!server.receivedMessages().empty());
        QJsonDocument doc = QJsonDocument::fromJson(server.receivedMessages().front().toUtf8());
        QCOMPARE(doc.object()["type"].toString(), QStringLiteral("DeviceIdentify"));
        QCOMPARE(doc.object()["deviceId"].toString(), QStringLiteral("dev-1"));
        QCOMPARE(doc.object()["bearerToken"].toString(), QStringLiteral("tok-1"));

        client.disconnectFromServer();
    }

    /// Verify status transitions through Connecting → Connected.
    void statusTransitions() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient client(server.url(), QStringLiteral("dev-2"),
                                      QStringLiteral("tok-2"), &connState);

        QCOMPARE(connState.status(), ConnectionState::Disconnected);

        QSignalSpy statusSpy(&connState, &ConnectionState::statusChanged);
        QSignalSpy sessionSpy(&client, &PairionWebSocketClient::sessionOpened);
        client.connectToServer();
        QCOMPARE(connState.status(), ConnectionState::Connecting);

        QVERIFY(sessionSpy.wait(5000));
        QCOMPARE(connState.status(), ConnectionState::Connected);

        client.disconnectFromServer();
        QCOMPARE(connState.status(), ConnectionState::Disconnected);
    }

    /// Verify heartbeat ping is sent after the heartbeat interval.
    void heartbeatPingSent() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient client(server.url(), QStringLiteral("dev-3"),
                                      QStringLiteral("tok-3"), &connState);

        // Use a very short heartbeat interval for testing
        client.setHeartbeatIntervalMs(200);

        QSignalSpy sessionSpy(&client, &PairionWebSocketClient::sessionOpened);
        client.connectToServer();
        QVERIFY(sessionSpy.wait(5000));

        // Wait for at least one heartbeat tick
        QTest::qWait(500);

        // Verify a HeartbeatPing was sent (after the DeviceIdentify)
        bool foundPing = false;
        for (const auto &msg : server.receivedMessages()) {
            QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
            if (doc.object()["type"].toString() == QLatin1String("HeartbeatPing")) {
                QVERIFY(!doc.object()["timestamp"].toString().isEmpty());
                foundPing = true;
                break;
            }
        }
        QVERIFY(foundPing);

        client.disconnectFromServer();
    }

    /// Verify server disconnect triggers reconnection.
    void reconnectOnDisconnect() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient client(server.url(), QStringLiteral("dev-4"),
                                      QStringLiteral("tok-4"), &connState);

        QSignalSpy sessionSpy(&client, &PairionWebSocketClient::sessionOpened);
        client.connectToServer();
        QVERIFY(sessionSpy.wait(5000));

        QSignalSpy disconnectSpy(&client, &PairionWebSocketClient::disconnected);
        server.disconnectClient();
        QVERIFY(disconnectSpy.wait(5000));

        // State should be Reconnecting
        QCOMPARE(connState.status(), ConnectionState::Reconnecting);
        QCOMPARE(connState.reconnectAttempts(), 1);

        // Session ID and version should be cleared on disconnect
        QVERIFY(connState.sessionId().isEmpty());
        QVERIFY(connState.serverVersion().isEmpty());

        // Wait for reconnect (1s backoff)
        QVERIFY(sessionSpy.wait(5000));
        QCOMPARE(connState.status(), ConnectionState::Connected);
        QCOMPARE(connState.reconnectAttempts(), 0);

        client.disconnectFromServer();
    }

    /// Verify intentional disconnect does not trigger reconnection.
    void intentionalDisconnectNoReconnect() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient client(server.url(), QStringLiteral("dev-5"),
                                      QStringLiteral("tok-5"), &connState);

        QSignalSpy sessionSpy(&client, &PairionWebSocketClient::sessionOpened);
        client.connectToServer();
        QVERIFY(sessionSpy.wait(5000));

        client.disconnectFromServer();
        QCOMPARE(connState.status(), ConnectionState::Disconnected);
        QCOMPARE(connState.reconnectAttempts(), 0);

        // Wait to ensure no reconnection happens
        QTest::qWait(1500);
        QCOMPARE(connState.status(), ConnectionState::Disconnected);
    }

    /// Verify server error message is forwarded.
    void serverErrorForwarded() {
        MockServer server;
        server.setMessageHandler([](const QString &msg, QWebSocket *client) {
            QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
            QString type = doc.object()["type"].toString();
            if (type == QLatin1String("DeviceIdentify")) {
                QJsonObject reply;
                reply["type"] = QStringLiteral("Error");
                reply["code"] = QStringLiteral("AUTH_FAILED");
                reply["message"] = QStringLiteral("Bad token");
                client->sendTextMessage(
                    QString::fromUtf8(QJsonDocument(reply).toJson(QJsonDocument::Compact)));
            }
        });

        ConnectionState connState;
        PairionWebSocketClient client(server.url(), QStringLiteral("dev-6"),
                                      QStringLiteral("tok-bad"), &connState);

        QSignalSpy errorSpy(&client, &PairionWebSocketClient::serverError);
        client.connectToServer();

        QVERIFY(errorSpy.wait(5000));
        QCOMPARE(errorSpy.at(0).at(0).toString(), QStringLiteral("AUTH_FAILED"));
        QCOMPARE(errorSpy.at(0).at(1).toString(), QStringLiteral("Bad token"));

        client.disconnectFromServer();
    }

    /// Verify session closed signal is emitted.
    void sessionClosedForwarded() {
        MockServer server;
        server.setMessageHandler([](const QString &msg, QWebSocket *client) {
            QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
            QString type = doc.object()["type"].toString();
            if (type == QLatin1String("DeviceIdentify")) {
                QJsonObject reply;
                reply["type"] = QStringLiteral("SessionClosed");
                reply["reason"] = QStringLiteral("server_shutdown");
                client->sendTextMessage(
                    QString::fromUtf8(QJsonDocument(reply).toJson(QJsonDocument::Compact)));
            }
        });

        ConnectionState connState;
        PairionWebSocketClient client(server.url(), QStringLiteral("dev-7"),
                                      QStringLiteral("tok-7"), &connState);

        QSignalSpy closedSpy(&client, &PairionWebSocketClient::sessionClosed);
        client.connectToServer();

        QVERIFY(closedSpy.wait(5000));
        QCOMPARE(closedSpy.at(0).at(0).toString(), QStringLiteral("server_shutdown"));

        client.disconnectFromServer();
    }

    /// Verify binary frame signal is emitted.
    void binaryFrameReceived() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient client(server.url(), QStringLiteral("dev-8"),
                                      QStringLiteral("tok-8"), &connState);

        QSignalSpy sessionSpy(&client, &PairionWebSocketClient::sessionOpened);
        client.connectToServer();
        QVERIFY(sessionSpy.wait(5000));

        QSignalSpy binarySpy(&client, &PairionWebSocketClient::binaryFrameReceived);
        QByteArray testData("binary_test_data");
        server.sendBinaryToClient(testData);

        QVERIFY(binarySpy.wait(5000));
        QCOMPARE(binarySpy.at(0).at(0).toByteArray(), testData);

        client.disconnectFromServer();
    }

    /// Verify inboundMessage signal is emitted for recognized messages.
    void inboundMessageSignalEmitted() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient client(server.url(), QStringLiteral("dev-9"),
                                      QStringLiteral("tok-9"), &connState);

        QSignalSpy inboundSpy(&client, &PairionWebSocketClient::inboundMessage);
        QSignalSpy sessionSpy(&client, &PairionWebSocketClient::sessionOpened);
        client.connectToServer();

        QVERIFY(sessionSpy.wait(5000));
        // SessionOpened should have triggered inboundMessage
        QVERIFY(inboundSpy.count() >= 1);

        client.disconnectFromServer();
    }

    /// Verify unknown messages are logged but don't crash.
    void unknownMessageHandled() {
        MockServer server;
        server.setMessageHandler([](const QString &msg, QWebSocket *client) {
            QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
            QString type = doc.object()["type"].toString();
            if (type == QLatin1String("DeviceIdentify")) {
                // Send unknown message type
                QJsonObject reply;
                reply["type"] = QStringLiteral("FutureMessageType");
                reply["data"] = QStringLiteral("something");
                client->sendTextMessage(
                    QString::fromUtf8(QJsonDocument(reply).toJson(QJsonDocument::Compact)));
            }
        });

        ConnectionState connState;
        PairionWebSocketClient client(server.url(), QStringLiteral("dev-10"),
                                      QStringLiteral("tok-10"), &connState);

        client.connectToServer();
        // Just verify no crash
        QTest::qWait(1000);

        client.disconnectFromServer();
    }

    /// Verify TranscriptPartial and TranscriptFinal messages are forwarded.
    void transcriptMessagesForwarded() {
        MockServer server;
        server.setMessageHandler([](const QString &msg, QWebSocket *client) {
            QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
            if (doc.object()["type"].toString() == QLatin1String("DeviceIdentify")) {
                // Send SessionOpened then TranscriptPartial then TranscriptFinal
                QJsonObject opened;
                opened["type"] = QStringLiteral("SessionOpened");
                opened["sessionId"] = QStringLiteral("s1");
                opened["serverVersion"] = QStringLiteral("1.0");
                client->sendTextMessage(
                    QString::fromUtf8(QJsonDocument(opened).toJson(QJsonDocument::Compact)));

                QJsonObject partial;
                partial["type"] = QStringLiteral("TranscriptPartial");
                partial["text"] = QStringLiteral("hell");
                client->sendTextMessage(
                    QString::fromUtf8(QJsonDocument(partial).toJson(QJsonDocument::Compact)));

                QJsonObject final_;
                final_["type"] = QStringLiteral("TranscriptFinal");
                final_["text"] = QStringLiteral("hello");
                client->sendTextMessage(
                    QString::fromUtf8(QJsonDocument(final_).toJson(QJsonDocument::Compact)));
            }
        });

        ConnectionState connState;
        PairionWebSocketClient client(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                      &connState);
        QSignalSpy partialSpy(&client, &PairionWebSocketClient::transcriptPartialReceived);
        QSignalSpy finalSpy(&client, &PairionWebSocketClient::transcriptFinalReceived);
        client.connectToServer();

        QVERIFY(partialSpy.wait(5000));
        QCOMPARE(partialSpy.at(0).at(0).toString(), QStringLiteral("hell"));
        QCOMPARE(connState.transcriptPartial(), QStringLiteral("hell"));

        QVERIFY(finalSpy.count() > 0 || finalSpy.wait(2000));
        QCOMPARE(connState.transcriptFinal(), QStringLiteral("hello"));

        client.disconnectFromServer();
    }

    /// Verify LlmTokenStream messages are forwarded and appendLlmToken is called.
    void llmTokenStreamForwarded() {
        MockServer server;
        server.setMessageHandler([](const QString &msg, QWebSocket *client) {
            QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
            if (doc.object()["type"].toString() == QLatin1String("DeviceIdentify")) {
                QJsonObject opened;
                opened["type"] = QStringLiteral("SessionOpened");
                opened["sessionId"] = QStringLiteral("s2");
                opened["serverVersion"] = QStringLiteral("1.0");
                client->sendTextMessage(
                    QString::fromUtf8(QJsonDocument(opened).toJson(QJsonDocument::Compact)));

                QJsonObject tok;
                tok["type"] = QStringLiteral("LlmTokenStream");
                tok["delta"] = QStringLiteral("Hi");
                client->sendTextMessage(
                    QString::fromUtf8(QJsonDocument(tok).toJson(QJsonDocument::Compact)));
            }
        });

        ConnectionState connState;
        PairionWebSocketClient client(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                      &connState);
        QSignalSpy tokenSpy(&client, &PairionWebSocketClient::llmTokenReceived);
        client.connectToServer();

        QVERIFY(tokenSpy.wait(5000));
        QCOMPARE(tokenSpy.at(0).at(0).toString(), QStringLiteral("Hi"));
        QCOMPARE(connState.llmResponse(), QStringLiteral("Hi"));

        client.disconnectFromServer();
    }

    /// Verify AgentStateChange "thinking" clears the LLM response accumulator.
    void agentStateThinkingClearsLlm() {
        MockServer server;
        server.setMessageHandler([](const QString &msg, QWebSocket *client) {
            QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
            if (doc.object()["type"].toString() == QLatin1String("DeviceIdentify")) {
                QJsonObject opened;
                opened["type"] = QStringLiteral("SessionOpened");
                opened["sessionId"] = QStringLiteral("s3");
                opened["serverVersion"] = QStringLiteral("1.0");
                client->sendTextMessage(
                    QString::fromUtf8(QJsonDocument(opened).toJson(QJsonDocument::Compact)));

                // First: append a token to the LLM response
                QJsonObject tok;
                tok["type"] = QStringLiteral("LlmTokenStream");
                tok["delta"] = QStringLiteral("text");
                client->sendTextMessage(
                    QString::fromUtf8(QJsonDocument(tok).toJson(QJsonDocument::Compact)));

                // Then: AgentStateChange "thinking" should clear it
                QJsonObject acs;
                acs["type"] = QStringLiteral("AgentStateChange");
                acs["state"] = QStringLiteral("thinking");
                client->sendTextMessage(
                    QString::fromUtf8(QJsonDocument(acs).toJson(QJsonDocument::Compact)));
            }
        });

        ConnectionState connState;
        PairionWebSocketClient client(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                      &connState);
        QSignalSpy agentSpy(&client, &PairionWebSocketClient::agentStateReceived);
        client.connectToServer();

        QVERIFY(agentSpy.wait(5000));
        QCOMPARE(agentSpy.at(0).at(0).toString(), QStringLiteral("thinking"));
        QVERIFY(connState.llmResponse().isEmpty());

        client.disconnectFromServer();
    }

    /// Verify appendLog stores entries and recentLogs returns them.
    void appendLogAndRecentLogs() {
        ConnectionState connState;
        QSignalSpy logSpy(&connState, &ConnectionState::recentLogsChanged);

        connState.appendLog(QStringLiteral("entry1"));
        connState.appendLog(QStringLiteral("entry2"));

        QCOMPARE(logSpy.count(), 2);
        QStringList logs = connState.recentLogs();
        // appendLog prepends — entry2 is first
        QCOMPARE(logs.at(0), QStringLiteral("entry2"));
        QCOMPARE(logs.at(1), QStringLiteral("entry1"));
    }

    /// Verify appendLog trims to 50 when more than 50 entries are added.
    void appendLogTrimsAt50() {
        ConnectionState connState;
        for (int i = 0; i < 55; ++i) {
            connState.appendLog(QStringLiteral("log%1").arg(i));
        }
        QCOMPARE(connState.recentLogs().size(), 50);
    }

    /// Verify sendMessage sends arbitrary outbound messages.
    void sendMessageWorks() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient client(server.url(), QStringLiteral("dev-11"),
                                      QStringLiteral("tok-11"), &connState);

        QSignalSpy sessionSpy(&client, &PairionWebSocketClient::sessionOpened);
        client.connectToServer();
        QVERIFY(sessionSpy.wait(5000));

        pairion::protocol::TextMessage tm;
        tm.text = QStringLiteral("Hello server");
        client.sendMessage(tm);

        QTest::qWait(500);

        bool found = false;
        for (const auto &msg : server.receivedMessages()) {
            QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
            if (doc.object()["type"].toString() == QLatin1String("TextMessage")) {
                QCOMPARE(doc.object()["text"].toString(), QStringLiteral("Hello server"));
                found = true;
                break;
            }
        }
        QVERIFY(found);

        client.disconnectFromServer();
    }
};

QTEST_GUILESS_MAIN(TestWsClientStateMachine)
#include "tst_ws_client_state_machine.moc"
