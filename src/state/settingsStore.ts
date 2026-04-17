/**
 * Zustand store for user settings and window preferences.
 *
 * Manages HUD position, visibility preferences, and other user-configurable
 * options. In M0, only basic window preferences are active.
 *
 * @module settingsStore
 */

import { create } from "zustand";

/** Settings state shape. */
export interface SettingsState {
  /** Whether the transcript strip is visible. */
  transcriptVisible: boolean;
  /** Whether the window is always on top. */
  alwaysOnTop: boolean;
  /** Whether screen capture is currently active (invariant indicator). */
  screenCaptureActive: boolean;
}

/** Actions available on the settings store. */
export interface SettingsActions {
  /** Toggle transcript visibility. */
  setTranscriptVisible: (visible: boolean) => void;
  /** Toggle always-on-top behavior. */
  setAlwaysOnTop: (enabled: boolean) => void;
  /** Set the screen capture active flag (drives the visible indicator). */
  setScreenCaptureActive: (active: boolean) => void;
}

/** Combined store type. */
export type SettingsStore = SettingsState & SettingsActions;

/**
 * Zustand store for managing user settings.
 *
 * The `screenCaptureActive` flag drives the mandatory visible indicator
 * per Architecture §7 and §16. In M0, this flag is never set to true
 * because no screen capture happens.
 */
export const useSettingsStore = create<SettingsStore>((set) => ({
  transcriptVisible: true,
  alwaysOnTop: false,
  screenCaptureActive: false,

  setTranscriptVisible: (visible: boolean) =>
    set({
      transcriptVisible: visible,
    }),

  setAlwaysOnTop: (enabled: boolean) =>
    set({
      alwaysOnTop: enabled,
    }),

  setScreenCaptureActive: (active: boolean) =>
    set({
      screenCaptureActive: active,
    }),
}));
