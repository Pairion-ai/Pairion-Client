import { render, screen } from "@testing-library/react";
import { describe, it, expect, beforeEach } from "vitest";
import App from "./App";
import { useConnectionStore } from "./state/connectionStore";
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
});
