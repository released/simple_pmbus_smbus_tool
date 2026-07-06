param(
    [string]$ContractPath = "docs\PMBUS_COMMAND_CONTRACT.csv",
    [string]$OutputPath = "src\core\pmbus_contract.generated.inc"
)

$ErrorActionPreference = "Stop"

function Fail($Message) {
    Write-Error $Message
    exit 1
}

function Escape-CppWideString($Text) {
    return ($Text -replace '\\', '\\' -replace '"', '\"')
}

function TxnEnum($Name) {
    switch ($Name) {
        "SendByte" { return "PmbusTransactionType::SendByte" }
        "ReceiveByte" { return "PmbusTransactionType::ReceiveByte" }
        "WriteByte" { return "PmbusTransactionType::WriteByte" }
        "WriteWord" { return "PmbusTransactionType::WriteWord" }
        "ReadByte" { return "PmbusTransactionType::ReadByte" }
        "ReadWord" { return "PmbusTransactionType::ReadWord" }
        "Read32" { return "PmbusTransactionType::Read32" }
        "BlockWrite" { return "PmbusTransactionType::BlockWrite" }
        "BlockRead" { return "PmbusTransactionType::BlockRead" }
        "ProcessCall" { return "PmbusTransactionType::ProcessCall" }
        "BlockWriteReadProcessCall" { return "PmbusTransactionType::BlockWriteReadProcessCall" }
        default { Fail "Unknown transaction enum: $Name" }
    }
}

function FormatEnum($Name) {
    switch ($Name) {
        "None" { return "PmbusDataFormat::None" }
        "RawByte" { return "PmbusDataFormat::RawByte" }
        "RawWord" { return "PmbusDataFormat::RawWord" }
        "RawDword" { return "PmbusDataFormat::RawDword" }
        "Linear11" { return "PmbusDataFormat::Linear11" }
        "Linear16Vout" { return "PmbusDataFormat::Linear16Vout" }
        "VoutModeAwareWord" { return "PmbusDataFormat::VoutModeAwareWord" }
        "BlockAscii" { return "PmbusDataFormat::BlockAscii" }
        "StatusByte" { return "PmbusDataFormat::StatusByte" }
        "StatusWord" { return "PmbusDataFormat::StatusWord" }
        "Capability" { return "PmbusDataFormat::Capability" }
        "QueryResult" { return "PmbusDataFormat::QueryResult" }
        "PmbusRevision" { return "PmbusDataFormat::PmbusRevision" }
        "Percent0p1" { return "PmbusDataFormat::Percent0p1" }
        "AppProfileSupport" { return "PmbusDataFormat::AppProfileSupport" }
        "RawBlock" { return "PmbusDataFormat::RawBlock" }
        default { Fail "Unknown decode format: $Name" }
    }
}

$maskBits = @{
    "Send Byte" = 0x00000001
    "Receive Byte" = 0x00000002
    "Write Byte" = 0x00000004
    "Write Word" = 0x00000008
    "Read Byte" = 0x00000010
    "Read Word" = 0x00000020
    "Read 32" = 0x00000040
    "Block Write" = 0x00000080
    "Block Read" = 0x00000100
    "Process Call" = 0x00000200
    "Block Write-Read Process Call" = 0x00000400
}

function Add-MaskFromTableCell($CurrentMask, $Cell, $Code) {
    $value = $Cell.Trim()
    if (($value.Length -eq 0) -or ($value -eq "N/A")) {
        return $CurrentMask
    }
    if ($value -eq "Mfr Defined") {
        if ((($Code -ge 0xB0) -and ($Code -le 0xBF)) -or (($Code -ge 0xC4) -and ($Code -le 0xFD))) {
            return ($CurrentMask -bor $maskBits["Block Write"] -bor $maskBits["Block Read"])
        }
        return $CurrentMask
    }
    if ($value -eq "Extended Command") {
        return $CurrentMask
    }
    if (-not $maskBits.ContainsKey($value)) {
        Fail "Unsupported Table31 protocol cell '$value' for 0x$($Code.ToString('X2'))"
    }
    return ($CurrentMask -bor $maskBits[$value])
}

if (-not (Test-Path -LiteralPath $ContractPath)) {
    Fail "PMBus contract not found: $ContractPath"
}

$rows = Import-Csv -LiteralPath $ContractPath
if ($rows.Count -ne 224) {
    Fail "Expected 224 PMBus Table 31 non-reserved rows, got $($rows.Count)."
}

$entries = @()
foreach ($row in $rows) {
    if ($row.gui_status -eq "ExtendedMode") {
        continue
    }

    $code = [Convert]::ToInt32($row.code_hex, 16)
    $mask = 0
    $mask = Add-MaskFromTableCell $mask $row.table31_write $code
    $mask = Add-MaskFromTableCell $mask $row.table31_read $code

    $entries += [pscustomobject]@{
        Code = $code
        Name = $row.gui_name
        Txn = (TxnEnum $row.gui_preferred_protocol)
        Format = (FormatEnum $row.gui_decode_format)
        Mask = $mask
    }
}

$outDir = Split-Path -Parent $OutputPath
if (($outDir -ne $null) -and ($outDir.Length -gt 0) -and (-not (Test-Path -LiteralPath $outDir))) {
    New-Item -ItemType Directory -Path $outDir | Out-Null
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("// Auto-generated from docs/PMBUS_COMMAND_CONTRACT.csv. Do not edit by hand.")
$lines.Add("// Regenerate with scripts/generate_pmbus_contract_header.ps1.")
$lines.Add("static const PmbusContractPresetEntry kPmbusContractPresetEntries[] = {")
foreach ($entry in $entries) {
    $name = Escape-CppWideString $entry.Name
    $lines.Add(("    {{0x{0}u, L""{1}"", {2}, {3}, 0x{4}u}}," -f $entry.Code.ToString("X2"), $name, $entry.Txn, $entry.Format, $entry.Mask.ToString("X8")))
}
$lines.Add("};")
$lines.Add("")
$lines.Add("static const size_t kPmbusContractPresetEntryCount =")
$lines.Add("    sizeof(kPmbusContractPresetEntries) / sizeof(kPmbusContractPresetEntries[0]);")

Set-Content -LiteralPath $OutputPath -Value $lines -Encoding ASCII
Write-Host "Generated $OutputPath from $ContractPath ($($entries.Count) GUI entries)."
