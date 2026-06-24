$ErrorActionPreference = "Stop"

$applicationDir = Resolve-Path (Join-Path $PSScriptRoot "..\application")
$manifestPath = Join-Path $applicationDir "vcpkg.json"
$configureArgs = @("--preset", "msvc-debug")

function Ensure-VcpkgBaseline {
    param(
        [string]$VcpkgRoot,
        [string]$ManifestPath
    )

    $manifest = Get-Content -Path $ManifestPath -Raw | ConvertFrom-Json
    if ($manifest.PSObject.Properties.Name -contains "builtin-baseline") {
        return
    }

    $baseline = $null
    try {
        $baseline = (& git -C $VcpkgRoot rev-parse HEAD 2>$null).Trim()
    } catch {
        $baseline = $null
    }

    if ([string]::IsNullOrWhiteSpace($baseline)) {
        $vcpkgExe = Join-Path $VcpkgRoot "vcpkg.exe"
        if (-not (Test-Path -LiteralPath $vcpkgExe)) {
            throw "vcpkg.exe was not found at '$vcpkgExe'."
        }

        Write-Host "Adding initial vcpkg manifest baseline with vcpkg."
        & $vcpkgExe x-update-baseline --add-initial-baseline "--x-manifest-root=$applicationDir"
        if ($LASTEXITCODE -ne 0) {
            throw "vcpkg baseline update failed. Exit code: $LASTEXITCODE"
        }
        return
    }

    Write-Host "Adding initial vcpkg manifest baseline: $baseline"
    $manifest | Add-Member -NotePropertyName "builtin-baseline" -NotePropertyValue $baseline
    $manifest |
        ConvertTo-Json -Depth 16 |
        Set-Content -Path $ManifestPath -Encoding utf8
}

if ($env:VCPKG_ROOT) {
    $vcpkgToolchain = Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"
    $vcpkgPorts = Join-Path $env:VCPKG_ROOT "ports"
    $vcpkgVersions = Join-Path $env:VCPKG_ROOT "versions"

    if ((Test-Path -LiteralPath $vcpkgToolchain) -and
        (Test-Path -LiteralPath $vcpkgPorts) -and
        (Test-Path -LiteralPath $vcpkgVersions)) {
        Ensure-VcpkgBaseline -VcpkgRoot $env:VCPKG_ROOT -ManifestPath $manifestPath
        $configureArgs += "-DCMAKE_TOOLCHAIN_FILE=$vcpkgToolchain"
    } else {
        Write-Warning "VCPKG_ROOT is set, but it does not look like a full vcpkg ports checkout: $env:VCPKG_ROOT"
        Write-Warning "Skipping vcpkg toolchain. Clone https://github.com/microsoft/vcpkg and set VCPKG_ROOT to that path to auto-install wxWidgets."
    }
}

Push-Location $applicationDir
try {
    cmake @configureArgs
    cmake --build --preset msvc-debug
} finally {
    Pop-Location
}
