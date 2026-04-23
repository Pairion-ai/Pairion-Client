#pragma once

/**
 * @file pipeline_health_monitor.h
 * @brief Monitors audio pipeline component health and triggers recovery actions.
 *
 * Runs an initial health check 5 seconds after start() to allow the pipeline to
 * warm up, then re-checks every 30 seconds. Verifies WebSocket connectivity,
 * microphone capture state, worker thread liveness, and audio frame flow. Updates
 * ConnectionState.pipelineHealth and logs all failures at WARNING/CRITICAL level.
 *
 * Threading: all slots run on the main thread event loop.
 */

#include "../audio/pairion_audio_capture.h"
#include "../state/connection_state.h"
#include "../ws/pairion_websocket_client.h"

#include <QElapsedTimer>
#include <QObject>
#include <QThread>
#include <QTimer>

// Forward declare to avoid circular include; AudioSessionOrchestrator.h pulls in many headers.
namespace pairion::pipeline {
class AudioSessionOrchestrator;
}

namespace pairion::pipeline {

/**
 * @brief Periodic health checker for the audio pipeline.
 *
 * Constructed after initAudioPipeline() succeeds and started immediately. Emits
 * healthChanged() whenever the derived health status string changes. All methods
 * run on the main thread.
 */
class PipelineHealthMonitor : public QObject {
    Q_OBJECT
  public:
    /**
     * @brief Construct the health monitor with all pipeline dependencies.
     * @param wsClient WebSocket client for connectivity status (not owned).
     * @param capture Audio capture for microphone liveness (not owned).
     * @param encoderThread Opus encoder worker thread (not owned).
     * @param inferenceThread Wake/VAD inference worker thread (not owned).
     * @param orchestrator Pipeline orchestrator for state inspection; may be nullptr (not owned).
     * @param connState ConnectionState singleton for health property updates (not owned).
     * @param parent QObject parent.
     */
    PipelineHealthMonitor(pairion::ws::PairionWebSocketClient *wsClient,
                          pairion::audio::PairionAudioCapture *capture, QThread *encoderThread,
                          QThread *inferenceThread,
                          pairion::pipeline::AudioSessionOrchestrator *orchestrator,
                          pairion::state::ConnectionState *connState,
                          QObject *parent = nullptr);

  public slots:
    /**
     * @brief Start the health monitor.
     *
     * Connects to capture's audioFrameAvailable signal to track audio flow.
     * Schedules the initial check 5 seconds after start(); subsequent checks
     * fire every 30 seconds.
     */
    void start();

    /**
     * @brief Execute a single health check pass across all pipeline components.
     *
     * Checks: WebSocket connectivity, microphone capture, encoder thread,
     * inference thread, audio frame flow, and orchestrator state. Exposed as a
     * public slot so tests can invoke it directly without waiting for the timer.
     */
    void performHealthCheck();

  signals:
    /// Emitted when the derived pipeline health status string changes.
    void healthChanged(const QString &health);

  private slots:
    void onAudioFrame();

  private:
    pairion::ws::PairionWebSocketClient *m_wsClient;
    pairion::audio::PairionAudioCapture *m_capture;
    QThread *m_encoderThread;
    QThread *m_inferenceThread;
    pairion::pipeline::AudioSessionOrchestrator *m_orchestrator;
    pairion::state::ConnectionState *m_connState;

    QTimer m_timer;
    QElapsedTimer m_lastFrameTimer;
    bool m_frameTimerRunning = false;

    static constexpr int kInitialDelayMs = 5000;  ///< Warm-up delay before first check.
    static constexpr int kCheckIntervalMs = 30000; ///< Periodic check interval.
    static constexpr int kFrameTimeoutMs = 5000;   ///< Max silence before mic is flagged.
};

} // namespace pairion::pipeline
