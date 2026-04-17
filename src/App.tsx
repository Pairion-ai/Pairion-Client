/**
 * Root application component for the Pairion Client.
 *
 * Renders the connection-status window with M1 voice-state indicator
 * and transcript strip. Subscribes to Zustand stores for reactive updates.
 *
 * @module App
 */

import { useConnectionStore } from "./state/connectionStore";
import { useSessionStore } from "./state/sessionStore";
import type { AgentState } from "./state/sessionStore";
import { useSettingsStore } from "./state/settingsStore";
import { ScreenCaptureIndicator } from "./hud/ScreenCaptureIndicator";

/** Status color mapping for the connection state indicator. */
function connectionStatusColor(status: string): string {
  switch (status) {
    case "connected":
      return "bg-pairion-cyan";
    case "connecting":
      return "bg-pairion-amber";
    case "reconnecting":
      return "bg-pairion-amber";
    case "disconnected":
      return "bg-red-500";
    default:
      return "bg-gray-500";
  }
}

/** Status label for display. */
function connectionStatusLabel(status: string, attempt: number): string {
  switch (status) {
    case "connected":
      return "Connected";
    case "connecting":
      return "Connecting\u2026";
    case "reconnecting":
      return `Reconnecting\u2026 (attempt ${String(attempt)})`;
    case "disconnected":
      return "Disconnected";
    default:
      return "Unknown";
  }
}

/** Color for the voice-state badge. */
function agentStateColor(state: AgentState): string {
  switch (state) {
    case "listening":
      return "bg-pairion-amber";
    case "thinking":
      return "bg-pairion-cyan";
    case "speaking":
      return "bg-green-500";
    case "identifying":
      return "bg-pairion-purple";
    case "handoff":
      return "bg-pairion-teal";
    default:
      return "bg-pairion-dark-border";
  }
}

/** Label for the voice-state badge. */
function agentStateLabel(state: AgentState): string {
  switch (state) {
    case "idle":
      return "Idle";
    case "listening":
      return "Listening";
    case "thinking":
      return "Thinking";
    case "speaking":
      return "Speaking";
    case "identifying":
      return "Identifying";
    case "handoff":
      return "Handoff";
    default:
      return "Idle";
  }
}

/**
 * Main application component.
 *
 * Displays connection status, voice-state indicator, and transcript strip.
 * Uses the Pairion amber/cyan palette on a dark background.
 */
function App() {
  const { status, latencyMs, reconnectAttempt, serverUrl } = useConnectionStore();
  const { agentState, partialTranscript, finalTranscript, active } = useSessionStore();
  const { screenCaptureActive } = useSettingsStore();

  const displayTranscript = partialTranscript || finalTranscript;

  return (
    <div className="flex flex-col h-screen bg-pairion-dark select-none" data-testid="app-root">
      {screenCaptureActive && <ScreenCaptureIndicator />}

      <main className="flex-1 flex flex-col items-center justify-center px-6">
        {/* Connection status dot */}
        <div
          className={`w-4 h-4 rounded-full mb-4 transition-opacity duration-500 ${connectionStatusColor(status)}`}
          data-testid="status-dot"
        />
        <h1
          className="text-2xl font-mono font-semibold tracking-wide text-white transition-opacity duration-500"
          data-testid="status-label"
        >
          {connectionStatusLabel(status, reconnectAttempt)}
        </h1>

        {/* Voice-state indicator (M1) */}
        {status === "connected" && (
          <div className="mt-6 flex flex-col items-center" data-testid="voice-section">
            <div
              className={`px-4 py-1.5 rounded-full text-sm font-mono font-medium text-white transition-all duration-300 ${agentStateColor(agentState)}`}
              data-testid="agent-state-badge"
            >
              {agentStateLabel(agentState)}
            </div>

            {/* Transcript strip */}
            {active && displayTranscript && (
              <p
                className="mt-3 text-sm font-sans text-pairion-dark-border max-w-xs text-center transition-opacity duration-300"
                data-testid="transcript-strip"
              >
                {displayTranscript}
              </p>
            )}
          </div>
        )}
      </main>

      <footer className="px-4 py-2 flex justify-between items-center text-xs font-mono text-pairion-dark-border border-t border-pairion-dark-border">
        <span data-testid="server-url">{serverUrl}</span>
        <span data-testid="latency">
          {status === "connected" && latencyMs !== null ? `${String(Math.round(latencyMs))}ms` : ""}
        </span>
      </footer>
    </div>
  );
}

export default App;
