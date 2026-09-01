$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot\..").Path

$manifestPath = Join-Path $PSScriptRoot "tool-versions.json"
$tools = Get-Content $manifestPath -Raw | ConvertFrom-Json
$config = $tools.vulkanSdkTools

$installDir = Join-Path $repoRoot $config.installDirectory

$requiredFiles = @(
    "Bin\dxc.exe",
    "Bin\VkLayer_khronos_validation.json",
    "Bin\VkLayer_gfxreconstruct.json",
    "Bin\VkLayer_gfxreconstruct.dll",
    "Bin\gfxrecon-replay.exe"
)

$installationReady = $true

foreach ($relativePath in $requiredFiles) {
    $path = Join-Path $installDir $relativePath

    if (-not (Test-Path $path -PathType Leaf)) {
        $installationReady = $false
        break
    }
}

if ($installationReady) {
    Write-Host "Vulkan SDK tools $($config.version) already installed."
    Write-Host "Location: $installDir"
    exit 0
}

$downloadDir = Join-Path $repoRoot ".tools\_cache"
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

if (Test-Path $installDir) {
    Write-Host "Removing incomplete Vulkan SDK tools installation..."
    Remove-Item -Recurse -Force $installDir
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


foreach ($relativePath in $requiredFiles) {
    $path = Join-Path $installDir $relativePath

    if (-not (Test-Path $path)) {
        throw "Expected Vulkan SDK tool missing: $path"
    }
}

Write-Host "Vulkan SDK tools $($config.version) ready."
Write-Host "Location: $installDir"