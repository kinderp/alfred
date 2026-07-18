[CmdletBinding()]
param(
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Fail {
    param(
        [string]$Message,
        [int]$Code
    )

    [Console]::Error.WriteLine($Message)
    exit $Code
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$profileDir = Join-Path $scriptDir "sol-ultra"

if ($env:CODEX_HOME) {
    $codexHome = $env:CODEX_HOME
} else {
    if (-not $env:USERPROFILE) {
        Fail "USERPROFILE is not set and CODEX_HOME was not provided." 1
    }
    $codexHome = Join-Path $env:USERPROFILE ".codex"
}

$agentsDir = Join-Path $codexHome "agents"
$backupDir = Join-Path $codexHome "backups"
$targetConfig = Join-Path $codexHome "config.toml"
$sourceConfig = Join-Path $profileDir "config.toml"
$sourceAgentsDir = Join-Path $profileDir "agents"

if (-not (Test-Path -LiteralPath $sourceConfig -PathType Leaf)) {
    Fail "Missing source config: $sourceConfig" 1
}

if ((Test-Path -LiteralPath $targetConfig -PathType Leaf) -and (-not $Force)) {
    Fail "Existing Codex config found: $targetConfig. Re-run with -Force to back it up and install the Sol Ultra profile." 2
}

New-Item -ItemType Directory -Force -Path $codexHome, $agentsDir, $backupDir | Out-Null

if (Test-Path -LiteralPath $targetConfig -PathType Leaf) {
    $stamp = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
    $backupPath = Join-Path $backupDir "config.toml.before-sol-ultra-$stamp"
    Copy-Item -LiteralPath $targetConfig -Destination $backupPath -Force
    Write-Host "Backed up existing config to: $backupPath"
}

Copy-Item -LiteralPath $sourceConfig -Destination $targetConfig -Force

Get-ChildItem -LiteralPath $sourceAgentsDir -Filter "*.toml" -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $agentsDir $_.Name) -Force
}

Write-Host "Installed Codex Sol Ultra profile in: $codexHome"
Write-Host "Restart Codex, then validate with:"
Write-Host "  codex --strict-config --help"
Write-Host "  codex features list"
