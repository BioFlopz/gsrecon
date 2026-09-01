$ErrorActionPreference = "Stop"

$repoRoot = (
    Resolve-Path -Path (
        Join-Path -Path $PSScriptRoot -ChildPath "..\.."
    )
).Path

$versionsFile = Join-Path `
    -Path $PSScriptRoot `
    -ChildPath "tool-versions.json"

$config = Get-Content `
    -Path $versionsFile `
    -Raw |
    ConvertFrom-Json

$renderDoc = $config.renderdoc

$archiveName = $renderDoc.archiveName
$expectedHash = $renderDoc.sha256.ToUpperInvariant()

$cacheDirectory = Join-Path `
    -Path $repoRoot `
    -ChildPath ".tools\cache"

$archivePath = Join-Path `
    -Path $cacheDirectory `
    -ChildPath $archiveName

$installDirectory = Join-Path `
    -Path $repoRoot `
    -ChildPath $renderDoc.installDirectory

$checkScript = Join-Path `
    -Path $PSScriptRoot `
    -ChildPath "check-renderdoc.ps1"

# Already bootstrapped: just verify it.
if (Test-Path -Path $installDirectory)
{
    & $checkScript
    exit $LASTEXITCODE
}

New-Item `
    -ItemType Directory `
    -Path $cacheDirectory `
    -Force |
    Out-Null

# Download the exact archive if the cache is empty.
if (-not (Test-Path -Path $archivePath -PathType Leaf))
{
    Write-Host "Finding $archiveName on the official RenderDoc builds page..."

    $buildsPage = Invoke-WebRequest `
        -Uri $renderDoc.buildsUrl `
        -UseBasicParsing

    $escapedArchiveName =
        [regex]::Escape($archiveName)

    $pattern =
        "href\s*=\s*[""']([^""']*$escapedArchiveName[^""']*)[""']"

    $match = [regex]::Match(
        $buildsPage.Content,
        $pattern,
        [System.Text.RegularExpressions.RegexOptions]::IgnoreCase
    )

    if (-not $match.Success)
    {
        Write-Error "Could not find $archiveName on $($renderDoc.buildsUrl)"
        exit 1
    }

    $href = $match.Groups[1].Value

    $downloadUrl = (
        New-Object System.Uri(
            (New-Object System.Uri($renderDoc.buildsUrl)),
            $href
        )
    ).AbsoluteUri

    Write-Host "Downloading:"
    Write-Host $downloadUrl

    Invoke-WebRequest `
        -Uri $downloadUrl `
        -OutFile $archivePath `
        -UseBasicParsing
}

# Verify the archive before extracting anything.
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

    Write-Error @"
RenderDoc archive SHA-256 mismatch.

Expected:
$expectedHash

Actual:
$actualHash

The downloaded archive was deleted.
"@

    exit 1
}

Write-Host "RenderDoc archive SHA-256 OK"

$tempDirectory = Join-Path `
    -Path $cacheDirectory `
    -ChildPath "renderdoc-$($renderDoc.version)-extract"

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

$renderDocCmd = Get-ChildItem `
    -Path $tempDirectory `
    -Filter "renderdoccmd.exe" `
    -Recurse `
    -File |
    Select-Object -First 1

if ($null -eq $renderDocCmd)
{
    Remove-Item `
        -Path $tempDirectory `
        -Recurse `
        -Force

    Write-Error "renderdoccmd.exe was not found in the archive."
    exit 1
}

$packageRoot = Split-Path `
    -Path $renderDocCmd.FullName `
    -Parent

New-Item `
    -ItemType Directory `
    -Path $installDirectory `
    -Force |
    Out-Null

Copy-Item `
    -Path (
        Join-Path -Path $packageRoot -ChildPath "*"
    ) `
    -Destination $installDirectory `
    -Recurse

Remove-Item `
    -Path $tempDirectory `
    -Recurse `
    -Force

& $checkScript
exit $LASTEXITCODE