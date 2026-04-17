/**
 * Structured logger utility for the Pairion Client React frontend.
 *
 * Wraps log entries with structured fields (level, subsystem, timestamp)
 * and forwards them to the Rust backend via a Tauri command. Direct
 * `console.*` usage is banned in production code per CONVENTIONS §2.9.
 *
 * @module logger
 */

import { invoke } from "@tauri-apps/api/core";

/** Log level enumeration. */
export type LogLevel = "trace" | "debug" | "info" | "warn" | "error";

/** A structured log entry. */
export interface LogEntry {
  /** ISO 8601 timestamp. */
  timestamp: string;
  /** Log level. */
  level: LogLevel;
  /** Subsystem that produced the entry. */
  subsystem: string;
  /** Human-readable message. */
  message: string;
}

/**
 * Forwards a structured log entry to the Rust backend.
 *
 * @param level - The log level.
 * @param subsystem - The subsystem producing the log.
 * @param message - The log message.
 */
async function forwardToRust(level: LogLevel, subsystem: string, message: string): Promise<void> {
  try {
    await invoke("forward_log", { level, subsystem, message });
  } catch {
    // If Tauri is not available (e.g., in tests), silently drop the forward
  }
}

/**
 * Creates a logger instance for a specific subsystem.
 *
 * @param subsystem - The subsystem name for all entries from this logger.
 * @returns A logger object with level-specific methods.
 */
export function createLogger(subsystem: string) {
  return {
    /** Log at trace level. */
    trace: (message: string): void => {
      void forwardToRust("trace", subsystem, message);
    },
    /** Log at debug level. */
    debug: (message: string): void => {
      void forwardToRust("debug", subsystem, message);
    },
    /** Log at info level. */
    info: (message: string): void => {
      void forwardToRust("info", subsystem, message);
    },
    /** Log at warn level. */
    warn: (message: string): void => {
      void forwardToRust("warn", subsystem, message);
    },
    /** Log at error level. */
    error: (message: string): void => {
      void forwardToRust("error", subsystem, message);
    },
  };
}
