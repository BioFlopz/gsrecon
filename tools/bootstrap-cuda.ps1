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
    -ChildPath ".tools\_cache"

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
# CUDA NVCC
#

$nvcc = $tools.cudaNvcc

$nvccInstallDirectory = Join-Path `
    -Path $repoRoot `
    -ChildPath $nvcc.installDirectory

$nvccExecutable = Join-Path `
    -Path $nvccInstallDirectory `
    -ChildPath "bin\nvcc.exe"

if (Test-Path -Path $nvccExecutable -PathType Leaf)
{
    Write-Host "CUDA NVCC $($nvcc.version) already installed."
}
else
{
    if (Test-Path -Path $nvccInstallDirectory)
    {
        Write-Host "Removing incomplete CUDA NVCC installation..."

        Remove-Item `
            -Path $nvccInstallDirectory `
            -Recurse `
            -Force
    }

    $nvccArchive = Join-Path `
        -Path $cacheDirectory `
        -ChildPath $nvcc.archiveName

    if (-not (Test-Path -Path $nvccArchive -PathType Leaf))
    {
        Write-Host "Downloading CUDA NVCC $($nvcc.version)..."

        Invoke-WebRequest `
            -Uri $nvcc.downloadUrl `
            -OutFile $nvccArchive `
            -UseBasicParsing
    }

    $expectedHash =
        $nvcc.sha256.ToUpperInvariant()

    $actualHash = (
        Get-FileHash `
            -Path $nvccArchive `
            -Algorithm SHA256
    ).Hash.ToUpperInvariant()

    if ($actualHash -ne $expectedHash)
    {
        Remove-Item `
            -Path $nvccArchive `
            -Force

        throw "CUDA NVCC archive SHA-256 mismatch."
    }

    Write-Host "CUDA NVCC archive SHA-256 OK"

    $nvccTempDirectory = Join-Path `
        -Path $cacheDirectory `
        -ChildPath "cuda-nvcc-$($nvcc.version)-extract"

    if (Test-Path -Path $nvccTempDirectory)
    {
        Remove-Item `
            -Path $nvccTempDirectory `
            -Recurse `
            -Force
    }

    New-Item `
        -ItemType Directory `
        -Path $nvccTempDirectory `
        -Force |
        Out-Null

    Expand-Archive `
        -Path $nvccArchive `
        -DestinationPath $nvccTempDirectory

    $nvccInArchive = Get-ChildItem `
        -Path $nvccTempDirectory `
        -Filter "nvcc.exe" `
        -Recurse `
        -File |
        Select-Object -First 1

    if ($null -eq $nvccInArchive)
    {
        throw "nvcc.exe was not found in the CUDA NVCC archive."
    }

    $nvccBinDirectory = Split-Path `
        -Path $nvccInArchive.FullName `
        -Parent

    $nvccPackageRoot = Split-Path `
        -Path $nvccBinDirectory `
        -Parent

    New-Item `
        -ItemType Directory `
        -Path $nvccInstallDirectory `
        -Force |
        Out-Null

    Copy-Item `
        -Path (
            Join-Path `
                -Path $nvccPackageRoot `
                -ChildPath "*"
        ) `
        -Destination $nvccInstallDirectory `
        -Recurse `
        -Force

    Remove-Item `
        -Path $nvccTempDirectory `
        -Recurse `
        -Force

    if (-not (
        Test-Path `
            -Path $nvccExecutable `
            -PathType Leaf
    ))
    {
        throw "nvcc.exe missing after installation."
    }

    Write-Host "CUDA NVCC $($nvcc.version) ready."
}


$libNvvm = $tools.libNvvm

#
# libNVVM
#

$libNvvmInstallDirectory = Join-Path `
    -Path $repoRoot `
    -ChildPath $libNvvm.installDirectory

$cicc = Join-Path `
    -Path $libNvvmInstallDirectory `
    -ChildPath "bin\cicc.exe"

$libdeviceDirectory = Join-Path `
    -Path $libNvvmInstallDirectory `
    -ChildPath "libdevice"

$libdevice = Get-ChildItem `
    -Path $libdeviceDirectory `
    -Filter "libdevice*.bc" `
    -File `
    -ErrorAction SilentlyContinue |
    Select-Object -First 1

$libNvvmReady =
    (Test-Path -Path $cicc -PathType Leaf) -and
    ($null -ne $libdevice)

if ($libNvvmReady)
{
    Write-Host "CUDA libNVVM $($libNvvm.version) already installed."
}
else
{
    if (Test-Path -Path $libNvvmInstallDirectory)
    {
        Write-Host "Removing incomplete CUDA libNVVM installation..."

        Remove-Item `
            -Path $libNvvmInstallDirectory `
            -Recurse `
            -Force
    }

    $libNvvmArchive = Join-Path `
        -Path $cacheDirectory `
        -ChildPath $libNvvm.archiveName

    if (-not (Test-Path -Path $libNvvmArchive -PathType Leaf))
    {
        Write-Host "Downloading CUDA libNVVM $($libNvvm.version)..."

        Invoke-WebRequest `
            -Uri $libNvvm.downloadUrl `
            -OutFile $libNvvmArchive `
            -UseBasicParsing
    }

    $expectedHash =
        $libNvvm.sha256.ToUpperInvariant()

    $actualHash = (
        Get-FileHash `
            -Path $libNvvmArchive `
            -Algorithm SHA256
    ).Hash.ToUpperInvariant()

    if ($actualHash -ne $expectedHash)
    {
        Remove-Item `
            -Path $libNvvmArchive `
            -Force

        throw "CUDA libNVVM archive SHA-256 mismatch."
    }

    Write-Host "CUDA libNVVM archive SHA-256 OK"

    $libNvvmTempDirectory = Join-Path `
        -Path $cacheDirectory `
        -ChildPath "cuda-libnvvm-$($libNvvm.version)-extract"

    if (Test-Path -Path $libNvvmTempDirectory)
    {
        Remove-Item `
            -Path $libNvvmTempDirectory `
            -Recurse `
            -Force
    }

    New-Item `
        -ItemType Directory `
        -Path $libNvvmTempDirectory `
        -Force |
        Out-Null

    Expand-Archive `
        -Path $libNvvmArchive `
        -DestinationPath $libNvvmTempDirectory

    $ciccInArchive = Get-ChildItem `
        -Path $libNvvmTempDirectory `
        -Filter "cicc.exe" `
        -Recurse `
        -File |
        Select-Object -First 1

    if ($null -eq $ciccInArchive)
    {
        throw "cicc.exe was not found in the CUDA libNVVM archive."
    }

    $binDirectory = Split-Path `
        -Path $ciccInArchive.FullName `
        -Parent

    $libNvvmPackageRoot = Split-Path `
        -Path $binDirectory `
        -Parent

    New-Item `
        -ItemType Directory `
        -Path $libNvvmInstallDirectory `
        -Force |
        Out-Null

    Copy-Item `
        -Path (
            Join-Path `
                -Path $libNvvmPackageRoot `
                -ChildPath "*"
        ) `
        -Destination $libNvvmInstallDirectory `
        -Recurse `
        -Force

    Remove-Item `
        -Path $libNvvmTempDirectory `
        -Recurse `
        -Force

    $libdevice = Get-ChildItem `
        -Path $libdeviceDirectory `
        -Filter "libdevice*.bc" `
        -File `
        -ErrorAction SilentlyContinue |
        Select-Object -First 1

    if (-not (Test-Path -Path $cicc -PathType Leaf))
    {
        throw "CUDA libNVVM cicc.exe missing after installation."
    }

    if ($null -eq $libdevice)
    {
        throw "CUDA libdevice bitcode missing after installation."
    }

    Write-Host "CUDA libNVVM $($libNvvm.version) ready."
}

#
# Success
#

Write-Host ""
Write-Host "CUDA project-local foundation ready."
Write-Host "Runtime: $runtimeInstallDirectory"
Write-Host "CRT:     $crtInstallDirectory"
Write-Host "NVCC:    $nvccInstallDirectory"