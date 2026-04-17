/**
 * Root application component for the Pairion Client.
 *
 * Renders the M0 connection-status window: a status indicator showing
 * the current WebSocket connection state and a footer with server URL
 * and latency information. Subscribes to the Zustand connection store
 * for reactive updates.
 *
 * @module App
 */

import { useConnectionStore } from "./state/connectionStore";
import { useSettingsStore } from "./state/settingsStore";
import { ScreenCaptureIndicator } from "./hud/ScreenCaptureIndicator";

/** Status color mapping for the connection state indicator. */
function statusColor(status: string): string {
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
function statusLabel(status: string, attempt: number): string {
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

/**
 * Main application component.
 *
 * Displays the Pairion connection status with the on-brand amber/cyan
 * palette on a dark background, even at M0's minimal scope.
 */
function App() {
  const { status, latencyMs, reconnectAttempt, serverUrl } = useConnectionStore();
  const { screenCaptureActive } = useSettingsStore();

  return (
    <div className="flex flex-col h-screen bg-pairion-dark select-none" data-testid="app-root">
      {screenCaptureActive && <ScreenCaptureIndicator />}

      <main className="flex-1 flex flex-col items-center justify-center px-6">
        <div
          className={`w-4 h-4 rounded-full mb-4 transition-opacity duration-500 ${statusColor(status)}`}
          data-testid="status-dot"
        />
        <h1
          className="text-2xl font-mono font-semibold tracking-wide text-white transition-opacity duration-500"
          data-testid="status-label"
        >
          {statusLabel(status, reconnectAttempt)}
        </h1>
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
