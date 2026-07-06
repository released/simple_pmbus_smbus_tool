param(
    [string]$ContractPath = "docs\PMBUS_COMMAND_CONTRACT.csv",
    [string]$GeneratedIncludePath = "src\core\pmbus_contract.generated.inc"
)

$ErrorActionPreference = "Stop"

function Fail($Message) {
    Write-Error $Message
    exit 1
}

if (-not (Test-Path -LiteralPath $ContractPath)) {
    Fail "PMBus contract not found: $ContractPath"
}

if (-not (Test-Path -LiteralPath $GeneratedIncludePath)) {
    Fail "Generated PMBus GUI contract include not found: $GeneratedIncludePath"
}

$rows = Import-Csv -LiteralPath $ContractPath
if ($rows.Count -ne 224) {
    Fail "Expected 224 PMBus Table 31 non-reserved rows, got $($rows.Count)."
}

$requiredColumns = @(
    "code_hex",
    "spec_name",
    "gui_status",
    "gui_name",
    "gui_preferred_protocol",
    "gui_decode_format",
    "bridge_transport_status",
    "device_model_status",
    "device_model_name",
    "device_model_note",
    "product_binding_status",
    "validation_status",
    "notes"
)

$actualColumns = @($rows[0].PSObject.Properties.Name)
foreach ($column in $requiredColumns) {
    if ($actualColumns -notcontains $column) {
        Fail "Missing PMBus contract column: $column"
    }
}

$duplicateCodes = $rows |
    Group-Object -Property code_hex |
    Where-Object { $_.Count -gt 1 } |
    Select-Object -ExpandProperty Name
if ($duplicateCodes.Count -gt 0) {
    Fail "Duplicate command code(s) in contract: $($duplicateCodes -join ', ')"
}

$reservedCodes = @(
    "0x09","0x0A","0x0B","0x0C","0x0D","0x0E","0x0F",
    "0x1C","0x1D","0x1E","0x1F",
    "0x2C","0x2D","0x2E","0x2F",
    "0x4D","0x4E",
    "0x67",
    "0x6C","0x6D","0x6E","0x6F","0x70","0x71","0x72","0x73","0x74","0x75","0x76","0x77",
    "0xAF","0xC3"
)
$reservedPresent = $rows | Where-Object { $reservedCodes -contains $_.code_hex }
if ($reservedPresent.Count -gt 0) {
    Fail "Reserved command code(s) present in contract: $($reservedPresent.code_hex -join ', ')"
}

$unexpectedGap = $rows | Where-Object { $_.device_model_status -eq "Gap" }
if ($unexpectedGap.Count -gt 0) {
    Fail "Non-policy target command gap(s) exist: $($unexpectedGap.code_hex -join ', ')"
}

$policyGap = @($rows | Where-Object { $_.device_model_status -eq "PolicyGap" })
if ($policyGap.Count -gt 0) {
    Fail "Target policy gap(s) exist: $($policyGap.code_hex -join ', ')"
}

$transportGaps = @($rows | Where-Object { $_.bridge_transport_status -ne "TransportReady" })
if ($transportGaps.Count -gt 0) {
    Fail "Bridge transport gap(s) exist: $($transportGaps.code_hex -join ', ')"
}

$shadow = @($rows | Where-Object { $_.product_binding_status -eq "HostVisibleShadowOrPlaceholder" })
$concreteBehavior = @($rows | Where-Object { $_.product_binding_status -eq "ImplementedBehavior" })
$guiMismatch = @($rows | Where-Object { $_.notes -like "GUI name differs:*" })

$tempInclude = [System.IO.Path]::GetTempFileName()
try {
    & powershell -ExecutionPolicy Bypass -File "scripts\generate_pmbus_contract_header.ps1" -ContractPath $ContractPath -OutputPath $tempInclude | Out-Null
    $expectedInclude = Get-Content -LiteralPath $tempInclude -Raw
    $actualInclude = Get-Content -LiteralPath $GeneratedIncludePath -Raw
    if ($expectedInclude -ne $actualInclude) {
        Fail "Generated PMBus GUI include is stale. Run scripts\generate_pmbus_contract_header.ps1."
    }
} finally {
    if (Test-Path -LiteralPath $tempInclude) {
        Remove-Item -LiteralPath $tempInclude -Force
    }
}

Write-Host "PMBus contract validation PASS"
Write-Host "Rows: $($rows.Count)"
Write-Host "Bridge transport ready: $($rows.Count - $transportGaps.Count)"
Write-Host "Target policy gaps: $($policyGap.Count)"
Write-Host "Host-visible shadows/placeholders: $($shadow.Count)"
Write-Host "Concrete/non-placeholder behavior rows: $($concreteBehavior.Count)"
Write-Host "GUI/spec name mismatch warnings: $($guiMismatch.Count)"

if ($guiMismatch.Count -gt 0) {
    Write-Host "Warning: GUI/spec name mismatches exist. Review docs\PMBUS_COMMAND_CONTRACT.csv before claiming UI naming is fully spec-aligned."
}
