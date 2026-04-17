/** Mock for @tauri-apps/plugin-os used in Vitest tests. */
export const platform = async (): Promise<string> => "darwin";
export const version = async (): Promise<string> => "14.0.0";
export const arch = async (): Promise<string> => "aarch64";
