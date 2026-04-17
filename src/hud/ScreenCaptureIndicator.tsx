/**
 * Screen capture active indicator.
 *
 * Renders an unmistakable red "REC" dot whenever screen capture is active.
 * This is a hard invariant per Architecture §7 and §16 — screen capture
 * cannot happen without this indicator being visible.
 *
 * In M0, the indicator exists and is tested but the flag is never set
 * to true because no screen capture occurs.
 *
 * @module ScreenCaptureIndicator
 */

/**
 * Renders a red recording indicator when screen capture is active.
 */
export function ScreenCaptureIndicator() {
  return (
    <div
      className="absolute top-2 right-2 flex items-center gap-1.5 px-2 py-1 rounded bg-red-600/90 z-50"
      data-testid="screen-capture-indicator"
    >
      <div className="w-2 h-2 rounded-full bg-white animate-pulse" />
      <span className="text-xs font-mono font-bold text-white">REC</span>
    </div>
  );
}
