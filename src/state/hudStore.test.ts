import { describe, it, expect, beforeEach } from "vitest";
import { useHudStore } from "./hudStore";

describe("hudStore", () => {
  beforeEach(() => {
    useHudStore.setState({
      agentState: "idle",
      waveformData: [],
      pulseRateHz: 1.0,
    });
  });

  it("has correct initial state", () => {
    const state = useHudStore.getState();
    expect(state.agentState).toBe("idle");
    expect(state.waveformData).toEqual([]);
    expect(state.pulseRateHz).toBe(1.0);
  });

  it("sets agent state", () => {
    useHudStore.getState().setAgentState("listening");
    expect(useHudStore.getState().agentState).toBe("listening");
  });

  it("sets waveform data", () => {
    useHudStore.getState().setWaveformData([0.1, 0.5, 0.8]);
    expect(useHudStore.getState().waveformData).toEqual([0.1, 0.5, 0.8]);
  });

  it("sets pulse rate", () => {
    useHudStore.getState().setPulseRateHz(2.5);
    expect(useHudStore.getState().pulseRateHz).toBe(2.5);
  });
});
