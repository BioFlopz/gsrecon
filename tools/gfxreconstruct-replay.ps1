$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot\..").Path
$manifestPath = Join-Path $PSScriptRoot "tool-versions.json"

$tools = Get-Content $manifestPath -Raw | ConvertFrom-Json
$config = $tools.vulkanSdkTools

$toolBin = Join-Path $repoRoot (
    Join-Path $config.installDirectory "Bin"
)

$replay = Join-Path $toolBin "gfxrecon-replay.exe"

$capture = Join-Path `
    $repoRoot `
    "captures\gfxreconstruct\gsrecon_frame_16.gfxr"

if (-not (Test-Path $replay)) {
    throw "GFXReconstruct replay tool is not bootstrapped."
}

if (-not (Test-Path $capture)) {
    throw "GFXReconstruct capture not found: $capture"
}

# Replay must not accidentally run with the capture layer enabled.
Remove-Item Env:VK_INSTANCE_LAYERS -ErrorAction SilentlyContinue
Remove-Item Env:GFXRECON_CAPTURE_FILE -ErrorAction SilentlyContinue
Remove-Item Env:GFXRECON_CAPTURE_FRAMES -ErrorAction SilentlyContinue

Write-Host "Replaying GFXReconstruct capture."
Write-Host "Capture: $capture"

& $replay $capture

if ($LASTEXITCODE -ne 0) {
    throw "GFXReconstruct replay failed with exit code $LASTEXITCODE."
}

Write-Host "GFXReconstruct replay succeeded."