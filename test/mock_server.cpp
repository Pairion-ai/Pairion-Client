/**
 * @file mock_server.cpp
 * @brief Implementation of the mock WebSocket server for testing.
 */

#include "mock_server.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace pairion::test {

MockServer::MockServer(QObject *parent)
    : QObject(parent),
      m_server(QStringLiteral("PairionMockServer"), QWebSocketServer::NonSecureMode, this) {
    if (!m_server.listen(QHostAddress::LocalHost, 0)) {
        qFatal("MockServer: failed to listen: %s", qPrintable(m_server.errorString()));
    }
    connect(&m_server, &QWebSocketServer::newConnection, this, &MockServer::onNewConnection);
}

MockServer::~MockServer() {
    stop();
}

QUrl MockServer::url() const {
    return QUrl(QStringLiteral("ws://localhost:%1").arg(m_server.serverPort()));
}

quint16 MockServer::port() const {
    return m_server.serverPort();
}

void MockServer::sendToClient(const QString &json) {
    if (m_client != nullptr) {
        m_client->sendTextMessage(json);
    }
}

void MockServer::sendToClient(const QJsonObject &obj) {
    sendToClient(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void MockServer::sendBinaryToClient(const QByteArray &data) {
    if (m_client != nullptr) {
        m_client->sendBinaryMessage(data);
    }
}

void MockServer::disconnectClient() {
    if (m_client != nullptr) {
        m_client->close();
    }
}

const std::vector<QString> &MockServer::receivedMessages() const {
    return m_received;
}

const std::vector<QByteArray> &MockServer::receivedBinaryMessages() const {
    return m_receivedBinary;
}

bool MockServer::hasClient() const {
    return m_client != nullptr && m_client->state() == QAbstractSocket::ConnectedState;
}

void MockServer::setMessageHandler(std::function<void(const QString &, QWebSocket *)> handler) {
    m_handler = std::move(handler);
}

void MockServer::stop() {
    if (m_client != nullptr) {
        m_client->close();
        m_client->deleteLater();
        m_client = nullptr;
    }
    m_server.close();
}

void MockServer::onNewConnection() {
    m_client = m_server.nextPendingConnection();
    if (m_client == nullptr) {
        return;
    }

    connect(m_client, &QWebSocket::textMessageReceived, this, [this](const QString &msg) {
        m_received.push_back(msg);
        emit messageReceived(msg);
        if (m_handler) {
            m_handler(msg, m_client);
        } else {
            defaultMessageHandler(msg, m_client);
        }
    });

    connect(m_client, &QWebSocket::binaryMessageReceived, this,
            [this](const QByteArray &data) { m_receivedBinary.push_back(data); });

    connect(m_client, &QWebSocket::disconnected, this, [this]() {
        emit clientDisconnected();
        if (m_client != nullptr) {
            m_client->deleteLater();
            m_client = nullptr;
        }
    });

    emit clientConnected();
}

void MockServer::defaultMessageHandler(const QString &message, QWebSocket *client) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        return;
    }
    QJsonObject obj = doc.object();
    QString type = obj[QStringLiteral("type")].toString();

    if (type == QLatin1String("DeviceIdentify")) {
        QJsonObject reply;
        reply[QStringLiteral("type")] = QStringLiteral("SessionOpened");
        reply[QStringLiteral("sessionId")] = QStringLiteral("test-session-001");
        reply[QStringLiteral("serverVersion")] = QStringLiteral("2.0.0-test");
        client->sendTextMessage(
            QString::fromUtf8(QJsonDocument(reply).toJson(QJsonDocument::Compact)));
    } else if (type == QLatin1String("HeartbeatPing")) {
        QJsonObject reply;
        reply[QStringLiteral("type")] = QStringLiteral("HeartbeatPong");
        reply[QStringLiteral("timestamp")] = obj[QStringLiteral("timestamp")].toString();
        client->sendTextMessage(
            QString::fromUtf8(QJsonDocument(reply).toJson(QJsonDocument::Compact)));
    }
}

} // namespace pairion::test
