param(
    [string]$OutputDir = "build\script_presets",
    [string]$Address = "0x5A",
    [string]$DelayMs = "10"
)

$ErrorActionPreference = "Stop"

$Columns = @(
    "Kind",
    "Selected",
    "Profile",
    "Type",
    "Address",
    "Command",
    "Data",
    "ReadLength",
    "DelayMs",
    "PEC",
    "Comment"
)

function New-RowList {
    return New-Object System.Collections.ArrayList
}

function New-BaseRow {
    param(
        [string]$Kind,
        [string]$Selected = "",
        [string]$Profile = "",
        [string]$Type = "",
        [string]$Address = "",
        [string]$Command = "",
        [string]$Data = "",
        [string]$ReadLength = "",
        [string]$DelayMs = "",
        [string]$Pec = "",
        [string]$Comment = ""
    )

    return [pscustomobject][ordered]@{
        Kind       = $Kind
        Selected   = $Selected
        Profile    = $Profile
        Type       = $Type
        Address    = $Address
        Command    = $Command
        Data       = $Data
        ReadLength = $ReadLength
        DelayMs    = $DelayMs
        PEC        = $Pec
        Comment    = $Comment
    }
}

function Add-Comment {
    param(
        [System.Collections.ArrayList]$Rows,
        [string]$Profile,
        [string]$Text
    )
    [void]$Rows.Add((New-BaseRow -Kind "Comment" -Profile $Profile -Comment $Text))
}

function Add-Command {
    param(
        [System.Collections.ArrayList]$Rows,
        [string]$Profile,
        [string]$Type,
        [string]$Command = "",
        [string]$Data = "",
        [string]$ReadLength = "",
        [string]$DelayMs = $script:DelayMs,
        [string]$Pec = "0",
        [string]$Comment = "",
        [string]$Address = $script:Address
    )
    [void]$Rows.Add((New-BaseRow -Kind "Command" -Selected "1" -Profile $Profile -Type $Type `
        -Address $Address -Command $Command -Data $Data -ReadLength $ReadLength -DelayMs $DelayMs -Pec $Pec -Comment $Comment))
}

function ConvertTo-CsvField {
    param([object]$Value)
    $text = ""
    if ($null -ne $Value) {
        $text = [string]$Value
    }
    if ($text.Contains(",") -or $text.Contains('"') -or $text.Contains("`r") -or $text.Contains("`n")) {
        return '"' + $text.Replace('"', '""') + '"'
    }
    return $text
}

function Write-ScriptCsv {
    param(
        [System.Collections.ArrayList]$Rows,
        [string]$Path
    )

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add(($Columns -join ","))
    foreach ($row in $Rows) {
        $fields = foreach ($column in $Columns) {
            ConvertTo-CsvField $row.$column
        }
        $lines.Add(($fields -join ","))
    }
    $parent = Split-Path -Parent $Path
    if ($parent -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }
    [System.IO.File]::WriteAllLines((Resolve-Path -LiteralPath $parent).Path + "\" + (Split-Path -Leaf $Path), $lines, [System.Text.Encoding]::ASCII)
}

function Add-PmbusHeader {
    param(
        [System.Collections.ArrayList]$Rows,
        [string]$Profile,
        [string]$Suite
    )
    Add-Comment $Rows $Profile "Format=M032CSV; Source=button-suite; Suite=$Suite; PEC=False; Address=$Address; DelayMs=$DelayMs"
    Add-Comment $Rows $Profile "Set the PMBus tab profile to the matching profile before Load/Run."
    Add-Comment $Rows $Profile "CSV covers bus transactions. GUI-only assertions, dynamic read/restore checks, group write, ARA, and manual lab items remain in the PMBus Full button."
}

function Add-PmbusCommonFull {
    param(
        [System.Collections.ArrayList]$Rows,
        [string]$Profile
    )

    Add-Command $Rows $Profile "SendByte" "0x03" "" "" $DelayMs "0" "Full preflight CLEAR_FAULTS"
    Add-Command $Rows $Profile "BusRecover" "" "" "" $DelayMs "0" "Bus idle/recovery preflight"

    Add-Comment $Rows $Profile "Basic checklist transaction coverage"
    Add-Command $Rows $Profile "ReadByte" "0x98" "" "1" $DelayMs "0" "PMBUS_REVISION"
    Add-Command $Rows $Profile "ReadByte" "0x98" "" "1" $DelayMs "0" "Repeated START read path"
    Add-Command $Rows $Profile "ReadByte" "0x00" "" "1" $DelayMs "0" "PAGE baseline"
    Add-Command $Rows $Profile "WriteByte" "0x00" "00" "" $DelayMs "0" "PAGE write smoke"
    Add-Command $Rows $Profile "ReadByte" "0x00" "" "1" $DelayMs "0" "PAGE readback"
    Add-Command $Rows $Profile "ReadByte" "0x01" "" "1" $DelayMs "0" "OPERATION baseline"
    Add-Command $Rows $Profile "WriteByte" "0x01" "80" "" $DelayMs "0" "OPERATION write smoke"
    Add-Command $Rows $Profile "ReadByte" "0x01" "" "1" $DelayMs "0" "OPERATION readback"
    Add-Command $Rows $Profile "ReadByte" "0x02" "" "1" $DelayMs "0" "ON_OFF_CONFIG baseline"
    Add-Command $Rows $Profile "WriteByte" "0x02" "1A" "" $DelayMs "0" "ON_OFF_CONFIG write smoke"
    Add-Command $Rows $Profile "ReadByte" "0x02" "" "1" $DelayMs "0" "ON_OFF_CONFIG readback"
    Add-Command $Rows $Profile "ReadByte" "0x3A" "" "1" $DelayMs "0" "FAN_CONFIG_1_2 baseline"
    Add-Command $Rows $Profile "WriteByte" "0x3A" "00" "" $DelayMs "0" "FAN_CONFIG_1_2 write smoke"
    Add-Command $Rows $Profile "ReadByte" "0x3A" "" "1" $DelayMs "0" "FAN_CONFIG_1_2 readback"
    Add-Command $Rows $Profile "ReadByte" "0x20" "" "1" $DelayMs "0" "VOUT_MODE default"

    foreach ($cmd in @("0x78", "0x79", "0x7A", "0x7B", "0x7C", "0x7D", "0x7E", "0x7F", "0x80", "0x81")) {
        $type = "ReadByte"
        if ($cmd -eq "0x79") {
            $type = "ReadWord"
        }
        Add-Command $Rows $Profile $type $cmd "" "" $DelayMs "0" "Status read $cmd"
    }

    Add-Comment $Rows $Profile "Telemetry checklist transaction coverage"
    foreach ($cmd in @("0x88", "0x89", "0x8B", "0x8C", "0x8D", "0x8E", "0x8F", "0x90", "0x91", "0x96", "0x97")) {
        Add-Command $Rows $Profile "ReadWord" $cmd "" "" $DelayMs "0" "Telemetry ReadWord $cmd"
    }
    foreach ($cmd in @("0x83", "0x84")) {
        Add-Command $Rows $Profile "Read32" $cmd "" "" $DelayMs "0" "Telemetry Read32 $cmd"
    }
    foreach ($cmd in @("0x86", "0x87", "0x99", "0x9A", "0x9B", "0x9E")) {
        Add-Command $Rows $Profile "BlockRead" $cmd "" "40" $DelayMs "0" "Telemetry/identity BlockRead $cmd"
    }

    Add-Comment $Rows $Profile "Transport extension and ordering stress coverage"
    Add-Command $Rows $Profile "ReadWord" "0x21" "" "" $DelayMs "0" "VOUT_COMMAND baseline"
    Add-Command $Rows $Profile "ProcessCall" "0x21" "E0 2E" "2" $DelayMs "0" "Process Call VOUT_COMMAND"
    Add-Command $Rows $Profile "ReadWord" "0x21" "" "" $DelayMs "0" "VOUT_COMMAND post Process Call"
    Add-Command $Rows $Profile "BlockWriteReadProcessCall" "0x1A" "98" "1" $DelayMs "0" "QUERY PMBUS_REVISION"
    Add-Command $Rows $Profile "BlockWriteReadProcessCall" "0x1B" "00" "2" $DelayMs "0" "SMBALERT_MASK read"
    Add-Command $Rows $Profile "WriteWord" "0x1B" "00 00" "" $DelayMs "0" "SMBALERT_MASK write smoke"
    Add-Command $Rows $Profile "BlockWriteReadProcessCall" "0x1B" "00" "2" $DelayMs "0" "SMBALERT_MASK readback"
    Add-Command $Rows $Profile "ReadByte" "0x98" "" "1" $DelayMs "0" "Stress ReadByte before ReadWord"
    Add-Command $Rows $Profile "ReadWord" "0x79" "" "" $DelayMs "0" "Stress ReadWord"
    Add-Command $Rows $Profile "ReadByte" "0x98" "" "1" $DelayMs "0" "Stress ReadByte after ReadWord"
    Add-Command $Rows $Profile "BlockRead" "0x9A" "" "32" $DelayMs "0" "Stress BlockRead before ReadByte"
    Add-Command $Rows $Profile "ReadByte" "0x98" "" "1" $DelayMs "0" "Stress ReadByte after BlockRead"
    Add-Command $Rows $Profile "ProcessCall" "0x21" "E0 2E" "2" $DelayMs "0" "Stress ProcessCall before ReadByte"
    Add-Command $Rows $Profile "ReadByte" "0x98" "" "1" $DelayMs "0" "Stress ReadByte after ProcessCall"
    Add-Command $Rows $Profile "BlockWriteReadProcessCall" "0x1A" "98" "1" $DelayMs "0" "Stress BWRPC before ReadByte"
    Add-Command $Rows $Profile "ReadByte" "0x98" "" "1" $DelayMs "0" "Stress ReadByte after BWRPC"

    Add-Comment $Rows $Profile "PEC and negative-path coverage"
    Add-Command $Rows $Profile "ReadByte" "0x98" "" "1" $DelayMs "1" "PEC Read Byte PMBUS_REVISION"
    Add-Command $Rows $Profile "ReadWord" "0x79" "" "" $DelayMs "1" "PEC Read Word STATUS_WORD"
    Add-Command $Rows $Profile "BlockRead" "0x9A" "" "32" $DelayMs "1" "PEC Block Read MFR_MODEL"
    Add-Command $Rows $Profile "BadPecWriteByte" "0x02" "1A" "" $DelayMs "1" "Expected bad-PEC negative write"
    Add-Command $Rows $Profile "SendByte" "0x03" "" "" $DelayMs "0" "Post bad-PEC CLEAR_FAULTS"
    Add-Command $Rows $Profile "BusRecover" "" "" "" $DelayMs "0" "Post bad-PEC bus recover"
    Add-Command $Rows $Profile "ReadByte" "0x98" "" "1" $DelayMs "0" "Positive read after bad PEC"
}

function Add-PmbusBaseFull {
    $rows = New-RowList
    $profile = "PMBus-Base"
    Add-PmbusHeader $rows $profile "PMBus Base Full"
    Add-PmbusCommonFull $rows $profile
    Add-Comment $rows $profile "PMBus Base USER/MFR namespace shadow coverage"
    Add-Command $rows $profile "BlockRead" "0xB0" "" "32" $DelayMs "0" "USER_DATA_00 baseline"
    Add-Command $rows $profile "BlockWrite" "0xB0" "55 A5 10 20" "" $DelayMs "0" "USER_DATA_00 block write smoke"
    Add-Command $rows $profile "BlockRead" "0xB0" "" "32" $DelayMs "0" "USER_DATA_00 readback"
    Add-Command $rows $profile "BlockRead" "0xC4" "" "32" $DelayMs "0" "MFR_SPECIFIC_C4 baseline"
    Add-Command $rows $profile "BlockWrite" "0xC4" "C4 11 22 33" "" $DelayMs "0" "MFR_SPECIFIC_C4 block write smoke"
    Add-Command $rows $profile "BlockRead" "0xC4" "" "32" $DelayMs "0" "MFR_SPECIFIC_C4 readback"
    return $rows
}

function Add-PmbusCrpsFull {
    $rows = New-RowList
    $profile = "PMBus-CRPS"
    Add-PmbusHeader $rows $profile "PMBus M-CRPS Full"
    Add-PmbusCommonFull $rows $profile
    Add-Comment $rows $profile "M-CRPS profile command shadow read coverage"
    Add-Command $rows $profile "BlockRead" "0xB3" "" "16" $DelayMs "0" "MFR_EFFICIENCY_DATA"
    foreach ($cmd in @("0xC0", "0xC1", "0xC2", "0xD2", "0xD4", "0xD8", "0xE2", "0xE3", "0xEC", "0xED", "0xF0", "0xF2", "0xF3")) {
        Add-Command $rows $profile "ReadWord" $cmd "" "" $DelayMs "0" "M-CRPS ReadWord $cmd"
    }
    foreach ($cmd in @("0xD0", "0xD5", "0xD6", "0xDB", "0xDF", "0xE1", "0xE4")) {
        Add-Command $rows $profile "ReadByte" $cmd "" "1" $DelayMs "0" "M-CRPS ReadByte $cmd"
    }
    foreach ($pair in @(
        @("0xD0", "01"),
        @("0xDB", "01"),
        @("0xDF", "01"),
        @("0xE1", "01"),
        @("0xE4", "01")
    )) {
        Add-Command $rows $profile "WriteByte" $pair[0] $pair[1] "" $DelayMs "0" "M-CRPS WriteByte smoke $($pair[0])"
        Add-Command $rows $profile "ReadByte" $pair[0] "" "1" $DelayMs "0" "M-CRPS ReadByte after write $($pair[0])"
    }
    foreach ($pair in @(
        @("0xD8", "00 00"),
        @("0xE2", "00 00"),
        @("0xED", "00 00"),
        @("0xF0", "00 00"),
        @("0xF2", "00 00"),
        @("0xF3", "00 00")
    )) {
        Add-Command $rows $profile "WriteWord" $pair[0] $pair[1] "" $DelayMs "0" "M-CRPS WriteWord smoke $($pair[0])"
        Add-Command $rows $profile "ReadWord" $pair[0] "" "" $DelayMs "0" "M-CRPS ReadWord after write $($pair[0])"
    }
    foreach ($cmd in @("0xD1", "0xD9", "0xDC", "0xDD", "0xDE", "0xE9", "0xEB", "0xEE", "0xF1")) {
        Add-Command $rows $profile "BlockRead" $cmd "" "32" $DelayMs "0" "M-CRPS BlockRead $cmd"
    }
    Add-Command $rows $profile "BlockWriteReadProcessCall" "0xD3" "00" "16" $DelayMs "0" "MFR_READ_CONFIG_FILE BWRPC"
    Add-Command $rows $profile "BlockWriteReadProcessCall" "0xDA" "00 01" "16" $DelayMs "0" "MFR_SPDM BWRPC"
    Add-Command $rows $profile "BlockWrite" "0xE9" "01 02 03 04 05 06 07 08" "" $DelayMs "0" "MFR_PEAK_CURRENT_RECORD write smoke"
    Add-Command $rows $profile "BlockRead" "0xE9" "" "16" $DelayMs "0" "MFR_PEAK_CURRENT_RECORD readback"
    Add-Command $rows $profile "BlockWrite" "0xEE" "10 20 30 40 50 60 70 80" "" $DelayMs "0" "MFR_OCWPL1_SETTING write smoke"
    Add-Command $rows $profile "BlockRead" "0xEE" "" "16" $DelayMs "0" "MFR_OCWPL1_SETTING readback"
    return $rows
}

function Add-PmbusTiFull {
    $rows = New-RowList
    $profile = "PMBus-TI-UCD90xxx"
    Add-PmbusHeader $rows $profile "PMBus TI UCD90xxx Full"
    Add-PmbusCommonFull $rows $profile
    Add-Comment $rows $profile "TI UCD90xxx profile command smoke coverage"
    foreach ($cmd in @("0xB5", "0xB6", "0xB7", "0xB9", "0xD2", "0xD3", "0xD5", "0xD7", "0xDD", "0xDF", "0xE1", "0xE2", "0xE3", "0xE8", "0xE9", "0xEA", "0xEC", "0xED", "0xEF", "0xF1", "0xF2", "0xF3", "0xF6", "0xF8", "0xF9", "0xFC", "0xFD")) {
        Add-Command $rows $profile "BlockRead" $cmd "" "32" $DelayMs "0" "TI UCD90xxx BlockRead $cmd"
    }
    foreach ($cmd in @("0xD0", "0xD1", "0xD8", "0xDC", "0xE4", "0xE5", "0xEB")) {
        Add-Command $rows $profile "ReadWord" $cmd "" "" $DelayMs "0" "TI UCD90xxx ReadWord $cmd"
    }
    foreach ($cmd in @("0xD6", "0xDA", "0xE0", "0xE7", "0xEE", "0xF5", "0xF7", "0xFA", "0xFB")) {
        Add-Command $rows $profile "ReadByte" $cmd "" "1" $DelayMs "0" "TI UCD90xxx ReadByte $cmd"
    }
    foreach ($cmd in @("0xD4", "0xD9", "0xDB", "0xF0")) {
        Add-Command $rows $profile "SendByte" $cmd "" "" $DelayMs "0" "TI UCD90xxx SendByte smoke $cmd"
    }
    Add-Command $rows $profile "WriteWord" "0xDE" "00 00" "" $DelayMs "0" "TI UCD90xxx RESEQUENCE WriteWord smoke"
    return $rows
}

function Add-SmbusGenericRunAll {
    $rows = New-RowList
    $profile = "SMBus-Generic"
    Add-Comment $rows $profile "Format=M032CSV; Source=SMBus Generic Run All button; PEC=True on normal rows; Address=$Address; DelayMs=$DelayMs"
    Add-Command $rows $profile "QuickWrite" "" "" "" $DelayMs "0" "Example Quick Write"
    Add-Command $rows $profile "QuickRead" "" "" "" $DelayMs "0" "Example Quick Read"
    Add-Command $rows $profile "SendByte" "0x10" "" "" $DelayMs "1" "Example Send Byte A"
    Add-Command $rows $profile "SendByte" "0x11" "" "" $DelayMs "1" "Example Send Byte B"
    Add-Command $rows $profile "SendByte" "0x12" "" "" $DelayMs "1" "Example Receive Select A"
    Add-Command $rows $profile "ReceiveByte" "" "" "1" $DelayMs "1" "Example Receive Byte A"
    Add-Command $rows $profile "SendByte" "0x13" "" "" $DelayMs "1" "Example Receive Select B"
    Add-Command $rows $profile "ReceiveByte" "" "" "1" $DelayMs "1" "Example Receive Byte B"
    Add-Command $rows $profile "SendByte" "0x12" "" "" $DelayMs "1" "Stress Back-to-back Receive Select A"
    Add-Command $rows $profile "ReceiveByte" "" "" "1" $DelayMs "1" "Stress Back-to-back Receive Byte A"
    Add-Command $rows $profile "WriteByte" "0x20" "00" "" $DelayMs "1" "Example Write Byte A"
    Add-Command $rows $profile "WriteByte" "0x21" "81" "" $DelayMs "1" "Example Write Byte B"
    Add-Command $rows $profile "ReadByte" "0x22" "" "1" $DelayMs "1" "Example Read Byte A"
    Add-Command $rows $profile "ReadByte" "0x23" "" "1" $DelayMs "1" "Example Read Byte B"
    Add-Command $rows $profile "WriteWord" "0x30" "34 02" "" $DelayMs "1" "Example Write Word A"
    Add-Command $rows $profile "WriteWord" "0x31" "CD 83" "" $DelayMs "1" "Example Write Word B"
    Add-Command $rows $profile "ReadWord" "0x32" "" "2" $DelayMs "1" "Example Read Word A"
    Add-Command $rows $profile "ReadWord" "0x33" "" "2" $DelayMs "1" "Example Read Word B"
    Add-Command $rows $profile "BlockWrite" "0x40" "10 11 12 13 14 15 16 04" "" $DelayMs "1" "Example Block Write A"
    Add-Command $rows $profile "BlockWrite" "0x41" "20 21 22 23 24 25 26 27 28 29 2A 2B 2C 2D 2E 85" "" $DelayMs "1" "Example Block Write B"
    Add-Command $rows $profile "BlockRead" "0x42" "" "8" $DelayMs "1" "Example Block Read A"
    Add-Command $rows $profile "BlockRead" "0x43" "" "16" $DelayMs "1" "Example Block Read B"
    Add-Command $rows $profile "SendByte" "0x12" "" "" $DelayMs "1" "Stress After Block Read B Select A"
    Add-Command $rows $profile "ReceiveByte" "" "" "1" $DelayMs "1" "Stress After Block Read B Receive A"
    Add-Command $rows $profile "ProcessCall" "0x50" "34 06" "2" $DelayMs "1" "Example Process Call A"
    Add-Command $rows $profile "ProcessCall" "0x51" "CD 87" "2" $DelayMs "1" "Example Process Call B"
    Add-Command $rows $profile "BlockWriteReadProcessCall" "0x60" "01 02 03 04 05 06 07 08" "8" $DelayMs "1" "Example Block Process A"
    Add-Command $rows $profile "BlockWriteReadProcessCall" "0x61" "11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F 89" "16" $DelayMs "1" "Example Block Process B"
    Add-Command $rows $profile "SendByte" "0x12" "" "" $DelayMs "1" "Stress After Block Process B Select A"
    Add-Command $rows $profile "ReceiveByte" "" "" "1" $DelayMs "1" "Stress After Block Process B Receive A"
    Add-Command $rows $profile "BadPecWriteByte" "0x20" "4A" "" $DelayMs "1" "Example Bad PEC Write Byte"
    Add-Command $rows $profile "SendByte" "0x12" "" "" $DelayMs "1" "Stress After Bad PEC Select A"
    Add-Command $rows $profile "ReceiveByte" "" "" "1" $DelayMs "1" "Stress After Bad PEC Receive A"
    Add-Command $rows $profile "BusRecover" "" "" "" $DelayMs "0" "Bus Recover Preflight"
    return $rows
}

function Add-SmbusUbmRunAll {
    $rows = New-RowList
    $profile = "SMBus-UBM"
    Add-Comment $rows $profile "Format=M032CSV; Source=SMBus UBM Controller Run All button; Address=$Address; DelayMs=$DelayMs"
    Add-Command $rows $profile "ReadByte" "0x00" "" "1" $DelayMs "0" "UBM Operational State"
    Add-Command $rows $profile "ReadByte" "0x01" "" "1" $DelayMs "0" "UBM Last Command Status"
    Add-Command $rows $profile "BlockRead" "0x02" "" "14" $DelayMs "0" "UBM Silicon Identity"
    Add-Command $rows $profile "ReadByte" "0x03" "" "1" $DelayMs "0" "UBM Update Capabilities"
    Add-Command $rows $profile "ReadByte" "0x30" "" "1" $DelayMs "0" "UBM HFC Info"
    Add-Command $rows $profile "ReadByte" "0x31" "" "1" $DelayMs "0" "UBM Backplane Info"
    Add-Command $rows $profile "ReadByte" "0x32" "" "1" $DelayMs "0" "UBM Starting Slot"
    Add-Command $rows $profile "ReadWord" "0x33" "" "2" $DelayMs "0" "UBM Capabilities"
    Add-Command $rows $profile "ReadWord" "0x34" "" "2" $DelayMs "0" "UBM Features Read"
    Add-Command $rows $profile "WriteWord" "0x34" "00 00" "" $DelayMs "0" "UBM Features Write"
    Add-Command $rows $profile "ReadByte" "0x01" "" "1" $DelayMs "0" "UBM LCS After Features"
    Add-Command $rows $profile "ReadWord" "0x35" "" "2" $DelayMs "0" "UBM Change Count Read"
    Add-Command $rows $profile "WriteWord" "0x35" "01 00" "" $DelayMs "0" "UBM Change Count Clear"
    Add-Command $rows $profile "ReadByte" "0x01" "" "1" $DelayMs "0" "UBM LCS After Change Clear"
    Add-Command $rows $profile "WriteByte" "0x36" "00" "" $DelayMs "0" "UBM DFC Index 0"
    Add-Command $rows $profile "BlockRead" "0x40" "" "8" $DelayMs "0" "UBM DFC Descriptor 0"
    Add-Command $rows $profile "WriteByte" "0x36" "01" "" $DelayMs "0" "UBM DFC Index 1"
    Add-Command $rows $profile "BlockRead" "0x40" "" "8" $DelayMs "0" "UBM DFC Descriptor 1"
    Add-Command $rows $profile "BlockRead" "0x20" "" "5" $DelayMs "0" "UBM Enter Update Shell Read"
    Add-Command $rows $profile "BlockWrite" "0x20" "B8 55 42 4D 01" "" $DelayMs "0" "UBM Enter Update Shell Write"
    Add-Command $rows $profile "ReadByte" "0x00" "" "1" $DelayMs "0" "UBM Operational State Reduced"
    Add-Command $rows $profile "BlockWrite" "0x21" "00 02 AA 55" "" $DelayMs "0" "UBM PMDT Shell Write"
    Add-Command $rows $profile "BlockRead" "0x22" "" "4" $DelayMs "0" "UBM Exit Update Shell Read"
    Add-Command $rows $profile "BlockWrite" "0x22" "55 42 4D 01" "" $DelayMs "0" "UBM Exit Update Shell Write"
    Add-Command $rows $profile "ReadByte" "0x00" "" "1" $DelayMs "0" "UBM Operational State Ready"
    Add-Command $rows $profile "WriteByte" "0x37" "00" "" $DelayMs "0" "UBM CCC Control"
    Add-Command $rows $profile "WriteByte" "0x38" "00" "" $DelayMs "0" "UBM CCC Result Index"
    Add-Command $rows $profile "BlockRead" "0x41" "" "35" $DelayMs "0" "UBM CCC Result Descriptor"
    Add-Command $rows $profile "WriteByte" "0x50" "00" "" $DelayMs "0" "UBM Flex IO Index"
    Add-Command $rows $profile "BlockRead" "0x51" "" "5" $DelayMs "0" "UBM Flex IO Descriptor"
    Add-Command $rows $profile "BlockRead" "0x60" "" "32" $DelayMs "0" "UBM Power Event Data"
    Add-Command $rows $profile "BadChecksumWrite" "0x34" "00 00" "" $DelayMs "0" "UBM Bad Checksum Negative"
    Add-Command $rows $profile "ReadByte" "0x01" "" "1" $DelayMs "0" "UBM LCS After Bad Checksum"
    return $rows
}

$resolvedOutputDir = $OutputDir
if (-not [System.IO.Path]::IsPathRooted($resolvedOutputDir)) {
    $resolvedOutputDir = Join-Path (Get-Location) $resolvedOutputDir
}
New-Item -ItemType Directory -Path $resolvedOutputDir -Force | Out-Null

$scripts = [ordered]@{
    "PMBus_Base_Full.csv"        = Add-PmbusBaseFull
    "PMBus_M_CRPS_Full.csv"      = Add-PmbusCrpsFull
    "PMBus_TI_UCD90xxx_Full.csv" = Add-PmbusTiFull
    "SMBus_Generic_RunAll.csv"   = Add-SmbusGenericRunAll
    "SMBus_UBM_RunAll.csv"       = Add-SmbusUbmRunAll
}

foreach ($name in $scripts.Keys) {
    $path = Join-Path $resolvedOutputDir $name
    Write-ScriptCsv -Rows $scripts[$name] -Path $path
    $commandCount = @($scripts[$name] | Where-Object { $_.Kind -eq "Command" }).Count
    Write-Host ("{0}: {1} command rows" -f $path, $commandCount)
}
