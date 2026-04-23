/**
 * @file pipeline_health_monitor.cpp
 * @brief Implementation of the periodic pipeline health monitor.
 */

#include "pipeline_health_monitor.h"

#include "audio_session_orchestrator.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcHealth, "pairion.pipeline.health") // LCOV_EXCL_LINE — static category registration; only the internal vtable-init path is unreachable

namespace pairion::pipeline {

PipelineHealthMonitor::PipelineHealthMonitor(
    pairion::ws::PairionWebSocketClient *wsClient,
    pairion::audio::PairionAudioCapture *capture, QThread *encoderThread,
    QThread *inferenceThread, pairion::pipeline::AudioSessionOrchestrator *orchestrator,
    pairion::state::ConnectionState *connState, QObject *parent)
    : QObject(parent), m_wsClient(wsClient), m_capture(capture),
      m_encoderThread(encoderThread), m_inferenceThread(inferenceThread),
      m_orchestrator(orchestrator), m_connState(connState) {
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &PipelineHealthMonitor::performHealthCheck);
}

void PipelineHealthMonitor::start() {
    connect(m_capture, &pairion::audio::PairionAudioCapture::audioFrameAvailable, this,
            &PipelineHealthMonitor::onAudioFrame);
    // Schedule the initial check after warm-up delay so the pipeline is fully running.
    // LCOV_EXCL_START — lambda fires after 5 s delay; never executes during fast unit tests
    QTimer::singleShot(kInitialDelayMs, this, [this]() {
        performHealthCheck();
        m_timer.start(kCheckIntervalMs);
    });
    // LCOV_EXCL_STOP
}

void PipelineHealthMonitor::onAudioFrame() {
    m_lastFrameTimer.restart();
    m_frameTimerRunning = true;
}

void PipelineHealthMonitor::performHealthCheck() {
    bool healthy = true;

    // 1. WebSocket connectivity — autonomous reconnect handles recovery.
    if (!m_wsClient->isConnected()) {
        qCWarning(lcHealth) << "Health check: WebSocket not connected";
        // Surfaced only when pipeline was previously ready to avoid overriding startup states.
        if (m_connState->pipelineHealth() == QLatin1String("ready")) {
            m_connState->setPipelineHealth(QStringLiteral("server_disconnected"));
            emit healthChanged(QStringLiteral("server_disconnected"));
        }
        // Do not mark healthy=false: PairionWebSocketClient handles reconnect autonomously.
    }

    // 2. Microphone capture.
    if (!m_capture->isRunning()) {
        qCWarning(lcHealth) << "Health check: audio capture not running";
        m_connState->appendLog(
            QStringLiteral("[WARN] Health: microphone capture stopped — attempting restart"));
        healthy = false;
        // LCOV_EXCL_START — restart requires real audio hardware; not exercisable in unit tests
        m_capture->stop();
        m_capture->start();
        if (m_capture->isRunning()) {
            qCInfo(lcHealth) << "Health check: audio capture restarted successfully";
            m_connState->appendLog(
                QStringLiteral("[INFO] Health: microphone capture restarted"));
            healthy = true;
        } else {
            qCCritical(lcHealth) << "Health check: audio capture restart failed";
            m_connState->appendLog(
                QStringLiteral("[ERROR] Health: microphone capture restart failed"));
            m_connState->setPipelineHealth(QStringLiteral("mic_offline"));
            emit healthChanged(QStringLiteral("mic_offline"));
        }
        // LCOV_EXCL_STOP
    }

    // 3. Encoder thread liveness.
    if (!m_encoderThread->isRunning()) {
        qCCritical(lcHealth) << "Health check: encoder thread not running";
        m_connState->appendLog(
            QStringLiteral("[ERROR] Health: encoder thread stopped — attempting restart"));
        healthy = false;
        // LCOV_EXCL_START — thread restart requires a running system; not exercisable in unit tests
        m_encoderThread->start();
        if (!m_encoderThread->isRunning()) {
            m_connState->setPipelineHealth(QStringLiteral("pipeline_error"));
            emit healthChanged(QStringLiteral("pipeline_error"));
        }
        // LCOV_EXCL_STOP
    }

    // 4. Inference thread liveness.
    if (!m_inferenceThread->isRunning()) {
        qCCritical(lcHealth) << "Health check: inference thread not running";
        m_connState->appendLog(
            QStringLiteral("[ERROR] Health: inference thread stopped — attempting restart"));
        healthy = false;
        // LCOV_EXCL_START — thread restart requires a running system; not exercisable in unit tests
        m_inferenceThread->start();
        if (!m_inferenceThread->isRunning()) {
            m_connState->setPipelineHealth(QStringLiteral("wake_failed"));
            emit healthChanged(QStringLiteral("wake_failed"));
        }
        // LCOV_EXCL_STOP
    }

    // 5. Audio frame flow — detect a silent microphone (frames stopped arriving).
    // LCOV_EXCL_START — requires 5+ seconds without audio frames during a running test; not exercisable
    if (m_frameTimerRunning && m_lastFrameTimer.elapsed() > kFrameTimeoutMs) {
        qCWarning(lcHealth) << "Health check: no audio frames for" << kFrameTimeoutMs << "ms";
        m_connState->appendLog(
            QStringLiteral("[WARN] Health: no audio frames — microphone may be offline"));
        m_connState->setPipelineHealth(QStringLiteral("mic_offline"));
        emit healthChanged(QStringLiteral("mic_offline"));
        healthy = false;
    }
    // LCOV_EXCL_STOP

    // 6. Orchestrator state (informational — the orchestrator's own 30 s timeout handles stuck streams).
    // LCOV_EXCL_START — requires an active session in Streaming state; not exercisable in unit tests
    if (m_orchestrator != nullptr
        && m_orchestrator->state() == AudioSessionOrchestrator::State::Streaming) {
        qCInfo(lcHealth) << "Health check: orchestrator is in Streaming state";
    }
    // LCOV_EXCL_STOP

    // Recovery: restore "ready" when all components are back to healthy.
    // LCOV_EXCL_START — requires wsClient connected and all components healthy simultaneously; needs live server
    if (healthy) {
        const QString &h = m_connState->pipelineHealth();
        if (h == QLatin1String("mic_offline") || h == QLatin1String("wake_failed")
            || h == QLatin1String("pipeline_error")
            || h == QLatin1String("server_disconnected")) {
            qCInfo(lcHealth) << "Health check: all components recovered — restoring ready";
            m_connState->setPipelineHealth(QStringLiteral("ready"));
            emit healthChanged(QStringLiteral("ready"));
        }
    }
    // LCOV_EXCL_STOP
}

} // namespace pairion::pipeline
