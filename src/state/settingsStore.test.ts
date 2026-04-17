import { describe, it, expect, beforeEach } from "vitest";
import { useSettingsStore } from "./settingsStore";

describe("settingsStore", () => {
  beforeEach(() => {
    useSettingsStore.setState({
      transcriptVisible: true,
      alwaysOnTop: false,
      screenCaptureActive: false,
    });
  });

  it("has correct initial state", () => {
    const state = useSettingsStore.getState();
    expect(state.transcriptVisible).toBe(true);
    expect(state.alwaysOnTop).toBe(false);
    expect(state.screenCaptureActive).toBe(false);
  });

  it("toggles transcript visibility", () => {
    useSettingsStore.getState().setTranscriptVisible(false);
    expect(useSettingsStore.getState().transcriptVisible).toBe(false);
    useSettingsStore.getState().setTranscriptVisible(true);
    expect(useSettingsStore.getState().transcriptVisible).toBe(true);
  });

  it("toggles always on top", () => {
    useSettingsStore.getState().setAlwaysOnTop(true);
    expect(useSettingsStore.getState().alwaysOnTop).toBe(true);
  });

  it("sets screen capture active flag", () => {
    useSettingsStore.getState().setScreenCaptureActive(true);
    expect(useSettingsStore.getState().screenCaptureActive).toBe(true);
    useSettingsStore.getState().setScreenCaptureActive(false);
    expect(useSettingsStore.getState().screenCaptureActive).toBe(false);
  });
});
