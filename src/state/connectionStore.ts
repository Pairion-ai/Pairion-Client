/**
 * Zustand store for WebSocket connection state.
 *
 * Tracks the connection status between the Client and Pairion Server,
 * including latency measurements, reconnect attempts, and error information.
 *
 * @module connectionStore
 */

import { create } from "zustand";

/** Possible connection status values. */
export type ConnectionStatus = "connecting" | "connected" | "disconnected" | "reconnecting";

/** Connection state shape. */
export interface ConnectionState {
  /** Current connection status. */
  status: ConnectionStatus;
  /** Last measured heartbeat latency in milliseconds, if connected. */
  latencyMs: number | null;
  /** Last connection error message, if any. */
  lastError: string | null;
  /** Current reconnection attempt number (1-based), if reconnecting. */
  reconnectAttempt: number;
  /** The server URL. */
  serverUrl: string;
}

/** Actions available on the connection store. */
export interface ConnectionActions {
  /** Transition to the connecting state. */
  setConnecting: () => void;
  /** Transition to the connected state with optional latency. */
  setConnected: (latencyMs?: number) => void;
  /** Transition to the disconnected state with optional reason. */
  setDisconnected: (reason?: string) => void;
  /** Transition to the reconnecting state with the current attempt number. */
  setReconnecting: (attempt: number) => void;
  /** Update the latency measurement. */
  updateLatency: (latencyMs: number) => void;
  /** Set the server URL. */
  setServerUrl: (url: string) => void;
}

/** Combined store type. */
export type ConnectionStore = ConnectionState & ConnectionActions;

/** Default server URL. */
const DEFAULT_SERVER_URL = "ws://localhost:18789/ws/v1";

/**
 * Zustand store for managing WebSocket connection state.
 *
 * Used by App.tsx and other components to reactively render
 * connection status information.
 */
export const useConnectionStore = create<ConnectionStore>((set) => ({
  status: "connecting",
  latencyMs: null,
  lastError: null,
  reconnectAttempt: 0,
  serverUrl: DEFAULT_SERVER_URL,

  setConnecting: () =>
    set({
      status: "connecting",
      lastError: null,
      reconnectAttempt: 0,
    }),

  setConnected: (latencyMs?: number) =>
    set({
      status: "connected",
      latencyMs: latencyMs ?? null,
      lastError: null,
      reconnectAttempt: 0,
    }),

  setDisconnected: (reason?: string) =>
    set({
      status: "disconnected",
      lastError: reason ?? null,
      latencyMs: null,
    }),

  setReconnecting: (attempt: number) =>
    set({
      status: "reconnecting",
      reconnectAttempt: attempt,
    }),

  updateLatency: (latencyMs: number) =>
    set({
      latencyMs,
    }),

  setServerUrl: (url: string) =>
    set({
      serverUrl: url,
    }),
}));
