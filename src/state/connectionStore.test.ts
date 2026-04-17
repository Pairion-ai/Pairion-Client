import { describe, it, expect, beforeEach } from "vitest";
import { useConnectionStore } from "./connectionStore";

describe("connectionStore", () => {
  beforeEach(() => {
    useConnectionStore.setState({
      status: "connecting",
      latencyMs: null,
      lastError: null,
      reconnectAttempt: 0,
      serverUrl: "ws://localhost:18789/ws/v1",
    });
  });

  it("has correct initial state", () => {
    const state = useConnectionStore.getState();
    expect(state.status).toBe("connecting");
    expect(state.latencyMs).toBeNull();
    expect(state.lastError).toBeNull();
    expect(state.reconnectAttempt).toBe(0);
    expect(state.serverUrl).toBe("ws://localhost:18789/ws/v1");
  });

  it("transitions to connected", () => {
    useConnectionStore.getState().setConnected(15);
    const state = useConnectionStore.getState();
    expect(state.status).toBe("connected");
    expect(state.latencyMs).toBe(15);
    expect(state.lastError).toBeNull();
    expect(state.reconnectAttempt).toBe(0);
  });

  it("transitions to connected without latency", () => {
    useConnectionStore.getState().setConnected();
    const state = useConnectionStore.getState();
    expect(state.status).toBe("connected");
    expect(state.latencyMs).toBeNull();
  });

  it("transitions to disconnected with reason", () => {
    useConnectionStore.getState().setDisconnected("server stopped");
    const state = useConnectionStore.getState();
    expect(state.status).toBe("disconnected");
    expect(state.lastError).toBe("server stopped");
    expect(state.latencyMs).toBeNull();
  });

  it("transitions to disconnected without reason", () => {
    useConnectionStore.getState().setDisconnected();
    const state = useConnectionStore.getState();
    expect(state.status).toBe("disconnected");
    expect(state.lastError).toBeNull();
  });

  it("transitions to reconnecting", () => {
    useConnectionStore.getState().setReconnecting(3);
    const state = useConnectionStore.getState();
    expect(state.status).toBe("reconnecting");
    expect(state.reconnectAttempt).toBe(3);
  });

  it("updates latency", () => {
    useConnectionStore.getState().setConnected(10);
    useConnectionStore.getState().updateLatency(25);
    expect(useConnectionStore.getState().latencyMs).toBe(25);
  });

  it("sets server URL", () => {
    useConnectionStore.getState().setServerUrl("ws://remote:18789/ws/v1");
    expect(useConnectionStore.getState().serverUrl).toBe("ws://remote:18789/ws/v1");
  });

  it("resets on setConnecting", () => {
    useConnectionStore.getState().setReconnecting(5);
    useConnectionStore.getState().setConnecting();
    const state = useConnectionStore.getState();
    expect(state.status).toBe("connecting");
    expect(state.reconnectAttempt).toBe(0);
    expect(state.lastError).toBeNull();
  });
});
