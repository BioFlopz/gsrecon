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

$runtime = $tools.cudaRuntime
$crt = $tools.cudaCrt

$cacheDirectory = Join-Path `
    -Path $repoRoot `
    -ChildPath ".tools\cache"

New-Item `
    -ItemType Directory `
    -Path $cacheDirectory `
    -Force |
    Out-Null


#
# CUDA Runtime
#

$runtimeInstallDirectory = Join-Path `
    -Path $repoRoot `
    -ChildPath $runtime.installDirectory

$runtimeHeader = Join-Path `
    -Path $runtimeInstallDirectory `
    -ChildPath "include\cuda_runtime_api.h"

$runtimeLibrary = Join-Path `
    -Path $runtimeInstallDirectory `
    -ChildPath "lib\x64\cudart.lib"

$runtimeDllDirectory = Join-Path `
    -Path $runtimeInstallDirectory `
    -ChildPath "bin\x64"

$runtimeDll = Get-ChildItem `
    -Path $runtimeDllDirectory `
    -Filter "cudart64_*.dll" `
    -File `
    -ErrorAction SilentlyContinue |
    Select-Object -First 1

$runtimeReady =
    (Test-Path -Path $runtimeHeader -PathType Leaf) -and
    (Test-Path -Path $runtimeLibrary -PathType Leaf) -and
    ($null -ne $runtimeDll)

if ($runtimeReady)
{
    Write-Host "CUDA Runtime $($runtime.version) already installed."
}
else
{
    if (Test-Path -Path $runtimeInstallDirectory)
    {
        Write-Host "Removing incomplete CUDA Runtime installation..."

        Remove-Item `
            -Path $runtimeInstallDirectory `
            -Recurse `
            -Force
    }

    $runtimeArchive = Join-Path `
        -Path $cacheDirectory `
        -ChildPath $runtime.archiveName

    if (-not (Test-Path -Path $runtimeArchive -PathType Leaf))
    {
        Write-Host "Downloading CUDA Runtime $($runtime.version)..."

        Invoke-WebRequest `
            -Uri $runtime.downloadUrl `
            -OutFile $runtimeArchive `
            -UseBasicParsing
    }

    $expectedHash =
        $runtime.sha256.ToUpperInvariant()

    $actualHash = (
        Get-FileHash `
            -Path $runtimeArchive `
            -Algorithm SHA256
    ).Hash.ToUpperInvariant()

    if ($actualHash -ne $expectedHash)
    {
        Remove-Item `
            -Path $runtimeArchive `
            -Force

        throw "CUDA Runtime archive SHA-256 mismatch."
    }

    Write-Host "CUDA Runtime archive SHA-256 OK"

    $runtimeTempDirectory = Join-Path `
        -Path $cacheDirectory `
        -ChildPath "cuda-cudart-$($runtime.version)-extract"

    if (Test-Path -Path $runtimeTempDirectory)
    {
        Remove-Item `
            -Path $runtimeTempDirectory `
            -Recurse `
            -Force
    }

    New-Item `
        -ItemType Directory `
        -Path $runtimeTempDirectory `
        -Force |
        Out-Null

    Expand-Archive `
        -Path $runtimeArchive `
        -DestinationPath $runtimeTempDirectory

    $runtimeHeaderInArchive = Get-ChildItem `
        -Path $runtimeTempDirectory `
        -Filter "cuda_runtime_api.h" `
        -Recurse `
        -File |
        Select-Object -First 1

    if ($null -eq $runtimeHeaderInArchive)
    {
        throw "cuda_runtime_api.h was not found in the CUDA Runtime archive."
    }

    $runtimeIncludeDirectory = Split-Path `
        -Path $runtimeHeaderInArchive.FullName `
        -Parent

    $runtimePackageRoot = Split-Path `
        -Path $runtimeIncludeDirectory `
        -Parent

    $runtimeLibraryInArchive = Join-Path `
        -Path $runtimePackageRoot `
        -ChildPath "lib\x64\cudart.lib"

    $runtimeDllInArchive = Get-ChildItem `
        -Path (
            Join-Path `
                -Path $runtimePackageRoot `
                -ChildPath "bin\x64"
        ) `
        -Filter "cudart64_*.dll" `
        -File `
        -ErrorAction SilentlyContinue |
        Select-Object -First 1

    if (-not (
        Test-Path `
            -Path $runtimeLibraryInArchive `
            -PathType Leaf
    ))
    {
        throw "cudart.lib was not found in the CUDA Runtime archive."
    }

    if ($null -eq $runtimeDllInArchive)
    {
        throw "CUDA Runtime DLL was not found in the archive."
    }

    New-Item `
        -ItemType Directory `
        -Path $runtimeInstallDirectory `
        -Force |
        Out-Null

    Copy-Item `
        -Path (
            Join-Path `
                -Path $runtimePackageRoot `
                -ChildPath "*"
        ) `
        -Destination $runtimeInstallDirectory `
        -Recurse `
        -Force

    Remove-Item `
        -Path $runtimeTempDirectory `
        -Recurse `
        -Force

    if (-not (
        Test-Path `
            -Path $runtimeHeader `
            -PathType Leaf
    ))
    {
        throw "CUDA Runtime header missing after installation."
    }

    if (-not (
        Test-Path `
            -Path $runtimeLibrary `
            -PathType Leaf
    ))
    {
        throw "CUDA Runtime library missing after installation."
    }

    $runtimeDll = Get-ChildItem `
        -Path $runtimeDllDirectory `
        -Filter "cudart64_*.dll" `
        -File `
        -ErrorAction SilentlyContinue |
        Select-Object -First 1

    if ($null -eq $runtimeDll)
    {
        throw "CUDA Runtime DLL missing after installation."
    }

    Write-Host "CUDA Runtime $($runtime.version) ready."
}


#
# CUDA CRT
#

$crtInstallDirectory = Join-Path `
    -Path $repoRoot `
    -ChildPath $crt.installDirectory

$crtHeader = Join-Path `
    -Path $crtInstallDirectory `
    -ChildPath "include\crt\host_defines.h"

if (Test-Path -Path $crtHeader -PathType Leaf)
{
    Write-Host "CUDA CRT $($crt.version) already installed."
}
else
{
    if (Test-Path -Path $crtInstallDirectory)
    {
        Write-Host "Removing incomplete CUDA CRT installation..."

        Remove-Item `
            -Path $crtInstallDirectory `
            -Recurse `
            -Force
    }

    $crtArchive = Join-Path `
        -Path $cacheDirectory `
        -ChildPath $crt.archiveName

    if (-not (Test-Path -Path $crtArchive -PathType Leaf))
    {
        Write-Host "Downloading CUDA CRT $($crt.version)..."

        Invoke-WebRequest `
            -Uri $crt.downloadUrl `
            -OutFile $crtArchive `
            -UseBasicParsing
    }

    $expectedHash =
        $crt.sha256.ToUpperInvariant()

    $actualHash = (
        Get-FileHash `
            -Path $crtArchive `
            -Algorithm SHA256
    ).Hash.ToUpperInvariant()

    if ($actualHash -ne $expectedHash)
    {
        Remove-Item `
            -Path $crtArchive `
            -Force

        throw "CUDA CRT archive SHA-256 mismatch."
    }

    Write-Host "CUDA CRT archive SHA-256 OK"

    $crtTempDirectory = Join-Path `
        -Path $cacheDirectory `
        -ChildPath "cuda-crt-$($crt.version)-extract"

    if (Test-Path -Path $crtTempDirectory)
    {
        Remove-Item `
            -Path $crtTempDirectory `
            -Recurse `
            -Force
    }

    New-Item `
        -ItemType Directory `
        -Path $crtTempDirectory `
        -Force |
        Out-Null

    Expand-Archive `
        -Path $crtArchive `
        -DestinationPath $crtTempDirectory

    $crtHeaderInArchive = Get-ChildItem `
        -Path $crtTempDirectory `
        -Filter "host_defines.h" `
        -Recurse `
        -File |
        Where-Object {
            $_.FullName -match `
                "[\\/]include[\\/]crt[\\/]host_defines\.h$"
        } |
        Select-Object -First 1

    if ($null -eq $crtHeaderInArchive)
    {
        throw "crt/host_defines.h was not found in the CUDA CRT archive."
    }

    $crtDirectory = Split-Path `
        -Path $crtHeaderInArchive.FullName `
        -Parent

    $crtIncludeDirectory = Split-Path `
        -Path $crtDirectory `
        -Parent

    $crtPackageRoot = Split-Path `
        -Path $crtIncludeDirectory `
        -Parent

    New-Item `
        -ItemType Directory `
        -Path $crtInstallDirectory `
        -Force |
        Out-Null

    Copy-Item `
        -Path (
            Join-Path `
                -Path $crtPackageRoot `
                -ChildPath "*"
        ) `
        -Destination $crtInstallDirectory `
        -Recurse `
        -Force

    Remove-Item `
        -Path $crtTempDirectory `
        -Recurse `
        -Force

    if (-not (
        Test-Path `
            -Path $crtHeader `
            -PathType Leaf
    ))
    {
        throw "CUDA CRT header missing after installation: $crtHeader"
    }

    Write-Host "CUDA CRT $($crt.version) ready."
}


#
# Success
#

Write-Host ""
Write-Host "CUDA project-local foundation ready."
Write-Host "Runtime: $runtimeInstallDirectory"
Write-Host "CRT:     $crtInstallDirectory"