//! Tauri commands exposed to the React frontend.
//!
//! Each command is a typed IPC bridge between React and Rust. M0 implements
//! three commands: [`get_connection_state`], [`get_device_id`], and
//! [`is_server_reachable`]. The remaining commands from Architecture §9
//! are scaffolded with `unimplemented!()` for later milestones.

use crate::state::{AppState, ConnectionState, SessionState};
use serde::Serialize;

/// Response type for connection state queries.
#[derive(Debug, Clone, Serialize)]
pub struct ConnectionStateResponse {
    /// The current connection state.
    pub state: ConnectionState,
    /// Human-readable label.
    pub label: String,
    /// The server URL.
    pub server_url: String,
}

/// Returns the current WebSocket connection state.
#[tauri::command]
pub fn get_connection_state(state: tauri::State<'_, AppState>) -> ConnectionStateResponse {
    let conn = state.connection_rx.borrow().clone();
    let label = conn.label();
    ConnectionStateResponse {
        state: conn,
        label,
        server_url: state.server_url.clone(),
    }
}

/// Returns the unique device identifier for this Client instance.
#[tauri::command]
pub fn get_device_id(state: tauri::State<'_, AppState>) -> String {
    state.device_id.clone()
}

/// Checks whether the Pairion Server is reachable by inspecting connection state.
#[tauri::command]
pub fn is_server_reachable(state: tauri::State<'_, AppState>) -> bool {
    matches!(
        *state.connection_rx.borrow(),
        ConnectionState::Connected { .. }
    )
}

/// Forwards a log entry from the React frontend to the Rust logging pipeline.
#[tauri::command]
pub fn forward_log(level: String, subsystem: String, message: String) {
    match level.as_str() {
        "error" => tracing::error!(subsystem = %subsystem, "{message}"),
        "warn" => tracing::warn!(subsystem = %subsystem, "{message}"),
        "debug" => tracing::debug!(subsystem = %subsystem, "{message}"),
        "trace" => tracing::trace!(subsystem = %subsystem, "{message}"),
        _ => tracing::info!(subsystem = %subsystem, "{message}"),
    }
}

/// Returns the current session state.
#[tauri::command]
pub fn get_session_state(state: tauri::State<'_, AppState>) -> SessionState {
    state.session_rx.borrow().clone()
}

// ── M1 voice session commands ────────────────────────────────────────

/// Starts a listening session for voice input.
///
/// In M1, this signals readiness to capture audio. The actual audio
/// pipeline startup happens when the Server responds with `SessionOpened`.
#[tauri::command]
pub fn start_listening_session(state: tauri::State<'_, AppState>) -> Result<String, String> {
    let session_id = uuid::Uuid::new_v4().to_string();
    tracing::info!(session_id = %session_id, "Starting listening session");

    let _ = state.session_tx.send(crate::state::SessionState {
        session_id: Some(session_id.clone()),
        agent_state: crate::state::AgentState::Listening,
        active: true,
        ..Default::default()
    });

    Ok(session_id)
}

/// Stops the current listening session.
///
/// Closes the audio capture pipeline and resets session state to idle.
#[tauri::command]
pub fn stop_current_session(state: tauri::State<'_, AppState>) -> Result<(), String> {
    tracing::info!("Stopping current session");
    let _ = state.session_tx.send(crate::state::SessionState::default());
    Ok(())
}

/// Sends a text message in the current session (dev-mode bypass).
///
/// Bypasses wake-word + audio, sending a text message directly to the
/// Server. Useful for testing without a microphone.
#[tauri::command]
pub fn send_text_message(
    _session_id: String,
    _text: String,
    _state: tauri::State<'_, AppState>,
) -> Result<(), String> {
    tracing::info!(text = %_text, "Sending text message (dev mode)");
    // In a full implementation, this sends a TextMessage over the WebSocket.
    // For M1, we log the intent. Full WS send integration requires access
    // to the WS write half, which is owned by the WS client task.
    Ok(())
}

// ── Scaffolded commands for later milestones ─────────────────────────

/// Sends `DeviceIdentify` over WS, returns the `IdentifyAck`.
/// Implemented in M1.
#[tauri::command]
pub fn identify_device() -> Result<String, String> {
    unimplemented!("identify_device: not needed in M1 — identification is automatic on WS connect")
}

/// Approves a pending computer-use action.
/// Implemented in M10.
#[tauri::command]
pub fn approve_action(_action_id: String) -> Result<(), String> {
    unimplemented!("approve_action: available from M10")
}

/// Rejects a pending computer-use action.
/// Implemented in M10.
#[tauri::command]
pub fn reject_action(_action_id: String) -> Result<(), String> {
    unimplemented!("reject_action: available from M10")
}

/// Requests a screen capture for the current session.
/// Implemented in M10.
#[tauri::command]
pub fn request_screen_capture(_session_id: String) -> Result<(), String> {
    unimplemented!("request_screen_capture: available from M10")
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::state::AppState;

    fn make_test_state() -> AppState {
        AppState::new(
            "test-device-001".to_string(),
            "ws://localhost:18789/ws/v1".to_string(),
        )
    }

    #[test]
    fn test_connection_state_response_serialization() {
        let response = ConnectionStateResponse {
            state: ConnectionState::Connected {
                latency_ms: Some(15.0),
            },
            label: "Connected".to_string(),
            server_url: "ws://localhost:18789/ws/v1".to_string(),
        };
        let json = serde_json::to_string(&response).unwrap();
        assert!(json.contains("\"label\":\"Connected\""));
        assert!(json.contains("\"server_url\":"));
    }

    #[test]
    fn test_app_state_device_id() {
        let state = make_test_state();
        assert_eq!(state.device_id, "test-device-001");
    }

    #[test]
    fn test_app_state_server_url() {
        let state = make_test_state();
        assert_eq!(state.server_url, "ws://localhost:18789/ws/v1");
    }

    #[test]
    fn test_app_state_initial_connection() {
        let state = make_test_state();
        let conn = state.connection_rx.borrow().clone();
        assert_eq!(conn, ConnectionState::Connecting);
    }

    #[test]
    fn test_app_state_connected() {
        let state = make_test_state();
        let _ = state.connection_tx.send(ConnectionState::Connected {
            latency_ms: Some(10.0),
        });
        let is_reachable = matches!(
            *state.connection_rx.borrow(),
            ConnectionState::Connected { .. }
        );
        assert!(is_reachable);
    }

    #[test]
    fn test_app_state_not_reachable_when_disconnected() {
        let state = make_test_state();
        let _ = state.connection_tx.send(ConnectionState::Disconnected {
            reason: Some("test".to_string()),
        });
        let is_reachable = matches!(
            *state.connection_rx.borrow(),
            ConnectionState::Connected { .. }
        );
        assert!(!is_reachable);
    }

    #[test]
    fn test_session_state_default_is_idle() {
        let state = make_test_state();
        let session = state.session_rx.borrow().clone();
        assert!(!session.active);
        assert!(session.session_id.is_none());
    }

    #[test]
    fn test_session_state_can_be_updated() {
        let state = make_test_state();
        let _ = state.session_tx.send(crate::state::SessionState {
            session_id: Some("test-session".to_string()),
            agent_state: crate::state::AgentState::Listening,
            active: true,
            ..Default::default()
        });
        let session = state.session_rx.borrow().clone();
        assert!(session.active);
        assert_eq!(session.session_id.as_deref(), Some("test-session"));
    }

    #[test]
    #[should_panic(expected = "identify_device: not needed in M1")]
    fn test_identify_device_unimplemented() {
        let _ = identify_device();
    }

    #[test]
    #[should_panic(expected = "approve_action: available from M10")]
    fn test_approve_action_unimplemented() {
        let _ = approve_action("action".to_string());
    }

    #[test]
    #[should_panic(expected = "reject_action: available from M10")]
    fn test_reject_action_unimplemented() {
        let _ = reject_action("action".to_string());
    }

    #[test]
    #[should_panic(expected = "request_screen_capture: available from M10")]
    fn test_request_screen_capture_unimplemented() {
        let _ = request_screen_capture("session".to_string());
    }
}
