$ErrorActionPreference = "Stop"

$repoRoot = (
    Resolve-Path -Path (
        Join-Path -Path $PSScriptRoot -ChildPath ".."
    )
).Path

$versionsFile = Join-Path `
    -Path $PSScriptRoot `
    -ChildPath "tool-versions.json"

if (-not (Test-Path -Path $versionsFile -PathType Leaf))
{
    Write-Error "Tool manifest not found: $versionsFile"
    exit 1
}

$config = Get-Content `
    -Path $versionsFile `
    -Raw |
    ConvertFrom-Json

$failures = @()

#
# RenderDoc
#

$renderDoc = $config.renderdoc

$renderDocDirectory = Join-Path `
    -Path $repoRoot `
    -ChildPath $renderDoc.installDirectory

$renderDocCmd = Join-Path `
    -Path $renderDocDirectory `
    -ChildPath "renderdoccmd.exe"

if (-not (Test-Path -Path $renderDocCmd -PathType Leaf))
{
    $failures += "RenderDoc not found: $renderDocCmd"
}
else
{
    $output = & $renderDocCmd version

    if ($LASTEXITCODE -ne 0)
    {
        $failures += "renderdoccmd version failed."
    }
    else
    {
        $outputText = $output -join [Environment]::NewLine
        $expectedVersion = "v$($renderDoc.version)"

        if ($outputText -notmatch [regex]::Escape($expectedVersion))
        {
            $failures += `
                "Expected RenderDoc $expectedVersion, got: $outputText"
        }

        if ($outputText -notmatch "x64")
        {
            $failures += `
                "Expected x64 RenderDoc, got: $outputText"
        }
    }
}

#
# Vulkan SDK tools
#

$vulkanTools = $config.vulkanSdkTools

$vulkanDirectory = Join-Path `
    -Path $repoRoot `
    -ChildPath $vulkanTools.installDirectory

$vulkanBin = Join-Path `
    -Path $vulkanDirectory `
    -ChildPath "Bin"

$requiredVulkanFiles = @(
    "dxc.exe",
    "gfxrecon-replay.exe",
    "VkLayer_gfxreconstruct.dll",
    "VkLayer_gfxreconstruct.json"
)

foreach ($file in $requiredVulkanFiles)
{
    $path = Join-Path `
        -Path $vulkanBin `
        -ChildPath $file

    if (-not (Test-Path -Path $path -PathType Leaf))
    {
        $failures += "Vulkan tool not found: $path"
    }
}

#
# CUDA Runtime
#

$cudaRuntime = $config.cudaRuntime

$cudaDirectory = Join-Path `
    -Path $repoRoot `
    -ChildPath $cudaRuntime.installDirectory

$cudaHeader = Join-Path `
    -Path $cudaDirectory `
    -ChildPath "include\cuda_runtime_api.h"

$cudaLibrary = Join-Path `
    -Path $cudaDirectory `
    -ChildPath "lib\x64\cudart.lib"

$cudaDllDirectory = Join-Path `
    -Path $cudaDirectory `
    -ChildPath "bin\x64"

if (-not (Test-Path -Path $cudaHeader -PathType Leaf))
{
    $failures += "CUDA Runtime header not found: $cudaHeader"
}

if (-not (Test-Path -Path $cudaLibrary -PathType Leaf))
{
    $failures += "CUDA Runtime library not found: $cudaLibrary"
}

$cudaDll = Get-ChildItem `
    -Path $cudaDllDirectory `
    -Filter "cudart64_*.dll" `
    -File `
    -ErrorAction SilentlyContinue |
    Select-Object -First 1

if ($null -eq $cudaDll)
{
    $failures += "CUDA Runtime DLL not found in: $cudaDllDirectory"
}


#
# Result
#

if ($failures.Count -ne 0)
{
    Write-Host ""
    Write-Host "Tool check FAILED"

    foreach ($failure in $failures)
    {
        Write-Host "  - $failure"
    }

    exit 1
}

Write-Host "RenderDoc OK"
Write-Host "Vulkan SDK tools OK"
Write-Host "CUDA Runtime OK"
Write-Host "All project tools OK"