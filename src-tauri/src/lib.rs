//! Pairion Client library — the Rust backend for the Tauri application.
//!
//! This crate provides the real-time and OS-integration layer for Pairion:
//! WebSocket communication, audio pipeline, screen capture, keychain secrets,
//! computer-use execution, and log forwarding. The React frontend communicates
//! with this layer exclusively through typed Tauri commands and events.

pub mod audio;
pub mod commands;
pub mod computer_use;
pub mod config;
pub mod logs;
pub mod pipeline;
pub mod screen;
pub mod secrets;
pub mod state;
pub mod vad;
pub mod wake;
pub mod ws;

use config::AppConfig;
use state::AppState;
use std::sync::Arc;

/// Builds and runs the Tauri application.
///
/// Initializes logging, generates or loads the device identity, reads the
/// bearer token, sets up shared state, spawns the audio orchestrator and
/// WebSocket client, and launches the Tauri window.
#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    logs::init_tracing();
    tracing::info!("Pairion Client starting");

    let device_id = load_or_generate_device_id();
    let app_config = AppConfig::default();

    // Create the outbound WS message channel
    let (outbound_tx, outbound_rx) = ws::create_outbound_channel();

    // Create session state watch channel (shared between orchestrator and WS client)
    let (sess_tx, sess_rx) = tokio::sync::watch::channel(state::SessionState::default());
    let sess_tx = Arc::new(sess_tx);

    // Spawn the audio session orchestrator
    let orchestrator = pipeline::spawn_orchestrator(outbound_tx.clone(), Arc::clone(&sess_tx));

    let app_state = AppState::new(
        device_id.clone(),
        app_config.ws_url.clone(),
        outbound_tx,
        orchestrator,
        Arc::clone(&sess_tx),
        sess_rx,
    );
    let connection_tx = app_state.connection_tx.clone();
    let orchestrator_for_ws = app_state.orchestrator.clone();

    // Read bearer token
    let keychain = secrets::NoopKeychainProvider;
    let token = secrets::read_bearer_token(&keychain).unwrap_or_else(|e| {
        tracing::warn!(error = %e, "No bearer token found — connection will fail auth");
        String::new()
    });

    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .manage(app_state)
        .invoke_handler(tauri::generate_handler![
            commands::get_connection_state,
            commands::get_device_id,
            commands::is_server_reachable,
            commands::forward_log,
            commands::get_session_state,
            commands::start_listening_session,
            commands::stop_current_session,
            commands::send_text_message,
        ])
        .setup(move |_app| {
            let config = app_config.clone();
            let dev_id = device_id.clone();
            let tok = token.clone();
            let conn_tx = Arc::clone(&connection_tx);
            let sess_tx_ws = Arc::clone(&sess_tx);
            let orch_ws = orchestrator_for_ws.clone();

            // Spawn the WebSocket client on the Tokio runtime
            tauri::async_runtime::spawn(async move {
                ws::run_ws_client(
                    config,
                    dev_id,
                    tok,
                    conn_tx,
                    sess_tx_ws,
                    outbound_rx,
                    orch_ws,
                )
                .await;
            });

            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running Pairion");
}

/// Loads or generates a persistent device identifier.
///
/// In M0, this generates a new UUID on each run. In later milestones,
/// the device id is persisted to the preferences file.
fn load_or_generate_device_id() -> String {
    uuid::Uuid::new_v4().to_string()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_load_or_generate_device_id() {
        let id1 = load_or_generate_device_id();
        let id2 = load_or_generate_device_id();
        assert_ne!(id1, id2);
        assert!(uuid::Uuid::parse_str(&id1).is_ok());
    }
}
