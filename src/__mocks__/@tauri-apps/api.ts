/** Mock for @tauri-apps/api used in Vitest tests. */
export const core = {
  invoke: async (): Promise<unknown> => {
    return undefined;
  },
};

export const invoke = core.invoke;

export const event = {
  listen: async (): Promise<() => void> => {
    return () => {};
  },
  emit: async (): Promise<void> => {},
};
