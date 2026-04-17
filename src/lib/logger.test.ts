import { describe, it, expect } from "vitest";
import { createLogger } from "./logger";

describe("logger", () => {
  it("creates a logger with all level methods", () => {
    const logger = createLogger("test");
    expect(typeof logger.trace).toBe("function");
    expect(typeof logger.debug).toBe("function");
    expect(typeof logger.info).toBe("function");
    expect(typeof logger.warn).toBe("function");
    expect(typeof logger.error).toBe("function");
  });

  it("does not throw when calling log methods", () => {
    const logger = createLogger("test");
    expect(() => logger.trace("trace message")).not.toThrow();
    expect(() => logger.debug("debug message")).not.toThrow();
    expect(() => logger.info("info message")).not.toThrow();
    expect(() => logger.warn("warn message")).not.toThrow();
    expect(() => logger.error("error message")).not.toThrow();
  });
});
