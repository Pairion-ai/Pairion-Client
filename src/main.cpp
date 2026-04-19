/**
 * @file main.cpp
 * @brief Entry point for the Pairion Client application.
 *
 * Creates the Qt application, requests microphone permission via QPermission API,
 * initializes device identity, settings, logging, model downloader, audio pipeline,
 * and QML UI. Wires the full M1 upstream pipeline: capture → wake → VAD →
 * encode → WebSocket.
 *
 * Threading model:
 *   - Main thread: QML, PairionAudioCapture (QAudioSource requires main thread on macOS),
 *     ConnectionState, WebSocket client, orchestrator
 *   - Encoder thread: PairionOpusEncoder (CPU-bound Opus encoding)
 *   - Wake/VAD process on main thread via signal delivery (mock-heavy at M1;
 *     move to worker thread at M2 if profiling shows main-thread pressure)
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
#include <QPermissions>
#include <QQmlApplicationEngine>
#include <QThread>

/**
 * @brief Initialize the audio pipeline after mic permission is granted and models are ready.
 *
 * Creates ONNX sessions, wake detector, VAD, orchestrator, and wires signal connections.
 * Called from the allModelsReady callback on the main thread.
 */
static void initAudioPipeline(QGuiApplication *app, pairion::audio::PairionAudioCapture *capture,
                              pairion::audio::PairionOpusEncoder *encoder,
                              pairion::ws::PairionWebSocketClient *wsClient,
                              pairion::state::ConnectionState *connState,
                              pairion::settings::Settings *settings, QThread *encoderThread,
                              QThread *inferenceThread) {
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
        connState->appendLog(QStringLiteral("[ERROR] Failed to load ONNX models: %1")
                                 .arg(QString::fromUtf8(e.what())));
        return;
    }

    // Wake detector and VAD run on the inference thread to avoid blocking
    // the main thread with ONNX model inference (~ms per frame at 50 fps).
    // Threshold lowered for M1 validation — openWakeWord produces lower scores
    // on USB condenser microphones with low native gain. Production tuning will
    // add gain normalization or use a mic-appropriate threshold.
    double wakeThreshold = std::min(settings->wakeThreshold(), 0.03);
    auto *wakeDetector =
        new pairion::wake::OpenWakewordDetector(melSession, embSession, clsSession, wakeThreshold);
    auto *vad = new pairion::vad::SileroVad(vadSession, settings->vadThreshold(),
                                            settings->vadSilenceEndMs());
    wakeDetector->moveToThread(inferenceThread);
    vad->moveToThread(inferenceThread);

    auto *orchestrator = new pairion::pipeline::AudioSessionOrchestrator(
        capture, encoder, wakeDetector, vad, wsClient, connState, app);

    // Connect capture → encoder (cross-thread to encoder thread, queued)
    QObject::connect(capture, &pairion::audio::PairionAudioCapture::audioFrameAvailable, encoder,
                     &pairion::audio::PairionOpusEncoder::encodePcmFrame);
    // Connect capture → wake detector (cross-thread to inference thread, queued)
    QObject::connect(capture, &pairion::audio::PairionAudioCapture::audioFrameAvailable,
                     wakeDetector, &pairion::wake::OpenWakewordDetector::processPcmFrame);
    // Connect capture → VAD (cross-thread to inference thread, queued)
    QObject::connect(capture, &pairion::audio::PairionAudioCapture::audioFrameAvailable, vad,
                     &pairion::vad::SileroVad::processPcmFrame);

    inferenceThread->start();
    encoderThread->start();

    // Warm up the wake detector on the inference thread (not main thread)
    QMetaObject::invokeMethod(wakeDetector, "warmup", Qt::QueuedConnection);

    capture->start();
    orchestrator->startListening();

    qInfo() << "Audio pipeline started";
}

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

    // Audio capture lives on main thread (macOS QAudioSource requires it).
    // Encoder lives on a worker thread (CPU-bound Opus encoding).
    auto *capture = new pairion::audio::PairionAudioCapture(&app);
    auto *encoder = new pairion::audio::PairionOpusEncoder();

    auto *encoderThread = new QThread(&app);
    encoderThread->setObjectName(QStringLiteral("EncoderThread"));
    encoder->moveToThread(encoderThread);

    auto *inferenceThread = new QThread(&app);
    inferenceThread->setObjectName(QStringLiteral("InferenceThread"));

    // QML singletons
    qmlRegisterSingletonInstance("Pairion", 1, 0, "ConnectionState", connState);
    qmlRegisterSingletonInstance("Pairion", 1, 0, "Settings", settings);

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    // Model downloader — triggers pipeline start when ready
    auto *modelDownloader = new pairion::core::ModelDownloader(nam, &app);
    bool micPermissionGranted = false;
    bool modelsReady = false;

    // Lambda to start pipeline when BOTH mic permission and models are ready
    auto tryStartPipeline = [&app, capture, encoder, wsClient, connState, settings, encoderThread,
                             inferenceThread, &micPermissionGranted, &modelsReady]() {
        qInfo() << "tryStartPipeline: mic=" << micPermissionGranted << "models=" << modelsReady;
        if (micPermissionGranted && modelsReady) {
            initAudioPipeline(&app, capture, encoder, wsClient, connState, settings, encoderThread,
                              inferenceThread);
        }
    };

    // Request microphone permission via QPermission API (must happen on main thread)
    QMicrophonePermission micPermission;
    auto permissionStatus = app.checkPermission(micPermission);

    auto handlePermission = [connState, &micPermissionGranted,
                             &tryStartPipeline](Qt::PermissionStatus status) {
        if (status == Qt::PermissionStatus::Granted) {
            qInfo() << "Microphone permission granted";
            connState->appendLog(QStringLiteral("[INFO] Microphone permission granted"));
            micPermissionGranted = true;
            tryStartPipeline();
        } else {
            qWarning() << "Microphone permission denied";
            connState->appendLog(
                QStringLiteral("[WARN] Microphone access denied. Grant access in System Settings > "
                               "Privacy & Security > Microphone."));
        }
    };

    switch (permissionStatus) {
    case Qt::PermissionStatus::Undetermined:
        qInfo() << "Requesting microphone permission...";
        app.requestPermission(micPermission, [handlePermission](const QPermission &perm) {
            handlePermission(perm.status());
        });
        break;
    case Qt::PermissionStatus::Granted:
        handlePermission(Qt::PermissionStatus::Granted);
        break;
    case Qt::PermissionStatus::Denied:
        handlePermission(Qt::PermissionStatus::Denied);
        break;
    }

    QObject::connect(modelDownloader, &pairion::core::ModelDownloader::allModelsReady, &app,
                     [&modelsReady, &tryStartPipeline]() {
                         qInfo() << "All models ready";
                         modelsReady = true;
                         tryStartPipeline();
                     });

    QObject::connect(modelDownloader, &pairion::core::ModelDownloader::downloadError, &app,
                     [connState](const QString &msg) {
                         qWarning() << "Model download error:" << msg;
                         connState->appendLog(
                             QStringLiteral("[ERROR] Model download: %1").arg(msg));
                     });

    // WebSocket connects regardless of mic permission — debug panel shows Connected
    QObject::connect(wsClient, &pairion::ws::PairionWebSocketClient::sessionOpened, modelDownloader,
                     [modelDownloader]() { modelDownloader->checkAndDownload(); });

    wsClient->connectToServer();

    int result = app.exec();

    inferenceThread->quit();
    inferenceThread->wait();
    encoderThread->quit();
    encoderThread->wait();
    delete encoder;

    return result;
}
