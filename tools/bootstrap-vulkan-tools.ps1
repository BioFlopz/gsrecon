$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot\..\..").Path

$manifestPath = Join-Path $PSScriptRoot "tool-versions.json"
$tools = Get-Content $manifestPath -Raw | ConvertFrom-Json
$config = $tools.vulkanSdkTools

$installDir = Join-Path $repoRoot $config.installDirectory
$downloadDir = Join-Path $repoRoot ".tools\downloads"
$installer = Join-Path $downloadDir $config.installerName

New-Item -ItemType Directory -Force $downloadDir | Out-Null

if (-not (Test-Path $installer)) {
    Write-Host "Downloading Vulkan SDK tools $($config.version)..."
    Invoke-WebRequest $config.downloadUrl -OutFile $installer
}

$actualHash = (Get-FileHash $installer -Algorithm SHA256).Hash

if ($actualHash -ne $config.sha256) {
    throw "Vulkan SDK installer SHA256 mismatch."
}

Write-Host "Installing project-local Vulkan SDK tools..."

& $installer `
    --root $installDir `
    --accept-licenses `
    --default-answer `
    --confirm-command `
    install `
    copy_only=1

if ($LASTEXITCODE -ne 0) {
    throw "Vulkan SDK installer failed with exit code $LASTEXITCODE."
}

$requiredFiles = @(
    "Bin\VkLayer_khronos_validation.json",
    "Bin\VkLayer_gfxreconstruct.json",
    "Bin\gfxrecon-replay.exe"
)

foreach ($relativePath in $requiredFiles) {
    $path = Join-Path $installDir $relativePath

    if (-not (Test-Path $path)) {
        throw "Expected Vulkan SDK tool missing: $path"
    }
}

Write-Host "Vulkan SDK tools $($config.version) ready."
Write-Host "Location: $installDir"