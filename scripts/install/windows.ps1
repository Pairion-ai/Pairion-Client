# scripts/install/windows.ps1
# One-command dependency setup for Pairion Client on Windows x86_64
# Uses vcpkg for Qt, opus, onnxruntime. Follows Phase B2.

param (
    [string]$VcpkgRoot = "$env:USERPROFILE\vcpkg"
)

Write-Host "=== Pairion Client Windows Dependency Installer ===" -ForegroundColor Green

# Install vcpkg if not present
if (-Not (Test-Path $VcpkgRoot)) {
    Write-Host "vcpkg not found. Cloning..."
    git clone https://github.com/microsoft/vcpkg.git $VcpkgRoot
    Push-Location $VcpkgRoot
    ./bootstrap-vcpkg.bat
    Pop-Location
}

# Install packages (triplet x64-windows for MSVC)
Write-Host "Installing packages via vcpkg (this may take time)..."
& "$VcpkgRoot\vcpkg" install qtbase:x64-windows qtdeclarative:x64-windows qtwebsockets:x64-windows qtmultimedia:x64-windows opus:x64-windows onnxruntime:x64-windows

Write-Host "✅ Windows dependencies installed via vcpkg." -ForegroundColor Green
Write-Host "Set VCPKG_ROOT=$VcpkgRoot before running CMake, or use -DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot/scripts/buildsystems/vcpkg.cmake"
Write-Host "Next: cmake --preset windows-x86_64-debug -DCMAKE_TOOLCHAIN_FILE=`"$VcpkgRoot/scripts/buildsystems/vcpkg.cmake`"" -ForegroundColor Cyan