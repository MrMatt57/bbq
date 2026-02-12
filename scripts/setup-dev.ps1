#Requires -Version 5.1
<#
.SYNOPSIS
    Sets up the BBQ Controller development environment on Windows.

.DESCRIPTION
    Installs all required tools and dependencies for firmware development,
    web UI development, and 3D enclosure design. Safe to run multiple times;
    skips anything already installed.

    Run from any directory:
        powershell -ExecutionPolicy Bypass -File scripts\setup-dev.ps1

.NOTES
    Requires Windows 10 1709+ or Windows 11 (for winget).
    Must be run from an elevated (Admin) PowerShell for winget installs.
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# --- Helpers ---

function Write-Step($msg) {
    Write-Host "`n>>> $msg" -ForegroundColor Cyan
}

function Write-Ok($msg) {
    Write-Host "    [OK] $msg" -ForegroundColor Green
}

function Write-Skip($msg) {
    Write-Host "    [SKIP] $msg" -ForegroundColor Yellow
}

function Write-Fail($msg) {
    Write-Host "    [FAIL] $msg" -ForegroundColor Red
}

function Test-Command($cmd) {
    $null = Get-Command $cmd -ErrorAction SilentlyContinue
    return $?
}

function Refresh-Path {
    # Reload PATH from registry so newly-installed tools are visible
    $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath    = [Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path    = "$machinePath;$userPath"
}

# --- Pre-flight ---

Write-Host ""
Write-Host "============================================" -ForegroundColor White
Write-Host "  BBQ Controller - Dev Environment Setup" -ForegroundColor White
Write-Host "============================================" -ForegroundColor White

# Check winget
Write-Step "Checking for winget (Windows Package Manager)"
if (Test-Command "winget") {
    $wingetVer = (winget --version) 2>&1
    Write-Ok "winget $wingetVer"
} else {
    Write-Fail "winget not found. Install 'App Installer' from the Microsoft Store."
    Write-Host "    https://aka.ms/getwinget" -ForegroundColor Gray
    exit 1
}

$needsPathRefresh = $false

# --- 1. Git ---

Write-Step "Checking Git"
if (Test-Command "git") {
    $gitVer = (git --version) 2>&1
    Write-Ok "$gitVer"
} else {
    Write-Host "    Installing Git..." -ForegroundColor White
    winget install --id Git.Git -e --accept-source-agreements --accept-package-agreements
    $needsPathRefresh = $true
    Write-Ok "Git installed"
}

# --- 2. Python ---

Write-Step "Checking Python 3"
if (Test-Command "python") {
    $pyVer = (python --version) 2>&1
    if ($pyVer -match "Python 3") {
        Write-Ok "$pyVer"
    } else {
        Write-Host "    Python 2 found, installing Python 3..." -ForegroundColor White
        winget install --id Python.Python.3.12 -e --accept-source-agreements --accept-package-agreements
        $needsPathRefresh = $true
        Write-Ok "Python 3.12 installed"
    }
} else {
    Write-Host "    Installing Python 3.12..." -ForegroundColor White
    winget install --id Python.Python.3.12 -e --accept-source-agreements --accept-package-agreements
    $needsPathRefresh = $true
    Write-Ok "Python 3.12 installed"
}

# --- 3. Node.js ---

Write-Step "Checking Node.js"
Refresh-Path
if (Test-Command "node") {
    $nodeVer = (node --version) 2>&1
    Write-Ok "Node.js $nodeVer"
} else {
    Write-Host "    Installing Node.js LTS..." -ForegroundColor White
    winget install --id OpenJS.NodeJS.LTS -e --accept-source-agreements --accept-package-agreements
    $needsPathRefresh = $true
    Write-Ok "Node.js LTS installed"
}

# --- 4. OpenSCAD ---

Write-Step "Checking OpenSCAD"
if (Test-Command "openscad") {
    Write-Ok "OpenSCAD found"
} else {
    $openscadPath = 'C:\Program Files\OpenSCAD\openscad.exe'
    if (Test-Path $openscadPath) {
        Write-Ok "OpenSCAD found at $openscadPath (not on PATH, add manually if needed)"
    } else {
        Write-Host "    Installing OpenSCAD..." -ForegroundColor White
        winget install --id OpenSCAD.OpenSCAD -e --accept-source-agreements --accept-package-agreements
        $needsPathRefresh = $true
        Write-Ok "OpenSCAD installed"
    }
}

# Refresh PATH if we installed anything
if ($needsPathRefresh) {
    Write-Step "Refreshing PATH"
    Refresh-Path
    Write-Ok "PATH updated (you may need to restart your terminal for all tools)"
}

# --- 5. PlatformIO CLI ---

Write-Step "Checking PlatformIO CLI"
if (Test-Command "pio") {
    $pioVer = (pio --version) 2>&1
    Write-Ok "$pioVer"
} else {
    Write-Host "    Installing PlatformIO CLI via pip..." -ForegroundColor White
    # pip writes PATH warnings to stderr which PowerShell treats as errors; suppress them
    $ErrorActionPreference = "Continue"
    python -m pip install --user --upgrade pip 2>&1 | Out-Null
    python -m pip install --user platformio 2>&1 | Out-Null
    $ErrorActionPreference = "Stop"
    # Add user Scripts dir to PATH for this session
    $userScripts = python -c "import sysconfig; print(sysconfig.get_path('scripts'))" 2>&1
    if (Test-Path $userScripts) {
        $env:Path = "$userScripts;$env:Path"
    }
    Refresh-Path
    if (Test-Command "pio") {
        $pioVer = (pio --version) 2>&1
        Write-Ok "$pioVer"
    } else {
        $pipScripts = python -c "import sysconfig; print(sysconfig.get_path('scripts'))" 2>&1
        Write-Ok "PlatformIO installed (you may need to add to PATH: $pipScripts)"
    }
}

# --- 6. Project dependencies ---

Write-Step "Installing simulator dependencies (npm)"
$simulatorDir = Join-Path $PSScriptRoot "..\simulator"
if (Test-Path (Join-Path $simulatorDir "package.json")) {
    Push-Location $simulatorDir
    if (Test-Command "npm") {
        npm install 2>&1
        Write-Ok "Simulator npm packages installed"
    } else {
        Write-Skip "npm not available yet. Restart terminal, then run: cd simulator; npm install"
    }
    Pop-Location
} else {
    Write-Skip "simulator/package.json not found yet (created during implementation)"
}

Write-Step "Installing PlatformIO platform and libraries"
$firmwareDir = Join-Path $PSScriptRoot "..\firmware"
if (Test-Path (Join-Path $firmwareDir "platformio.ini")) {
    if (Test-Command "pio") {
        Push-Location $firmwareDir
        pio pkg install 2>&1
        Write-Ok "PlatformIO packages installed"
        Pop-Location
    } else {
        Write-Skip "pio not available yet. Restart terminal, then run: cd firmware; pio pkg install"
    }
} else {
    Write-Skip "firmware/platformio.ini not found yet (created during implementation)"
}

# --- Summary ---

Write-Host ""
Write-Host "============================================" -ForegroundColor White
Write-Host "  Setup Complete" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor White
Write-Host ""

# Final verification
$tools = @(
    @{ Name = "Git";          Cmd = "git" },
    @{ Name = "Python";       Cmd = "python" },
    @{ Name = "Node.js";      Cmd = "node" },
    @{ Name = "npm";          Cmd = "npm" },
    @{ Name = "PlatformIO";   Cmd = "pio" }
)

$allGood = $true
foreach ($tool in $tools) {
    if (Test-Command $tool.Cmd) {
        $ver = & $tool.Cmd --version 2>&1 | Select-Object -First 1
        Write-Host "    $($tool.Name): $ver" -ForegroundColor Green
    } else {
        Write-Host "    $($tool.Name): not on PATH (restart terminal)" -ForegroundColor Yellow
        $allGood = $false
    }
}

Write-Host ""
if ($allGood) {
    Write-Host "All tools ready. You can start developing!" -ForegroundColor Green
} else {
    Write-Host "Some tools need a terminal restart to appear on PATH." -ForegroundColor Yellow
    Write-Host "Close this terminal, open a new one, and run this script again to verify." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Next steps:" -ForegroundColor White
Write-Host '    cd simulator; npm run dev        # Web UI development' -ForegroundColor Gray
Write-Host '    cd firmware;  pio run            # Build firmware' -ForegroundColor Gray
Write-Host '    cd firmware;  pio test -e native # Run tests' -ForegroundColor Gray
Write-Host ""
