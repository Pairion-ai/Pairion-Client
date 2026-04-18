#pragma once

/**
 * @file constants.h
 * @brief Application-wide constants for the Pairion Client.
 *
 * Defines compile-time constants for server connectivity, timing intervals,
 * and application metadata used throughout the client codebase.
 */

#include <chrono>
#include <cstdint>
#include <QString>

namespace pairion {

/// Client version string reported in DeviceIdentify messages.
inline constexpr const char *kClientVersion = "0.1.0";

/// Default WebSocket server URL.
inline constexpr const char *kDefaultServerUrl = "ws://localhost:18789/ws/v1";

/// Default REST base URL for log forwarding.
inline constexpr const char *kDefaultRestBaseUrl = "http://localhost:18789/v1";

/// Heartbeat ping interval in milliseconds.
inline constexpr int kHeartbeatIntervalMs = 15000;

/// Heartbeat pong deadline in milliseconds.
inline constexpr int kHeartbeatDeadlineMs = 10000;

/// Log flush interval in milliseconds.
inline constexpr int kLogFlushIntervalMs = 30000;

/// Reconnect backoff intervals in milliseconds.
inline constexpr int kReconnectBackoffMs[] = {1000, 2000, 4000, 8000, 15000, 30000};

/// Number of reconnect backoff steps.
inline constexpr int kReconnectBackoffSteps = 6;

/// PCM frame size in bytes (20 ms at 16 kHz, mono, 16-bit = 320 samples * 2 bytes).
inline constexpr int kPcmFrameBytes = 640;

/// Pre-roll buffer size in bytes (~200 ms at 16 kHz, 16-bit mono).
inline constexpr int kPreRollBytes = 6400;

/// Streaming timeout in milliseconds (safety cap for runaway streams).
inline constexpr int kStreamingTimeoutMs = 30000;

/// False-wake suppression window in milliseconds.
inline constexpr int kWakeSuppresssionMs = 500;

} // namespace pairion
