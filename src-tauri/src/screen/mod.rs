//! Screen capture pipeline for the Pairion Client.
//!
//! Integrates with macOS ScreenCaptureKit for the "what am I looking at"
//! and computer-use features. In M0 this module contains only type
//! declarations; the full pipeline is implemented in M10.

/// Whether screen capture is currently active.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub enum ScreenCaptureState {
    /// Screen capture is not active.
    #[default]
    Inactive,
    /// Screen capture is active — the visible indicator must be shown.
    Active,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_screen_capture_state_default() {
        assert_eq!(ScreenCaptureState::default(), ScreenCaptureState::Inactive);
    }

    #[test]
    fn test_screen_capture_state_equality() {
        assert_eq!(ScreenCaptureState::Active, ScreenCaptureState::Active);
        assert_ne!(ScreenCaptureState::Active, ScreenCaptureState::Inactive);
    }
}
