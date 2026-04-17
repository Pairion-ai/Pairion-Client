# Contributing to Pairion Client

Contributions are welcome! Pairion is built AI-first, and AI-generated PRs are explicitly encouraged.

## Before You Start

1. Read [CONVENTIONS.md](./CONVENTIONS.md) — every rule applies.
2. Read [Pairion-Client-Architecture.md](./Pairion-Client-Architecture.md) — understand the structure.
3. Check existing issues and PRs to avoid duplication.

## Development Workflow

1. Fork the repository.
2. Create a feature branch from `main`.
3. Write code, tests, and documentation in the same pass.
4. Ensure all checks pass: `cargo fmt`, `cargo clippy`, `pnpm lint`, `pnpm typecheck`, `cargo test`, `pnpm test`.
5. Open a PR with a clear description linking the relevant Claude Code prompt or issue.

## Commit Convention

We use [Conventional Commits](https://www.conventionalcommits.org/):

```
feat(ws): add exponential backoff reconnection
fix(audio): prevent buffer underrun on wake
docs(readme): update install instructions
test(hud): add visual regression for speaking state
```

## Quality Bar

- **100% test coverage** on public surfaces (Rust + React).
- **RustDoc** on every `pub` item in `src-tauri/`.
- **JSDoc** on every exported component, hook, and utility in `src/`.
- **No `console.log`** in React code. No `println!` in Rust production code.
- **CI must be green** before merge.

## Code of Conduct

See [CODE_OF_CONDUCT.md](./CODE_OF_CONDUCT.md).
