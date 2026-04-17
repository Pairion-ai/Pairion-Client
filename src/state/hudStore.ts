/**
 * Zustand store for HUD animation state (scaffolded for M0).
 *
 * Tracks the visual animation state, waveform data, and telemetry readouts
 * for the HUD. No active state management in M0 — this store exists as
 * a typed scaffold for M2 and beyond.
 *
 * @module hudStore
 */

import { create } from "zustand";

/** HUD agent state values. */
export type AgentState = "idle" | "listening" | "thinking" | "speaking";

/** HUD state shape. */
export interface HudState {
  /** Current agent state driving the HUD animation. */
  agentState: AgentState;
  /** Waveform amplitude data for reactive visualization. */
  waveformData: number[];
  /** Arc-reactor pulse rate in Hz. */
  pulseRateHz: number;
}

/** Actions available on the HUD store. */
export interface HudActions {
  /** Update the agent state. */
  setAgentState: (state: AgentState) => void;
  /** Update waveform data. */
  setWaveformData: (data: number[]) => void;
  /** Update pulse rate. */
  setPulseRateHz: (hz: number) => void;
}

/** Combined store type. */
export type HudStore = HudState & HudActions;

/** Scaffolded HUD store — no active usage in M0. */
export const useHudStore = create<HudStore>((set) => ({
  agentState: "idle",
  waveformData: [],
  pulseRateHz: 1.0,

  setAgentState: (agentState: AgentState) => set({ agentState }),

  setWaveformData: (waveformData: number[]) => set({ waveformData }),

  setPulseRateHz: (pulseRateHz: number) => set({ pulseRateHz }),
}));
