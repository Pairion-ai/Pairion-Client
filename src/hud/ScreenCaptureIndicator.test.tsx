import { render, screen } from "@testing-library/react";
import { describe, it, expect } from "vitest";
import { ScreenCaptureIndicator } from "./ScreenCaptureIndicator";

describe("ScreenCaptureIndicator", () => {
  it("renders the REC indicator", () => {
    render(<ScreenCaptureIndicator />);
    expect(screen.getByTestId("screen-capture-indicator")).toBeInTheDocument();
    expect(screen.getByText("REC")).toBeInTheDocument();
  });

  it("has the correct styling", () => {
    render(<ScreenCaptureIndicator />);
    const indicator = screen.getByTestId("screen-capture-indicator");
    expect(indicator.className).toContain("bg-red-600");
  });
});
