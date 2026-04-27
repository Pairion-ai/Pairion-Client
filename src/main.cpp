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
#include "audio/pairion_audio_playback.h"
#include "audio/pairion_opus_encoder.h"
#include "core/constants.h"
#include "core/device_identity.h"
#include "core/model_downloader.h"
#include "core/onnx_session.h"
#include "memory/memory_browser_model.h"
#include "memory/memory_client.h"
#include "pipeline/audio_session_orchestrator.h"
#include "pipeline/pipeline_health_monitor.h"
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
#include <QTimer>

/**
 * @brief Initialize the audio pipeline after mic permission is granted and models are ready.
 *
 * Creates ONNX sessions, wake detector, VAD, orchestrator, and wires signal connections.
 * Called from the allModelsReady callback on the main thread. If ONNX model loading fails,
 * the load is retried up to retryCount times with a 1-second delay between attempts.
 * On exhaustion of retries, pipelineHealth is set to "models_failed".
 *
 * @param retryCount Number of remaining retry attempts. Defaults to 3 on the initial call.
 */
static void initAudioPipeline(QGuiApplication *app, pairion::audio::PairionAudioCapture *capture,
                              pairion::audio::PairionOpusEncoder *encoder,
                              pairion::ws::PairionWebSocketClient *wsClient,
                              pairion::state::ConnectionState *connState,
                              pairion::settings::Settings *settings, QThread *encoderThread,
                              QThread *inferenceThread, int retryCount = 3) {
    QString modelDir = pairion::core::ModelDownloader::modelCacheDir();
    pairion::core::OnnxInferenceSession *melSession = nullptr;
    pairion::core::OnnxInferenceSession *embSession = nullptr;
    pairion::core::OnnxInferenceSession *clsSession = nullptr;
    pairion::core::OnnxInferenceSession *vadSession = nullptr;

    // Fix D: retry ONNX model load up to retryCount times with 1-second delay.
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
        qCritical() << "Failed to load ONNX models (attempt" << (4 - retryCount) << "):"
                    << e.what();
        connState->appendLog(
            QStringLiteral("[ERROR] Failed to load ONNX models: %1").arg(QString::fromUtf8(e.what())));
        if (retryCount > 0) {
            connState->appendLog(
                QStringLiteral("[INFO] Retrying ONNX model load in 1 second (%1 attempts left)...")
                    .arg(retryCount));
            QTimer::singleShot(
                1000, app,
                [app, capture, encoder, wsClient, connState, settings, encoderThread,
                 inferenceThread, retryCount]() {
                    initAudioPipeline(app, capture, encoder, wsClient, connState, settings,
                                      encoderThread, inferenceThread, retryCount - 1);
                });
        } else {
            qCritical() << "ONNX model load failed after all retries — pipeline offline";
            connState->appendLog(
                QStringLiteral("[CRITICAL] ONNX model load failed after all retries. "
                               "Delete the model cache and restart."));
            connState->setPipelineHealth(QStringLiteral("models_failed"));
        }
        return;
    }

    // Wake detector and VAD run on the inference thread to avoid blocking
    // the main thread with ONNX model inference (~ms per frame at 50 fps).
    double wakeThreshold = settings->wakeThreshold();
    auto *wakeDetector =
        new pairion::wake::OpenWakewordDetector(melSession, embSession, clsSession, wakeThreshold);
    auto *vad = new pairion::vad::SileroVad(vadSession, settings->vadThreshold(),
                                            settings->vadSilenceEndMs());
    auto *playback = new pairion::audio::PairionAudioPlayback(app);
    wakeDetector->moveToThread(inferenceThread);
    vad->moveToThread(inferenceThread);

    auto *orchestrator = new pairion::pipeline::AudioSessionOrchestrator(
        capture, encoder, wakeDetector, vad, wsClient, connState, playback, app);

    // Connect capture → encoder (cross-thread to encoder thread, queued)
    QObject::connect(capture, &pairion::audio::PairionAudioCapture::audioFrameAvailable, encoder,
                     &pairion::audio::PairionOpusEncoder::encodePcmFrame);
    // Connect capture → wake detector (cross-thread to inference thread, queued)
    QObject::connect(capture, &pairion::audio::PairionAudioCapture::audioFrameAvailable,
                     wakeDetector, &pairion::wake::OpenWakewordDetector::processPcmFrame);
    // Connect capture → VAD (cross-thread to inference thread, queued)
    QObject::connect(capture, &pairion::audio::PairionAudioCapture::audioFrameAvailable, vad,
                     &pairion::vad::SileroVad::processPcmFrame);

    // Fix G: start inference thread and verify it is running before proceeding.
    inferenceThread->start();
    if (!inferenceThread->isRunning()) {
        qCritical() << "InferenceThread failed to start — wake word and VAD offline";
        connState->appendLog(
            QStringLiteral("[CRITICAL] Inference thread failed to start — microphone pipeline "
                           "offline. Restart the application."));
        connState->setPipelineHealth(QStringLiteral("pipeline_error"));
        return;
    }

    // Fix G: start encoder thread and verify it is running.
    encoderThread->start();
    if (!encoderThread->isRunning()) {
        qCritical() << "EncoderThread failed to start — audio encoding offline";
        connState->appendLog(
            QStringLiteral("[CRITICAL] Encoder thread failed to start — microphone pipeline "
                           "offline. Restart the application."));
        connState->setPipelineHealth(QStringLiteral("pipeline_error"));
        return;
    }

    // Fix 5: Detect worker thread crashes — log critical and surface to the UI.
    QObject::connect(inferenceThread, &QThread::finished, app, [connState]() {
        qCritical() << "InferenceThread exited unexpectedly — wake word and VAD offline";
        connState->appendLog(
            QStringLiteral("[CRITICAL] Inference thread exited — microphone pipeline offline. "
                           "Restart the application."));
    });
    QObject::connect(encoderThread, &QThread::finished, app, [connState]() {
        qCritical() << "EncoderThread exited unexpectedly — audio encoding offline";
        connState->appendLog(
            QStringLiteral("[CRITICAL] Encoder thread exited — microphone pipeline offline. "
                           "Restart the application."));
    });

    // Fix 3: Surface pipeline errors and capture errors to the UI log.
    QObject::connect(orchestrator, &pairion::pipeline::AudioSessionOrchestrator::pipelineError, app,
                     [connState](const QString &reason) {
                         connState->appendLog(
                             QStringLiteral("[ERROR] Pipeline: %1").arg(reason));
                     });
    QObject::connect(capture, &pairion::audio::PairionAudioCapture::captureError, app,
                     [connState](const QString &reason) {
                         connState->appendLog(
                             QStringLiteral("[ERROR] Microphone: %1").arg(reason));
                     });

    // Fix H: InferenceThread is verified running (Fix G). Queue warmup safely.
    QMetaObject::invokeMethod(wakeDetector, "warmup", Qt::QueuedConnection);

    capture->start();
    orchestrator->startListening();

    // Pipeline is fully operational — update health and start the health monitor.
    connState->setPipelineHealth(QStringLiteral("ready"));

    auto *healthMonitor = new pairion::pipeline::PipelineHealthMonitor(
        wsClient, capture, encoderThread, inferenceThread, orchestrator, connState, app);
    healthMonitor->start();

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

    // Apply device/rate settings now and keep them in sync with Settings changes.
    capture->configure(settings->audioInputDevice(), settings->audioSampleRate());
    QObject::connect(settings, &pairion::settings::Settings::audioInputDeviceChanged, &app,
                     [capture, settings]() {
                         capture->configure(settings->audioInputDevice(),
                                            settings->audioSampleRate());
                     });
    QObject::connect(settings, &pairion::settings::Settings::audioSampleRateChanged, &app,
                     [capture, settings]() {
                         capture->configure(settings->audioInputDevice(),
                                            settings->audioSampleRate());
                     });

    auto *encoderThread = new QThread(&app);
    encoderThread->setObjectName(QStringLiteral("EncoderThread"));
    encoder->moveToThread(encoderThread);

    auto *inferenceThread = new QThread(&app);
    inferenceThread->setObjectName(QStringLiteral("InferenceThread"));

    // Memory browser
    auto *memClient = new pairion::memory::MemoryClient(
        nam, QString::fromUtf8(pairion::kDefaultRestBaseUrl), &app);
    auto *memModel = new pairion::memory::MemoryBrowserModel(memClient, &app);

    // QML singletons
    qmlRegisterSingletonInstance("Pairion", 1, 0, "ConnectionState", connState);
    qmlRegisterSingletonInstance("Pairion", 1, 0, "Settings", settings);
    qmlRegisterSingletonInstance("Pairion", 1, 0, "MemoryBrowserModel", memModel);

    QQmlApplicationEngine engine;
    // Allow scenes to import the PairionScene QML module (qml/PairionScene/qmldir).
    engine.addImportPath(QStringLiteral("qrc:/qml"));
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
            connState->setPipelineHealth(QStringLiteral("mic_offline"));
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

    // Fix B: signal connected before connectToServer() to ensure no sessionOpened is missed.
    QObject::connect(wsClient, &pairion::ws::PairionWebSocketClient::sessionOpened, modelDownloader,
                     [modelDownloader, connState]() {
                         connState->setPipelineHealth(QStringLiteral("models_loading"));
                         modelDownloader->checkAndDownload();
                     });

    QObject::connect(modelDownloader, &pairion::core::ModelDownloader::allModelsReady, &app,
                     [&modelsReady, &tryStartPipeline, connState]() {
                         qInfo() << "All models ready";
                         modelsReady = true;
                         connState->setPipelineHealth(QStringLiteral("initializing"));
                         tryStartPipeline();
                     });

    QObject::connect(modelDownloader, &pairion::core::ModelDownloader::downloadError, &app,
                     [connState](const QString &msg) {
                         qWarning() << "Model download error:" << msg;
                         connState->appendLog(
                             QStringLiteral("[ERROR] Model download: %1").arg(msg));
                     });

    // WebSocket connects regardless of mic permission — debug panel shows Connected.
    // pipelineHealth starts at "connecting" (ConnectionState default).
    wsClient->connectToServer();

    int result = app.exec();

    inferenceThread->quit();
    inferenceThread->wait();
    encoderThread->quit();
    encoderThread->wait();
    delete encoder;

    return result;
}
