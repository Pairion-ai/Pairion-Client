/**
 * @file main.cpp
 * @brief Entry point for the Pairion Client application.
 *
 * Creates the Qt application, initializes the device identity,
 * sets up the WebSocket client and logging infrastructure, registers QML
 * singletons, and loads the root QML window.
 */

#include "core/constants.h"
#include "core/device_identity.h"
#include "state/connection_state.h"
#include "util/logger.h"
#include "ws/pairion_websocket_client.h"

#include <QGuiApplication>
#include <QNetworkAccessManager>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Pairion"));
    app.setApplicationVersion(QString::fromUtf8(pairion::kClientVersion));
    app.setOrganizationName(QStringLiteral("Pairion"));

    auto *nam = new QNetworkAccessManager(&app);
    auto *connState = new pairion::state::ConnectionState(&app);

    auto *logger =
        new pairion::util::Logger(nam, QString::fromUtf8(pairion::kDefaultRestBaseUrl), &app);
    logger->setLogCallback([connState](const QString &entry) { connState->appendLog(entry); });
    logger->install();

    pairion::core::DeviceIdentity identity;

    auto *wsClient = new pairion::ws::PairionWebSocketClient(
        QUrl(QString::fromUtf8(pairion::kDefaultServerUrl)), identity.deviceId(),
        identity.bearerToken(), connState, &app);

    QObject::connect(wsClient, &pairion::ws::PairionWebSocketClient::sessionOpened, logger,
                     [logger](const QString &sessionId, const QString & /*version*/) {
                         logger->setSessionId(sessionId);
                     });

    qmlRegisterSingletonInstance("Pairion", 1, 0, "ConnectionState", connState);

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    wsClient->connectToServer();

    return app.exec();
}
