/**
 * @file main.cpp
 * @brief Entry point for the Pairion Client application.
 *
 * Creates the Qt application, initializes device identity, settings, logging,
 * model downloader, audio pipeline, and QML UI. Wires the full M1 upstream
 * pipeline: capture → wake → VAD → encode → WebSocket.
 */

#include "audio/pairion_audio_capture.h"
#include "audio/pairion_opus_encoder.h"
#include "core/constants.h"
#include "core/device_identity.h"
#include "core/model_downloader.h"
#include "core/onnx_session.h"
#include "pipeline/audio_session_orchestrator.h"
#include "settings/settings.h"
#include "state/connection_state.h"
#include "util/logger.h"
#include "vad/silero_vad.h"
#include "wake/open_wakeword_detector.h"
#include "ws/pairion_websocket_client.h"

#include <QGuiApplication>
#include <QNetworkAccessManager>
#include <QQmlApplicationEngine>
#include <QThread>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Pairion"));
    app.setApplicationVersion(QString::fromUtf8(pairion::kClientVersion));
    app.setOrganizationName(QStringLiteral("Pairion"));

    auto *nam = new QNetworkAccessManager(&app);
    auto *connState = new pairion::state::ConnectionState(&app);
    auto *settings = new pairion::settings::Settings(&app);

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

    // Audio pipeline components (created on main thread, moved to workers later)
    auto *capture = new pairion::audio::PairionAudioCapture();
    auto *encoder = new pairion::audio::PairionOpusEncoder();

    // Worker threads
    auto *audioThread = new QThread(&app);
    audioThread->setObjectName(QStringLiteral("AudioThread"));
    capture->moveToThread(audioThread);
    encoder->moveToThread(audioThread);

    // QML singletons
    qmlRegisterSingletonInstance("Pairion", 1, 0, "ConnectionState", connState);
    qmlRegisterSingletonInstance("Pairion", 1, 0, "Settings", settings);

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    // Model downloader — triggers pipeline start when ready
    auto *modelDownloader = new pairion::core::ModelDownloader(nam, &app);

    QObject::connect(
        modelDownloader, &pairion::core::ModelDownloader::allModelsReady, &app,
        [&app, capture, encoder, wsClient, connState, settings, audioThread]() {
            qInfo() << "All models ready, starting audio pipeline";

            // Create ONNX sessions for wake word + VAD
            QString modelDir = pairion::core::ModelDownloader::modelCacheDir();
            pairion::core::OnnxInferenceSession *melSession = nullptr;
            pairion::core::OnnxInferenceSession *embSession = nullptr;
            pairion::core::OnnxInferenceSession *clsSession = nullptr;
            pairion::core::OnnxInferenceSession *vadSession = nullptr;

            try {
                melSession = new pairion::core::OrtInferenceSession(
                    (modelDir + QStringLiteral("/melspectrogram.onnx")).toStdString());
                embSession = new pairion::core::OrtInferenceSession(
                    (modelDir + QStringLiteral("/embedding_model.onnx")).toStdString());
                clsSession = new pairion::core::OrtInferenceSession(
                    (modelDir + QStringLiteral("/hey_jarvis_v0.1.onnx")).toStdString());
                vadSession = new pairion::core::OrtInferenceSession(
                    (modelDir + QStringLiteral("/silero_vad.onnx")).toStdString());
            } catch (const std::exception &e) {
                qCritical() << "Failed to load ONNX models:" << e.what();
                return;
            }

            auto *wakeDetector = new pairion::wake::OpenWakewordDetector(
                melSession, embSession, clsSession, settings->wakeThreshold(), &app);
            auto *vad = new pairion::vad::SileroVad(vadSession, settings->vadThreshold(),
                                                    settings->vadSilenceEndMs(), &app);

            auto *orchestrator = new pairion::pipeline::AudioSessionOrchestrator(
                capture, encoder, wakeDetector, vad, wsClient, connState, &app);

            // Connect capture output to encoder, wake, and VAD (cross-thread via queued)
            QObject::connect(capture, &pairion::audio::PairionAudioCapture::audioFrameAvailable,
                             encoder, &pairion::audio::PairionOpusEncoder::encodePcmFrame);
            QObject::connect(capture, &pairion::audio::PairionAudioCapture::audioFrameAvailable,
                             wakeDetector, &pairion::wake::OpenWakewordDetector::processPcmFrame);
            QObject::connect(capture, &pairion::audio::PairionAudioCapture::audioFrameAvailable,
                             vad, &pairion::vad::SileroVad::processPcmFrame);

            audioThread->start();
            QMetaObject::invokeMethod(capture, "start", Qt::QueuedConnection);
            orchestrator->startListening();
        });

    QObject::connect(modelDownloader, &pairion::core::ModelDownloader::downloadError, &app,
                     [](const QString &msg) { qWarning() << "Model download error:" << msg; });

    // Connect to server, then check models
    QObject::connect(wsClient, &pairion::ws::PairionWebSocketClient::sessionOpened, modelDownloader,
                     [modelDownloader]() { modelDownloader->checkAndDownload(); });

    wsClient->connectToServer();

    int result = app.exec();

    audioThread->quit();
    audioThread->wait();
    delete capture;
    delete encoder;

    return result;
}
