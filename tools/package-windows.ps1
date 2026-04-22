<#
.SYNOPSIS
    Builds a Release configuration of Icebreaker 2 and produces a clean,
    redistributable .zip of the Windows runtime tree.

.DESCRIPTION
    Configures (if necessary) and builds the `build-release/` CMake tree
    using the same vcpkg toolchain as the dev `build/` tree, then packages
    `build-release/Icebreaker2/` into `dist/Icebreaker2-<version>-win64.zip`.

    The packaged zip contains:
      Icebreaker2.exe
      assets/
      SDL2*.dll  + transitive deps (libpng, zlib, freetype, ogg/vorbis, ...)
      README-WINDOWS.txt   (auto-generated runtime requirements)

    Excluded from the zip:
      .pdb, .ilk, .exp, .lib   (debug symbols, import libs)
      Anything not next to the exe

.PARAMETER Clean
    Remove `build-release/` before configuring, forcing a from-scratch build.

.PARAMETER SkipPackage
    Build but do not produce the .zip.

.PARAMETER VcpkgRoot
    Override the vcpkg root directory. Defaults to the value picked up from
    the dev build/ CMakeCache, then $env:VCPKG_ROOT, then C:\vcpkg.

.EXAMPLE
    pwsh tools\package-windows.ps1
    pwsh tools\package-windows.ps1 -Clean
#>
[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$SkipPackage,
    [string]$VcpkgRoot
)

$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path "$PSScriptRoot\.."
Set-Location $repoRoot

# ── Locate Visual Studio dev shell so cl.exe / cmake / dumpbin are on PATH ───
function Initialize-VsDevShell {
    if ($env:VSCMD_VER) { return }   # already inside a dev shell
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        $vswhere = "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
    }
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found; install Visual Studio 2022 (any edition)."
    }
    $vsRoot = & $vswhere -latest -property installationPath
    if (-not $vsRoot) { throw "No Visual Studio 2022 installation detected." }
    $devShell = Join-Path $vsRoot 'Common7\Tools\Launch-VsDevShell.ps1'
    if (-not (Test-Path $devShell)) { throw "Launch-VsDevShell.ps1 not found at $devShell" }
    Write-Host "Loading VS Dev Shell from $vsRoot..."
    & $devShell -Arch amd64 -SkipAutomaticLocation
    Set-Location $repoRoot
}

# ── Discover vcpkg toolchain ─────────────────────────────────────────────────
function Find-VcpkgToolchain {
    param([string]$Override)
    $candidates = @()
    if ($Override) { $candidates += $Override }

    # Honour whatever the existing build/ tree already uses, so the two trees
    # share the same set of installed packages.
    $devCache = Join-Path $repoRoot 'build\CMakeCache.txt'
    if (Test-Path $devCache) {
        $line = Select-String -Path $devCache -Pattern '^CMAKE_TOOLCHAIN_FILE:FILEPATH=(.+)$' -List
        if ($line) { $candidates += $line.Matches[0].Groups[1].Value }
    }

    if ($env:VCPKG_ROOT) {
        $candidates += (Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake')
    }
    $candidates += 'C:\vcpkg\scripts\buildsystems\vcpkg.cmake'

    foreach ($c in $candidates) {
        if ($c -and (Test-Path $c)) { return (Resolve-Path $c).Path }
    }
    throw "Could not find vcpkg.cmake. Install vcpkg and either set VCPKG_ROOT or pass -VcpkgRoot."
}

# ── Run ──────────────────────────────────────────────────────────────────────
Initialize-VsDevShell

if ($Clean -and (Test-Path build-release)) {
    Write-Host "Removing build-release/ ..."
    Remove-Item -Recurse -Force build-release
}

$toolchain = Find-VcpkgToolchain $VcpkgRoot
Write-Host "Using vcpkg toolchain: $toolchain"

# Clear env vars that would steer CMake away from our chosen toolchain
$env:VCPKG_ROOT = $null

if (-not (Test-Path build-release\CMakeCache.txt)) {
    Write-Host "Configuring build-release/ (Release, $toolchain) ..."
    & cmake -G Ninja -B build-release -S . `
        -DCMAKE_BUILD_TYPE=Release `
        "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
}

Write-Host "Building Release ..."
& cmake --build build-release --config Release
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

$runtimeDir = Join-Path $repoRoot 'build-release\Icebreaker2'
if (-not (Test-Path "$runtimeDir\Icebreaker2.exe")) {
    throw "Build succeeded but Icebreaker2.exe was not produced at $runtimeDir."
}

# ── Read version from CMakeLists.txt ────────────────────────────────────────
$projectVersion = '0.0'
$cmakeLists = Get-Content (Join-Path $repoRoot 'CMakeLists.txt') -Raw
if ($cmakeLists -match 'project\([^\)]*VERSION\s+(\d+(?:\.\d+){1,3})') {
    $projectVersion = $matches[1]
}

if ($SkipPackage) {
    Write-Host "Build complete. Runtime tree at $runtimeDir."
    return
}

# ── Stage a clean copy and zip it up ────────────────────────────────────────
$distDir   = Join-Path $repoRoot 'dist'
$stageName = "Icebreaker2-$projectVersion-win64"
$stageDir  = Join-Path $distDir $stageName
$zipPath   = Join-Path $distDir "$stageName.zip"

if (Test-Path $stageDir) { Remove-Item -Recurse -Force $stageDir }
if (Test-Path $zipPath)  { Remove-Item -Force $zipPath }
New-Item -ItemType Directory -Path $stageDir -Force | Out-Null

Write-Host "Staging redistributable files in $stageDir ..."
$excludeExt = @('.pdb', '.ilk', '.exp', '.lib')
Get-ChildItem -Path $runtimeDir -File | Where-Object {
    $excludeExt -notcontains $_.Extension.ToLower()
} | Copy-Item -Destination $stageDir
Copy-Item -Path (Join-Path $runtimeDir 'assets') -Destination $stageDir -Recurse

# Verify no debug-CRT DLLs slipped in (e.g. *d.dll from a misconfigured build)
$debugDlls = Get-ChildItem $stageDir -Filter '*.dll' | Where-Object {
    $_.Name -match '(VCRUNTIME|MSVCP|ucrtbase).*[dD]\.dll$'
}
if ($debugDlls) {
    throw "Debug CRT DLLs present in stage: $($debugDlls.Name -join ', '). Aborting."
}

# Dump exe imports for the README
$deps = & dumpbin /DEPENDENTS (Join-Path $stageDir 'Icebreaker2.exe') 2>&1 |
        Select-String '\.dll' | ForEach-Object { $_.Line.Trim() }

@"
Icebreaker II Turbo — Windows runtime
=====================================

Version : $projectVersion
Built   : $(Get-Date -Format 'yyyy-MM-dd HH:mm')

REQUIREMENTS
------------
* 64-bit Windows 10 or 11.
* Microsoft Visual C++ Redistributable for Visual Studio 2015-2022 (x64).
  Download: https://aka.ms/vs/17/release/vc_redist.x64.exe
  (Most up-to-date Windows 10/11 installs already have this.)

EVERYTHING ELSE IS BUNDLED
--------------------------
SDL2 and all its transitive dependencies, plus the game's assets folder, are
shipped alongside Icebreaker2.exe. Just unzip and run.

EXE-IMPORTED DLLS (for diagnostics)
-----------------------------------
$($deps -join "`n")
"@ | Set-Content (Join-Path $stageDir 'README-WINDOWS.txt') -Encoding UTF8

Write-Host "Compressing $stageDir -> $zipPath ..."
Compress-Archive -Path "$stageDir\*" -DestinationPath $zipPath -CompressionLevel Optimal

$zipSize = (Get-Item $zipPath).Length / 1MB
Write-Host ""
Write-Host ("Done. Wrote {0} ({1:N1} MB)." -f $zipPath, $zipSize)
Write-Host "Stage directory left at $stageDir for inspection."
