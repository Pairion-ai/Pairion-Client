# Test Harness

The mock WebSocket server fixture lives in `src-tauri/tests/harness.rs`.
It implements enough of the AsyncAPI protocol for Client tests to run
without a real Pairion Server.
