$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot\..").Path
$manifestPath = Join-Path $PSScriptRoot "tool-versions.json"

$tools = Get-Content $manifestPath -Raw | ConvertFrom-Json
$config = $tools.vulkanSdkTools

$toolBin = Join-Path $repoRoot (
    Join-Path $config.installDirectory "Bin"
)

$executable = Join-Path `
    $repoRoot `
    "build\windows-msvc\Release\gsrecon.exe"

$captureDir = Join-Path `
    $repoRoot `
    "captures\gfxreconstruct"

if (-not (Test-Path $executable)) {
    throw "gsrecon Release executable not found: $executable"
}

if (-not (Test-Path (
    Join-Path $toolBin "VkLayer_gfxreconstruct.json"
))) {
    throw "GFXReconstruct capture layer is not bootstrapped."
}

New-Item `
    -ItemType Directory `
    -Force `
    $captureDir | Out-Null

$env:VK_LAYER_PATH = $toolBin
$env:VK_INSTANCE_LAYERS = "VK_LAYER_LUNARG_gfxreconstruct"

$env:GFXRECON_CAPTURE_FILE = Join-Path `
    $captureDir `
    "gsrecon.gfxr"
$env:GFXRECON_CAPTURE_FILE_TIMESTAMP = "false"
$env:GFXRECON_CAPTURE_FRAMES = "16"
$env:GFXRECON_LOG_LEVEL = "warning"

Write-Host "Starting GFXReconstruct capture."
Write-Host "Frame: 16"

& $executable