param(
    [string]$FirmwareHeaderPath = "",
    [string]$OutputHeaderPath = ""
)

$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
    Write-Error $Message
    exit 1
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($FirmwareHeaderPath)) {
    $FirmwareHeaderPath = Join-Path (Join-Path $scriptDir "..") "demo_code\\M031BSP_USB_HID_PMBus_SMBus\\SampleCode\\Template\\bridge_version.h"
}
if ([string]::IsNullOrWhiteSpace($OutputHeaderPath)) {
    $OutputHeaderPath = Join-Path (Join-Path $scriptDir "..") "src\\core\\fw_version.generated.h"
}

if (-not (Test-Path -LiteralPath $FirmwareHeaderPath)) {
    Fail "Firmware version header not found: $FirmwareHeaderPath"
}

$raw = [System.IO.File]::ReadAllText($FirmwareHeaderPath)
$m = [System.Text.RegularExpressions.Regex]::Match(
    $raw,
    '(?m)^\s*#define\s+M032_BRIDGE_FW_VERSION_STR\s+"([0-9]+\.[0-9]+\.[0-9]+)"\s*$')
if (-not $m.Success) {
    Fail "M032_BRIDGE_FW_VERSION_STR not found in $FirmwareHeaderPath"
}

$version = $m.Groups[1].Value

$outDir = Split-Path -Parent $OutputHeaderPath
if (-not [string]::IsNullOrWhiteSpace($outDir)) {
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
}

$header = @"
#pragma once

#define M032_EXPECTED_FW_VERSION L"$version"
#define M032_EXPECTED_FW_VERSION_A "$version"
"@

[System.IO.File]::WriteAllText($OutputHeaderPath, $header, [System.Text.Encoding]::ASCII)
Write-Output "[OK] Synced firmware expected version to $version -> $OutputHeaderPath"
