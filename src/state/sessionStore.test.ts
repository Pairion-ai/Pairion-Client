import { describe, it, expect, beforeEach } from "vitest";
import { useSessionStore } from "./sessionStore";

describe("sessionStore", () => {
  beforeEach(() => {
    useSessionStore.setState({
      sessionId: null,
      userId: null,
      active: false,
    });
  });

  it("has correct initial state", () => {
    const state = useSessionStore.getState();
    expect(state.sessionId).toBeNull();
    expect(state.userId).toBeNull();
    expect(state.active).toBe(false);
  });

  it("opens a session", () => {
    useSessionStore.getState().openSession("session-1", "user-1");
    const state = useSessionStore.getState();
    expect(state.sessionId).toBe("session-1");
    expect(state.userId).toBe("user-1");
    expect(state.active).toBe(true);
  });

  it("closes a session", () => {
    useSessionStore.getState().openSession("session-1", "user-1");
    useSessionStore.getState().closeSession();
    const state = useSessionStore.getState();
    expect(state.sessionId).toBeNull();
    expect(state.userId).toBeNull();
    expect(state.active).toBe(false);
  });
});
