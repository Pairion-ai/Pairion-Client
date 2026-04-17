import { render, screen } from "@testing-library/react";
import { describe, it, expect, beforeEach } from "vitest";
import App from "./App";
import { useConnectionStore } from "./state/connectionStore";
import { useSessionStore } from "./state/sessionStore";
import { useSettingsStore } from "./state/settingsStore";

describe("App", () => {
  beforeEach(() => {
    useConnectionStore.setState({
      status: "connecting",
      latencyMs: null,
      lastError: null,
      reconnectAttempt: 0,
      serverUrl: "ws://localhost:18789/ws/v1",
    });
    useSessionStore.setState({
      sessionId: null,
      userId: null,
      active: false,
      agentState: "idle",
      partialTranscript: "",
      finalTranscript: "",
    });
    useSettingsStore.setState({
      screenCaptureActive: false,
    });
  });

  it("renders the app root", () => {
    render(<App />);
    expect(screen.getByTestId("app-root")).toBeInTheDocument();
  });

  it("shows Connecting status initially", () => {
    render(<App />);
    expect(screen.getByTestId("status-label")).toHaveTextContent("Connecting\u2026");
  });

  it("shows Connected status", () => {
    useConnectionStore.setState({ status: "connected", latencyMs: 15 });
    render(<App />);
    expect(screen.getByTestId("status-label")).toHaveTextContent("Connected");
  });

  it("shows latency when connected", () => {
    useConnectionStore.setState({ status: "connected", latencyMs: 42.7 });
    render(<App />);
    expect(screen.getByTestId("latency")).toHaveTextContent("43ms");
  });

  it("shows no latency when disconnected", () => {
    useConnectionStore.setState({ status: "disconnected", latencyMs: null });
    render(<App />);
    expect(screen.getByTestId("latency")).toHaveTextContent("");
  });

  it("shows Disconnected status", () => {
    useConnectionStore.setState({ status: "disconnected" });
    render(<App />);
    expect(screen.getByTestId("status-label")).toHaveTextContent("Disconnected");
  });

  it("shows Reconnecting status with attempt number", () => {
    useConnectionStore.setState({ status: "reconnecting", reconnectAttempt: 3 });
    render(<App />);
    expect(screen.getByTestId("status-label")).toHaveTextContent(
      "Reconnecting\u2026 (attempt 3)",
    );
  });

  it("displays the server URL in the footer", () => {
    render(<App />);
    expect(screen.getByTestId("server-url")).toHaveTextContent("ws://localhost:18789/ws/v1");
  });

  it("does not show screen capture indicator when inactive", () => {
    render(<App />);
    expect(screen.queryByTestId("screen-capture-indicator")).not.toBeInTheDocument();
  });

  it("shows screen capture indicator when active", () => {
    useSettingsStore.setState({ screenCaptureActive: true });
    render(<App />);
    expect(screen.getByTestId("screen-capture-indicator")).toBeInTheDocument();
  });

  it("renders status dot with correct color for connected", () => {
    useConnectionStore.setState({ status: "connected" });
    render(<App />);
    const dot = screen.getByTestId("status-dot");
    expect(dot.className).toContain("bg-pairion-cyan");
  });

  it("renders status dot with correct color for disconnected", () => {
    useConnectionStore.setState({ status: "disconnected" });
    render(<App />);
    const dot = screen.getByTestId("status-dot");
    expect(dot.className).toContain("bg-red-500");
  });

  // ── M1 voice state tests ──────────────────────────────────

  it("shows voice-state section when connected", () => {
    useConnectionStore.setState({ status: "connected" });
    render(<App />);
    expect(screen.getByTestId("voice-section")).toBeInTheDocument();
  });

  it("does not show voice-state section when disconnected", () => {
    useConnectionStore.setState({ status: "disconnected" });
    render(<App />);
    expect(screen.queryByTestId("voice-section")).not.toBeInTheDocument();
  });

  it("shows Idle agent state badge by default", () => {
    useConnectionStore.setState({ status: "connected" });
    render(<App />);
    expect(screen.getByTestId("agent-state-badge")).toHaveTextContent("Idle");
  });

  it("shows Listening agent state badge", () => {
    useConnectionStore.setState({ status: "connected" });
    useSessionStore.setState({ agentState: "listening" });
    render(<App />);
    const badge = screen.getByTestId("agent-state-badge");
    expect(badge).toHaveTextContent("Listening");
    expect(badge.className).toContain("bg-pairion-amber");
  });

  it("shows Thinking agent state badge", () => {
    useConnectionStore.setState({ status: "connected" });
    useSessionStore.setState({ agentState: "thinking" });
    render(<App />);
    const badge = screen.getByTestId("agent-state-badge");
    expect(badge).toHaveTextContent("Thinking");
    expect(badge.className).toContain("bg-pairion-cyan");
  });

  it("shows Speaking agent state badge", () => {
    useConnectionStore.setState({ status: "connected" });
    useSessionStore.setState({ agentState: "speaking" });
    render(<App />);
    const badge = screen.getByTestId("agent-state-badge");
    expect(badge).toHaveTextContent("Speaking");
    expect(badge.className).toContain("bg-green-500");
  });

  it("shows partial transcript strip", () => {
    useConnectionStore.setState({ status: "connected" });
    useSessionStore.setState({
      active: true,
      agentState: "listening",
      partialTranscript: "what's the",
    });
    render(<App />);
    expect(screen.getByTestId("transcript-strip")).toHaveTextContent("what's the");
  });

  it("shows final transcript strip", () => {
    useConnectionStore.setState({ status: "connected" });
    useSessionStore.setState({
      active: true,
      agentState: "thinking",
      finalTranscript: "what's the weather",
    });
    render(<App />);
    expect(screen.getByTestId("transcript-strip")).toHaveTextContent("what's the weather");
  });

  it("does not show transcript strip when no transcript", () => {
    useConnectionStore.setState({ status: "connected" });
    useSessionStore.setState({ active: true, agentState: "listening" });
    render(<App />);
    expect(screen.queryByTestId("transcript-strip")).not.toBeInTheDocument();
  });

  it("does not show transcript strip when session inactive", () => {
    useConnectionStore.setState({ status: "connected" });
    useSessionStore.setState({
      active: false,
      partialTranscript: "test",
    });
    render(<App />);
    expect(screen.queryByTestId("transcript-strip")).not.toBeInTheDocument();
  });
});
