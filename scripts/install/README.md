# Pairion-Client — Install Scripts

Idempotent one-command dependency installers. Run the script for your platform, then follow the CMake preset instructions.

## macOS (ARM64 or Intel)

```bash
chmod +x scripts/install/macos.sh
./scripts/install/macos.sh
```

Installs via Homebrew: `cmake`, `ninja`, `clang-format`, `qt@6`, `opus`, `onnxruntime`.

**Build:**
```bash
# Apple Silicon
cmake --preset macos-arm64-debug
cmake --build --preset macos-arm64-debug

# Intel
cmake --preset macos-x86_64-debug
cmake --build --preset macos-x86_64-debug
```

## Linux — Debian / Ubuntu (24.04+)

```bash
chmod +x scripts/install/linux-debian.sh
./scripts/install/linux-debian.sh
```

Installs via apt: Qt 6, libopus, lcov, build tools. Downloads onnxruntime from upstream GitHub releases.

## Linux — Fedora / RHEL

```bash
chmod +x scripts/install/linux-fedora.sh
./scripts/install/linux-fedora.sh
```

Installs via dnf: Qt 6, opus-devel, build tools. Downloads onnxruntime from upstream GitHub releases.

**Build (x86_64 or arm64):**
```bash
cmake --preset linux-x86_64-debug   # x86_64
cmake --preset linux-arm64-debug    # ARM64 (e.g. Raspberry Pi)
cmake --build --preset linux-x86_64-debug
```

## Windows (x86_64, PowerShell as Administrator)

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\scripts\install\windows.ps1
```

Installs vcpkg and all packages (`qtbase`, `qtdeclarative`, `qtwebsockets`, `qtmultimedia`, `opus`, `onnxruntime`).

**After install, set `VCPKG_ROOT` permanently:**
```powershell
[System.Environment]::SetEnvironmentVariable("VCPKG_ROOT", "$env:USERPROFILE\vcpkg", "User")
```

**Build:**
```powershell
cmake --preset windows-x86_64-debug
cmake --build --preset windows-x86_64-debug
```

The `windows-x86_64-debug` preset picks up `$env:VCPKG_ROOT` automatically via `CMAKE_TOOLCHAIN_FILE`.

## Post-install verification

```bash
cmake --version          # 3.25+
ninja --version
pkg-config --modversion opus
pkg-config --modversion libonnxruntime  # or check /usr/local/lib/cmake/onnxruntime/
qmake --version          # Qt 6.5+
```
