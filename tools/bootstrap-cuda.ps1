$ErrorActionPreference = "Stop"

$repoRoot = (
    Resolve-Path -Path (
        Join-Path -Path $PSScriptRoot -ChildPath ".."
    )
).Path

$manifestPath = Join-Path `
    -Path $PSScriptRoot `
    -ChildPath "tool-versions.json"

$tools = Get-Content `
    -Path $manifestPath `
    -Raw |
    ConvertFrom-Json

$config = $tools.cudaRuntime

$installDirectory = Join-Path `
    -Path $repoRoot `
    -ChildPath $config.installDirectory

$cacheDirectory = Join-Path `
    -Path $repoRoot `
    -ChildPath ".tools\_cache"

$archivePath = Join-Path `
    -Path $cacheDirectory `
    -ChildPath $config.archiveName

$headerPath = Join-Path `
    -Path $installDirectory `
    -ChildPath "include\cuda_runtime_api.h"

$libraryPath = Join-Path `
    -Path $installDirectory `
    -ChildPath "lib\x64\cudart.lib"

$dllDirectory = Join-Path `
    -Path $installDirectory `
    -ChildPath "bin\x64"


#
# Check an existing project-local installation.
#

$dll = Get-ChildItem `
    -Path $dllDirectory `
    -Filter "cudart64_*.dll" `
    -File `
    -ErrorAction SilentlyContinue |
    Select-Object -First 1

if (
    (Test-Path -Path $headerPath -PathType Leaf) -and
    (Test-Path -Path $libraryPath -PathType Leaf) -and
    ($null -ne $dll)
)
{
    Write-Host "CUDA Runtime $($config.version) already installed."
    Write-Host "Location: $installDirectory"
    exit 0
}


#
# Remove an incomplete installation.
#

if (Test-Path -Path $installDirectory)
{
    Write-Host "Removing incomplete CUDA Runtime installation..."

    Remove-Item `
        -Path $installDirectory `
        -Recurse `
        -Force
}


#
# Prepare download directory.
#

New-Item `
    -ItemType Directory `
    -Path $cacheDirectory `
    -Force |
    Out-Null


#
# Download the pinned NVIDIA CUDA Runtime archive.
#

if (-not (Test-Path -Path $archivePath -PathType Leaf))
{
    Write-Host "Downloading CUDA Runtime $($config.version)..."

    Invoke-WebRequest `
        -Uri $config.downloadUrl `
        -OutFile $archivePath `
        -UseBasicParsing
}


#
# Verify SHA-256.
#

$expectedHash = $config.sha256.ToUpperInvariant()

$actualHash = (
    Get-FileHash `
        -Path $archivePath `
        -Algorithm SHA256
).Hash.ToUpperInvariant()

if ($actualHash -ne $expectedHash)
{
    Remove-Item `
        -Path $archivePath `
        -Force

    throw @"
CUDA Runtime archive SHA-256 mismatch.

Expected:
$expectedHash

Actual:
$actualHash

The downloaded archive was deleted.
"@
}

Write-Host "CUDA Runtime archive SHA-256 OK"


#
# Extract into a temporary directory.
#

$tempDirectory = Join-Path `
    -Path $cacheDirectory `
    -ChildPath "cuda-cudart-$($config.version)-extract"

if (Test-Path -Path $tempDirectory)
{
    Remove-Item `
        -Path $tempDirectory `
        -Recurse `
        -Force
}

New-Item `
    -ItemType Directory `
    -Path $tempDirectory `
    -Force |
    Out-Null

Expand-Archive `
    -Path $archivePath `
    -DestinationPath $tempDirectory


#
# Locate the CUDA Runtime package root.
#

$runtimeHeader = Get-ChildItem `
    -Path $tempDirectory `
    -Filter "cuda_runtime_api.h" `
    -Recurse `
    -File |
    Select-Object -First 1

if ($null -eq $runtimeHeader)
{
    Remove-Item `
        -Path $tempDirectory `
        -Recurse `
        -Force

    throw "cuda_runtime_api.h was not found in the CUDA Runtime archive."
}

$includeDirectory = Split-Path `
    -Path $runtimeHeader.FullName `
    -Parent

$packageRoot = Split-Path `
    -Path $includeDirectory `
    -Parent


#
# Verify expected files inside the extracted archive.
#

$archiveLibrary = Join-Path `
    -Path $packageRoot `
    -ChildPath "lib\x64\cudart.lib"

$archiveDllDirectory = Join-Path `
    -Path $packageRoot `
    -ChildPath "bin\x64"

$archiveDll = Get-ChildItem `
    -Path $archiveDllDirectory `
    -Filter "cudart64_*.dll" `
    -File `
    -ErrorAction SilentlyContinue |
    Select-Object -First 1

if (-not (Test-Path -Path $archiveLibrary -PathType Leaf))
{
    Remove-Item `
        -Path $tempDirectory `
        -Recurse `
        -Force

    throw "cudart.lib was not found in the CUDA Runtime archive."
}

if ($null -eq $archiveDll)
{
    Remove-Item `
        -Path $tempDirectory `
        -Recurse `
        -Force

    throw "CUDA Runtime DLL was not found in the archive."
}


#
# Install the project-local CUDA Runtime.
#

New-Item `
    -ItemType Directory `
    -Path $installDirectory `
    -Force |
    Out-Null

Copy-Item `
    -Path (
        Join-Path `
            -Path $packageRoot `
            -ChildPath "*"
    ) `
    -Destination $installDirectory `
    -Recurse


#
# Remove temporary extraction directory.
#

Remove-Item `
    -Path $tempDirectory `
    -Recurse `
    -Force


#
# Verify the completed installation.
#

if (-not (Test-Path -Path $headerPath -PathType Leaf))
{
    throw "CUDA Runtime header missing after installation: $headerPath"
}

if (-not (Test-Path -Path $libraryPath -PathType Leaf))
{
    throw "CUDA Runtime library missing after installation: $libraryPath"
}

$dll = Get-ChildItem `
    -Path $dllDirectory `
    -Filter "cudart64_*.dll" `
    -File `
    -ErrorAction SilentlyContinue |
    Select-Object -First 1

if ($null -eq $dll)
{
    throw "CUDA Runtime DLL missing after installation."
}


#
# Success.
#

Write-Host "CUDA Runtime $($config.version) ready."
Write-Host "Location: $installDirectory"
Write-Host "DLL: $($dll.FullName)"