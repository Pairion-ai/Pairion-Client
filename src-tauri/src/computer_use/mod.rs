//! Computer-use action executor for the Pairion Client.
//!
//! Executes approved actions arriving via `ActionPending` messages.
//! In M0 this module contains only type declarations; the full
//! executor is implemented in M10.

use serde::{Deserialize, Serialize};

/// The kind of computer-use action to execute.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum ActionKind {
    /// Mouse click at a position.
    Click,
    /// Type text.
    Type,
    /// Send a keystroke combination.
    Keystroke,
    /// Run a script.
    Script,
    /// Edit a file.
    FileEdit,
    /// Create a file.
    FileCreate,
    /// Delete a file.
    FileDelete,
    /// Execute a shell command.
    Shell,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_action_kind_serialization() {
        let kind = ActionKind::FileEdit;
        let json = serde_json::to_string(&kind).unwrap();
        assert_eq!(json, "\"file-edit\"");
    }

    #[test]
    fn test_action_kind_deserialization() {
        let kind: ActionKind = serde_json::from_str("\"shell\"").unwrap();
        assert_eq!(kind, ActionKind::Shell);
    }
}
