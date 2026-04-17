import { describe, it, expect, beforeEach } from "vitest";
import { useSessionStore } from "./sessionStore";

describe("sessionStore", () => {
  beforeEach(() => {
    useSessionStore.setState({
      sessionId: null,
      userId: null,
      active: false,
      agentState: "idle",
      partialTranscript: "",
      finalTranscript: "",
    });
  });

  it("has correct initial state", () => {
    const state = useSessionStore.getState();
    expect(state.sessionId).toBeNull();
    expect(state.userId).toBeNull();
    expect(state.active).toBe(false);
    expect(state.agentState).toBe("idle");
    expect(state.partialTranscript).toBe("");
    expect(state.finalTranscript).toBe("");
  });

  it("opens a session", () => {
    useSessionStore.getState().openSession("session-1", "user-1");
    const state = useSessionStore.getState();
    expect(state.sessionId).toBe("session-1");
    expect(state.userId).toBe("user-1");
    expect(state.active).toBe(true);
    expect(state.agentState).toBe("listening");
  });

  it("closes a session and resets all fields", () => {
    useSessionStore.getState().openSession("session-1", "user-1");
    useSessionStore.getState().setPartialTranscript("test");
    useSessionStore.getState().closeSession();
    const state = useSessionStore.getState();
    expect(state.sessionId).toBeNull();
    expect(state.userId).toBeNull();
    expect(state.active).toBe(false);
    expect(state.agentState).toBe("idle");
    expect(state.partialTranscript).toBe("");
    expect(state.finalTranscript).toBe("");
  });

  it("sets agent state", () => {
    useSessionStore.getState().setAgentState("thinking");
    expect(useSessionStore.getState().agentState).toBe("thinking");
  });

  it("sets partial transcript", () => {
    useSessionStore.getState().setPartialTranscript("what's the");
    expect(useSessionStore.getState().partialTranscript).toBe("what's the");
  });

  it("sets final transcript and clears partial", () => {
    useSessionStore.getState().setPartialTranscript("what's the wea");
    useSessionStore.getState().setFinalTranscript("what's the weather");
    const state = useSessionStore.getState();
    expect(state.finalTranscript).toBe("what's the weather");
    expect(state.partialTranscript).toBe("");
  });

  it("clears transcripts", () => {
    useSessionStore.getState().setPartialTranscript("partial");
    useSessionStore.getState().setFinalTranscript("final");
    useSessionStore.getState().clearTranscripts();
    const state = useSessionStore.getState();
    expect(state.partialTranscript).toBe("");
    expect(state.finalTranscript).toBe("");
  });

  it("cycles through agent states", () => {
    useSessionStore.getState().setAgentState("listening");
    expect(useSessionStore.getState().agentState).toBe("listening");
    useSessionStore.getState().setAgentState("thinking");
    expect(useSessionStore.getState().agentState).toBe("thinking");
    useSessionStore.getState().setAgentState("speaking");
    expect(useSessionStore.getState().agentState).toBe("speaking");
    useSessionStore.getState().setAgentState("idle");
    expect(useSessionStore.getState().agentState).toBe("idle");
  });
});
