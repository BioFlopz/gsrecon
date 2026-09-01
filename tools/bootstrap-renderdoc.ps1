$ErrorActionPreference = "Stop"

$repoRoot = (
    Resolve-Path -Path (
        Join-Path -Path $PSScriptRoot -ChildPath ".."
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
$expectedVersion = "v$($renderDoc.version)"

$cacheDirectory = Join-Path `
    -Path $repoRoot `
    -ChildPath ".tools\_cache"

$archivePath = Join-Path `
    -Path $cacheDirectory `
    -ChildPath $archiveName

$installDirectory = Join-Path `
    -Path $repoRoot `
    -ChildPath $renderDoc.installDirectory

$renderDocCmd = Join-Path `
    -Path $installDirectory `
    -ChildPath "renderdoccmd.exe"


#
# Check an existing installation.
#

if (Test-Path -Path $renderDocCmd -PathType Leaf)
{
    $output = & $renderDocCmd version

    if ($LASTEXITCODE -eq 0)
    {
        $outputText = $output -join [Environment]::NewLine

        if (
            ($outputText -match [regex]::Escape($expectedVersion)) -and
            ($outputText -match "x64")
        )
        {
            Write-Host "RenderDoc $($renderDoc.version) already installed."
            Write-Host "Location: $installDirectory"
            exit 0
        }
    }

    Write-Host "Existing RenderDoc installation is invalid. Reinstalling..."

    Remove-Item `
        -Path $installDirectory `
        -Recurse `
        -Force
}
elseif (Test-Path -Path $installDirectory)
{
    Write-Host "Existing RenderDoc installation is incomplete. Reinstalling..."

    Remove-Item `
        -Path $installDirectory `
        -Recurse `
        -Force
}


#
# Prepare the download cache.
#

New-Item `
    -ItemType Directory `
    -Path $cacheDirectory `
    -Force |
    Out-Null


#
# Download the pinned RenderDoc archive if needed.
#

if (-not (Test-Path -Path $archivePath -PathType Leaf))
{
    Write-Host "Finding $archiveName on the official RenderDoc builds page..."

    $buildsPage = Invoke-WebRequest `
        -Uri $renderDoc.buildsUrl `
        -UseBasicParsing

    $escapedArchiveName = [regex]::Escape($archiveName)

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


#
# Verify the downloaded archive.
#

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


#
# Extract into a temporary directory.
#

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


#
# Locate the RenderDoc package inside the archive.
#

$renderDocCmdInArchive = Get-ChildItem `
    -Path $tempDirectory `
    -Filter "renderdoccmd.exe" `
    -Recurse `
    -File |
    Select-Object -First 1

if ($null -eq $renderDocCmdInArchive)
{
    Remove-Item `
        -Path $tempDirectory `
        -Recurse `
        -Force

    Write-Error "renderdoccmd.exe was not found in the archive."
    exit 1
}

$packageRoot = Split-Path `
    -Path $renderDocCmdInArchive.FullName `
    -Parent


#
# Install project-local RenderDoc.
#

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


#
# Verify the completed installation.
#

if (-not (Test-Path -Path $renderDocCmd -PathType Leaf))
{
    throw "renderdoccmd.exe is missing after installation: $renderDocCmd"
}

$output = & $renderDocCmd version

if ($LASTEXITCODE -ne 0)
{
    throw "renderdoccmd version failed after installation."
}

$outputText = $output -join [Environment]::NewLine

if ($outputText -notmatch [regex]::Escape($expectedVersion))
{
    throw "Installed RenderDoc version does not match $expectedVersion."
}

if ($outputText -notmatch "x64")
{
    throw "Installed RenderDoc is not x64."
}

Write-Host "RenderDoc $($renderDoc.version) ready."
Write-Host "Location: $installDirectory"