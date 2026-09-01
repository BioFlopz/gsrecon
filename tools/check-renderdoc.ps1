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

$installDirectory = Join-Path `
    -Path $repoRoot `
    -ChildPath $renderDoc.installDirectory

$renderDocCmd = Join-Path `
    -Path $installDirectory `
    -ChildPath "renderdoccmd.exe"

if (-not (Test-Path -Path $renderDocCmd -PathType Leaf))
{
    Write-Error "RenderDoc not found at: $renderDocCmd"
    exit 1
}

$output = & $renderDocCmd version

if ($LASTEXITCODE -ne 0)
{
    Write-Error "renderdoccmd version failed."
    exit 1
}

$outputText = $output -join [Environment]::NewLine

$expectedVersion = "v$($renderDoc.version)"

if ($outputText -notmatch [regex]::Escape($expectedVersion))
{
    Write-Error "Expected RenderDoc $expectedVersion, but got: $outputText"
    exit 1
}

if ($outputText -notmatch "x64")
{
    Write-Error "Expected x64 RenderDoc, but got: $outputText"
    exit 1
}

Write-Host "RenderDoc OK"
Write-Host $outputText