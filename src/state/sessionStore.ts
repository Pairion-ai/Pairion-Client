/**
 * Zustand store for session state (scaffolded for M0).
 *
 * Tracks the current conversation session, identified user, and turn state.
 * No active state management in M0 — this store exists as a typed scaffold
 * for M1 and beyond.
 *
 * @module sessionStore
 */

import { create } from "zustand";

/** Session state shape. */
export interface SessionState {
  /** Current session ID, if any. */
  sessionId: string | null;
  /** The identified user ID for this session. */
  userId: string | null;
  /** Whether a session is currently active. */
  active: boolean;
}

/** Actions available on the session store. */
export interface SessionActions {
  /** Open a new session. */
  openSession: (sessionId: string, userId: string) => void;
  /** Close the current session. */
  closeSession: () => void;
}

/** Combined store type. */
export type SessionStore = SessionState & SessionActions;

/** Scaffolded session store — no active usage in M0. */
export const useSessionStore = create<SessionStore>((set) => ({
  sessionId: null,
  userId: null,
  active: false,

  openSession: (sessionId: string, userId: string) =>
    set({
      sessionId,
      userId,
      active: true,
    }),

  closeSession: () =>
    set({
      sessionId: null,
      userId: null,
      active: false,
    }),
}));
