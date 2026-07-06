param(
    [string]$RcPath = "",
    [string]$HeaderPath = "",
    [string]$FirmwareHeaderPath = "",
    [string]$FwSyncHeaderPath = "",
    [int]$MaxRevision = 999,
    [int]$MaxPatch = 9,
    [int]$MaxMinor = 9,
    [int]$MaxMajor = 9
)

$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
    Write-Error $Message
    exit 1
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($RcPath)) {
    $RcPath = Join-Path (Join-Path $scriptDir "..") "src\\pmbus_smbus_tool.rc"
}
if ([string]::IsNullOrWhiteSpace($HeaderPath)) {
    $HeaderPath = Join-Path (Join-Path $scriptDir "..") "src\\build_info.generated.h"
}
if ([string]::IsNullOrWhiteSpace($FirmwareHeaderPath)) {
    $FirmwareHeaderPath = ""
}
if ([string]::IsNullOrWhiteSpace($FwSyncHeaderPath)) {
    $FwSyncHeaderPath = Join-Path (Join-Path $scriptDir "..") "src\\core\\fw_version.generated.h"
}

if (-not (Test-Path -LiteralPath $RcPath)) {
    Fail "RC file not found: $RcPath"
}

$raw = [System.IO.File]::ReadAllText($RcPath)

$m = [System.Text.RegularExpressions.Regex]::Match(
    $raw,
    '(?m)^\s*FILEVERSION\s+(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*$')
if (-not $m.Success) {
    Fail "FILEVERSION line not found in RC."
}

$major = [int]$m.Groups[1].Value
$minor = [int]$m.Groups[2].Value
$patch = [int]$m.Groups[3].Value
$rev = [int]$m.Groups[4].Value

$rev = $rev + 1
if ($rev -gt $MaxRevision) {
    $rev = 1
    $patch = $patch + 1
}
if ($patch -gt $MaxPatch) {
    $patch = 0
    $minor = $minor + 1
}
if ($minor -gt $MaxMinor) {
    $minor = 0
    $major = $major + 1
}
if ($major -gt $MaxMajor) {
    Fail "Major overflow: $major"
}

$numeric = "$major,$minor,$patch,$rev"
$text = "{0}.{1}.{2}.{3:D3}" -f $major, $minor, $patch, $rev
$buildTime = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")

$updated = $raw
$updated = [System.Text.RegularExpressions.Regex]::Replace(
    $updated,
    '(?m)^\s*FILEVERSION\s+\d+\s*,\s*\d+\s*,\s*\d+\s*,\s*\d+\s*$',
    " FILEVERSION $numeric",
    1)
$updated = [System.Text.RegularExpressions.Regex]::Replace(
    $updated,
    '(?m)^\s*PRODUCTVERSION\s+\d+\s*,\s*\d+\s*,\s*\d+\s*,\s*\d+\s*$',
    " PRODUCTVERSION $numeric",
    1)
$updated = [System.Text.RegularExpressions.Regex]::Replace(
    $updated,
    '(?m)^\s*VALUE\s+"FileVersion",\s*"\d+\.\d+\.\d+\.\d{3}\\0"\s*$',
    "            VALUE `"FileVersion`", `"$text\0`"",
    1)
$updated = [System.Text.RegularExpressions.Regex]::Replace(
    $updated,
    '(?m)^\s*VALUE\s+"ProductVersion",\s*"\d+\.\d+\.\d+\.\d{3}\\0"\s*$',
    "            VALUE `"ProductVersion`", `"$text\0`"",
    1)

[System.IO.File]::WriteAllText($RcPath, $updated, [System.Text.Encoding]::ASCII)

$header = @"
#pragma once

#define PMBUS_TOOL_BUILD_VERSION L"$text"
#define PMBUS_TOOL_BUILD_TIME L"$buildTime"
"@
[System.IO.File]::WriteAllText($HeaderPath, $header, [System.Text.Encoding]::ASCII)

if (-not [string]::IsNullOrWhiteSpace($FirmwareHeaderPath)) {
    $syncScript = Join-Path $scriptDir "sync_fw_version.ps1"
    if (-not (Test-Path -LiteralPath $syncScript)) {
        Fail "Firmware sync script not found: $syncScript"
    }
    & $syncScript -FirmwareHeaderPath $FirmwareHeaderPath -OutputHeaderPath $FwSyncHeaderPath
    if (-not $?) {
        Fail "Firmware version sync failed."
    }
}

Write-Output "[OK] Bumped version to $text at $buildTime"
