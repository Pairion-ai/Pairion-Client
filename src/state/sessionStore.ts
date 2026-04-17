/**
 * Zustand store for session state.
 *
 * Tracks the current conversation session, agent state, and transcript.
 * Active in M1 — drives the voice-state indicator and transcript strip.
 *
 * @module sessionStore
 */

import { create } from "zustand";

/** Agent state values matching the Server's AgentStateChange message. */
export type AgentState = "idle" | "listening" | "thinking" | "speaking" | "identifying" | "handoff";

/** Session state shape. */
export interface SessionState {
  /** Current session ID, if any. */
  sessionId: string | null;
  /** The identified user ID for this session. */
  userId: string | null;
  /** Whether a session is currently active. */
  active: boolean;
  /** Current agent state driving the HUD indicator. */
  agentState: AgentState;
  /** Latest partial transcript from STT (streaming). */
  partialTranscript: string;
  /** Latest finalized transcript from STT. */
  finalTranscript: string;
}

/** Actions available on the session store. */
export interface SessionActions {
  /** Open a new session. */
  openSession: (sessionId: string, userId: string) => void;
  /** Close the current session and reset state. */
  closeSession: () => void;
  /** Update the agent state. */
  setAgentState: (state: AgentState) => void;
  /** Update the partial transcript (streaming STT). */
  setPartialTranscript: (text: string) => void;
  /** Set the final transcript and clear partial. */
  setFinalTranscript: (text: string) => void;
  /** Clear all transcript text. */
  clearTranscripts: () => void;
}

/** Combined store type. */
export type SessionStore = SessionState & SessionActions;

/**
 * Zustand store for managing voice session state.
 *
 * Used by App.tsx to render the voice-state indicator and transcript strip.
 * Updated by Tauri event handlers when the Server sends session lifecycle
 * and agent state messages.
 */
export const useSessionStore = create<SessionStore>((set) => ({
  sessionId: null,
  userId: null,
  active: false,
  agentState: "idle",
  partialTranscript: "",
  finalTranscript: "",

  openSession: (sessionId: string, userId: string) =>
    set({
      sessionId,
      userId,
      active: true,
      agentState: "listening",
      partialTranscript: "",
      finalTranscript: "",
    }),

  closeSession: () =>
    set({
      sessionId: null,
      userId: null,
      active: false,
      agentState: "idle",
      partialTranscript: "",
      finalTranscript: "",
    }),

  setAgentState: (agentState: AgentState) => set({ agentState }),

  setPartialTranscript: (partialTranscript: string) => set({ partialTranscript }),

  setFinalTranscript: (text: string) =>
    set({
      finalTranscript: text,
      partialTranscript: "",
    }),

  clearTranscripts: () =>
    set({
      partialTranscript: "",
      finalTranscript: "",
    }),
}));
