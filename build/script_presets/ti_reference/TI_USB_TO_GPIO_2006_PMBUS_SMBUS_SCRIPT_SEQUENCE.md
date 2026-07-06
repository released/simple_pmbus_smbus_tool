# TI USB-TO-GPIO 2006 PMBus / SMBus ScriptForm Sequence

This file maps the requested PMBus / SMBus test prompt into concrete ScriptForm fields for an external TI USB-TO-GPIO 2006 script.

Scope:

- DUT: CRPS PSU PMBus slave.
- Primary profile: M-CRPS v1.06.00 RC1 draft command set.
- Default test address from the prompt: 7-bit `0x5A`.
- Supported ScriptForm transaction types only: `SendByte`, `WriteByte`, `ReadByte`, `WriteWord`, `ReadWord`, `BlockWrite`, `BlockRead`.
- Unsupported ScriptForm transactions are listed but must not be put in the default ScriptForm sequence.

Source basis:

- `docs/M-CRPS_Base_Specification_version_1p06p00_RC1-draft7_042026.pdf`
- `docs/PMBUS_COMMAND_CONTRACT.csv`
- PMBus firmware workspace: `SampleCode/Template/pmbus/pmbus_protocol.h`
- PMBus firmware workspace: `SampleCode/Template/pmbus/pmbus_profile_mcrps.h`
- PMBus firmware workspace: `SampleCode/Template/pmbus/pmbus_dispatch.c`

Important limitation:

- This is a script-entry reference, not a compliance certificate.
- Do not claim full M-CRPS compliance unless each mandatory command, status behavior, timing behavior, PEC behavior, and SMBALERT# behavior has been verified on the actual PSU.

## Addressing

The prompt default is:

```text
7-bit address = 0x5A
```

M-CRPS v1.06 Table 12-1 lists PMBus device read/write address pairs as 8-bit addresses:

| A1/A0 selection | 8-bit write/read | 7-bit equivalent |
|---|---:|---:|
| `0/0` non-redundant default | `B0h/B1h` | `0x58` |
| `0/1` | `B2h/B3h` | `0x59` |
| `1/0` | `B4h/B5h` | `0x5A` |
| `1/1` | `B6h/B7h` | `0x5B` |
| extended addressing option | `B8h/B9h` | `0x5C` |
| extended addressing option | `BAh/BBh` | `0x5D` |

Use `0x5A` only when the DUT A1/A0 strap selection maps to `B4h/B5h`.

## ScriptForm Field Rule

For every row below, fill ScriptForm fields as:

```text
Device Address: <Device Address column, default 0x5A>
Type:           <SMBus Type column>
Code(hex):      <Command Code Hex without the 0x prefix>
Data(hex):      <Data Hex column; leave empty for reads>
Delay:          <Delay ms column>
```

For `BlockWrite`, do not include the byte count in `Data(hex)` unless the TI ScriptForm specifically asks for it. The normal SMBus block count byte is transaction framing, not command payload.

## Generated CSV From M032 M-CRPS Log

`build/script_presets/ti_reference/SMBusI2CScriptForm.csv` is generated from:

```text
M031BSP_I2C_Slave_PMBus/teraterm_PROFILE_M_CRPS.log
```

Generation policy:

- The CSV follows the MCU log `PMBus RX` order, including repeated commands from repeated test phases.
- Read rows keep the current ScriptForm command-only style, for example `ReadByte,0x5A,0x98`. Response bytes from TeraTerm are not written as fixed expected data, so the same script can still run against a different DUT without failing only because telemetry or inventory strings differ.
- Write rows include the payload data bytes observed in the log. PEC bytes are not copied into the `Data` field.
- `WriteWord` rows are entered in the byte order required by TI ScriptForm so the logic-analyzer bus payload is PMBus low-byte then high-byte. In the first LA run, entering MCU-log byte order directly caused the tool to emit word data reversed on the bus.
- ScriptForm-unsupported transactions are kept as `Comment,Skipped...` rows so the replay order still shows where the original log had coverage that this CSV cannot execute.
- A `Pause,10,` row is inserted after every executable ScriptForm command. TI Fusion treats this as a 10 ms pause, which gives the 115200 baud MCU UART debug log time to drain between commands during batch validation.

Generated summary from the current log:

| Item | Count |
|---|---:|
| Total `PMBus RX` entries in log | 1324 |
| ScriptForm command rows emitted | 1275 |
| `ReadByte` rows | 388 |
| `ReadWord` rows | 594 |
| `BlockRead` rows | 201 |
| `SendByte` rows | 38 |
| `WriteByte` rows | 27 |
| `WriteWord` rows | 27 |
| `Pause,10,` rows inserted after executable commands | 1275 |
| ScriptForm-unsupported entries kept as comments | 49 |

Unsupported entries in this log are:

| Protocol gap | Commands in log | Count |
|---|---|---:|
| `ReadDWord` | `READ_KWH_IN (0x83)`, `READ_KWH_OUT (0x84)` | 10 |
| `ProcessCall` | `VOUT_COMMAND (0x21)` | 4 |
| `Block Write-Block Read Process Call` | `PAGE_PLUS_READ (0x06)`, `QUERY (0x1A)`, `SMBALERT_MASK (0x1B)`, `COEFFICIENTS (0x30)`, `MFR_READ_CONFIG_FILE (0xD3)`, `MFR_SPDM (0xDA)` | 35 |

## Current CSV Executable Command Inventory

The current `build/script_presets/ti_reference/SMBusI2CScriptForm.csv` contains 1275 executable ScriptForm rows, 1275 `Pause,10,` rows, and 170 unique PMBus command codes. The table below records exactly which command codes are executable in the current CSV and how many times each code appears. Command names should be resolved through `docs/PMBUS_COMMAND_CONTRACT.csv` plus the selected M-CRPS profile overlay.

| ScriptForm type | Rows | Unique commands | Exact command codes in current CSV |
|---|---:|---:|---|
| `SendByte` | 38 | 1 | `0x03` x38 |
| `ReadByte` | 388 | 41 | `0x00` x24, `0x01` x16, `0x02` x17, `0x04` x14, `0x10` x4, `0x19` x6, `0x20` x21, `0x34` x5, `0x3A` x18, `0x3D` x5, `0x41` x6, `0x45` x5, `0x47` x3, `0x49` x5, `0x4C` x5, `0x50` x4, `0x54` x5, `0x56` x6<br>`0x5A` x5, `0x5C` x5, `0x63` x6, `0x69` x4, `0x78` x7, `0x7A` x10, `0x7B` x5, `0x7C` x10, `0x7D` x10, `0x7E` x22, `0x7F` x10, `0x80` x10, `0x81` x10, `0x82` x6, `0x98` x19, `0xAC` x5, `0xD0` x14, `0xD5` x6<br>`0xD6` x10, `0xDB` x10, `0xDF` x10, `0xE1` x16, `0xE4` x9 |
| `ReadWord` | 594 | 95 | `0x07` x4, `0x08` x2, `0x21` x23, `0x22` x6, `0x23` x3, `0x24` x6, `0x25` x5, `0x26` x4, `0x27` x3, `0x28` x5, `0x29` x5, `0x2A` x6, `0x2B` x5, `0x31` x4, `0x32` x5, `0x33` x5, `0x35` x6, `0x36` x5<br>`0x37` x5, `0x38` x5, `0x39` x6, `0x3B` x5, `0x3C` x5, `0x3E` x5, `0x3F` x4, `0x40` x6, `0x42` x4, `0x43` x4, `0x44` x4, `0x46` x5, `0x48` x6, `0x4A` x4, `0x4B` x6, `0x4F` x4, `0x51` x5, `0x52` x6<br>`0x53` x6, `0x55` x4, `0x57` x3, `0x58` x6, `0x59` x6, `0x5B` x4, `0x5D` x4, `0x5E` x6, `0x5F` x6, `0x60` x3, `0x61` x3, `0x62` x6, `0x64` x6, `0x65` x3, `0x66` x6, `0x68` x5, `0x6A` x6, `0x6B` x6<br>`0x79` x13, `0x85` x5, `0x88` x10, `0x89` x9, `0x8A` x5, `0x8B` x18, `0x8C` x10, `0x8D` x5, `0x8E` x9, `0x8F` x7, `0x90` x10, `0x91` x7, `0x92` x4, `0x93` x6, `0x94` x6, `0x95` x5, `0x96` x10, `0x97` x8<br>`0xA0` x2, `0xA1` x6, `0xA2` x6, `0xA3` x5, `0xA4` x5, `0xA5` x4, `0xA6` x4, `0xA7` x6, `0xA8` x5, `0xA9` x6, `0xC0` x8, `0xC1` x7, `0xC2` x10, `0xD2` x6, `0xD4` x7, `0xD8` x13, `0xE2` x12, `0xE3` x5<br>`0xEC` x5, `0xED` x10, `0xF0` x15, `0xF2` x9, `0xF3` x11 |
| `BlockRead` | 201 | 31 | `0x86` x7, `0x87` x6, `0x99` x11, `0x9A` x17, `0x9B` x9, `0x9C` x2, `0x9D` x4, `0x9E` x6, `0x9F` x6, `0xAA` x4, `0xAB` x3, `0xAD` x6, `0xAE` x5, `0xB0` x15, `0xB1` x3, `0xB2` x3, `0xB3` x8, `0xB4` x3<br>`0xB8` x3, `0xBF` x2, `0xC4` x2, `0xD1` x8, `0xD9` x9, `0xDC` x8, `0xDD` x8, `0xDE` x1, `0xE9` x13, `0xEB` x7, `0xEE` x13, `0xF1` x7, `0xFD` x2 |
| `WriteByte` | 27 | 12 | `0x00` x4, `0x02` x4, `0x04` x4, `0x0F` x4, `0x20` x1, `0x3A` x1, `0x79` x4, `0xD0` x1, `0xDB` x1, `0xDF` x1, `0xE1` x1, `0xE4` x1 |
| `WriteWord` | 27 | 6 | `0x1B` x3, `0x21` x18, `0xD8` x2, `0xED` x2, `0xF2` x1, `0xF3` x1 |

Notable M-CRPS profile commands present in the executable CSV include `0xB3`, `0xC0..0xC2`, `0xD0..0xD2`, `0xD4..0xD6`, `0xD8..0xD9`, `0xDB..0xDF`, `0xE1..0xE4`, `0xE9`, `0xEB..0xEE`, and `0xF0..0xF3`. Commands such as `0xD3 MFR_READ_CONFIG_FILE` and `0xDA MFR_SPDM` are not executable in this CSV because they require Block Write-Block Read Process Call.

## Coverage Comparison With M032 GUI PMBus Checklist

The TI ScriptForm CSV and the M032 GUI PMBus tab do not measure the same thing.

| Area | TI USB-to-GPIO ScriptForm CSV | M032 GUI PMBus tab buttons |
|---|---|---|
| Primary purpose | External host replay of M-CRPS-heavy PMBus/SMBus traffic, useful for UART log and logic-analyzer correlation. | Active validation tool with command decoding, PEC checking, negative checks, restore logic, and profile-aware pass/fail criteria. |
| Scan | Included as repeated `PMBUS_REVISION`, `MFR_ID`, and `MFR_MODEL` reads in the generated CSV. | `Scan` probes address range and summarizes address, PMBus revision, MFR ID, and MFR model. |
| Basic | Covered by many replayed read/write rows, but no integrated semantic assertion beyond tool success/failure. | `Basic` runs 18 explicit cases: bus ACK, repeated-start read, PAGE/OPERATION/ON_OFF_CONFIG/FAN_CONFIG write/read/restore, VOUT_MODE, and status reads. |
| PEC | Can run with TI tool PEC mode if enabled; current CSV does not inject wrong PEC by itself. | `PEC` runs read-byte/read-word/block-read PEC checks plus a forced bad-PEC negative test and `STATUS_CML` verification. |
| Error | Includes replay rows for `0x0F` and wrong-form `0x79` from the M032 log. | `Error` deliberately probes unsupported command and illegal transaction behavior, then validates `STATUS_CML` and recovers. |
| Telemetry | Broad repeated read coverage: voltage/current/temp/fan/power/rating commands are replayed. | `Telemetry` runs 17 explicit read checks with decode and payload-size assertions. |
| MFR / profile | Very broad for ScriptForm-supported M-CRPS read/write forms: 170 unique command codes total in the CSV. | `MFR` / transport extensions include profile command sweep, selected CRPS shadow write/read/restore, `QUERY`, `SMBALERT_MASK`, group command, ALERT# poll, and CONTROL line helper. |
| Transaction coverage | Limited to ScriptForm-supported rows in this generated CSV: `SendByte`, `ReadByte`, `ReadWord`, `BlockRead`, `WriteByte`, `WriteWord`. | Covers more PMBus-specific behavior: Read32, Process Call, Block Write-Read Process Call, Group Command, bad PEC, bus recovery preflight, ALERT#/CONTROL helpers, and manual markers. |
| Safety model | Replays the logged command order; high-risk writes must be reviewed before using on a real PSU. | Uses explicit restore patterns for many writes and marks high-risk/product-specific items as manual. |

Coverage conclusion:

- The current TI ScriptForm CSV has larger raw replay volume and a broad list of M-CRPS command codes that are useful for external-host regression.
- M032 GUI `Full` has larger functional/protocol coverage because it validates transaction families and behaviors that ScriptForm cannot express, including bad PEC, Process Call / Block Write-Read Process Call, group command, ALERT#/CONTROL, and restore semantics.
- Use the TI ScriptForm CSV as an external host and LA/UART correlation test. Use M032 GUI `Full` when the goal is the broader PMBus behavior checklist.

## PEC Policy

M-CRPS command summary uses `w/PEC` for normal PMBus access.

If the TI ScriptForm exposes a PEC enable option:

1. Run the safe default read sequence with PEC disabled as a basic electrical/transport check.
2. Run the same sequence with PEC enabled as the normal M-CRPS validation pass.
3. Do not append request-side PEC bytes manually in the `Data(hex)` field if the tool generates PEC.

For PMBus command-read transactions, response PEC is calculated over:

```text
ADDR+W, Command, ADDR+R, Response Data
```

For write transactions, request PEC is calculated over:

```text
ADDR+W, Command, Data
```

If the TI ScriptForm does not expose PEC configuration or wrong-PEC injection, mark PEC negative tests as manual / custom-host required.

## Safe Default Script Sequence

This is the recommended default sequence for a first external script. It avoids OPERATION writes, output control writes, threshold writes, NVM/store commands, and firmware upload.

| Phase | Order | Device Address | SMBus Type | Command Code Hex | Command Name | Data Hex | Delay ms | Expected Response | Mandatory / Optional / MFR | Purpose | Risk | Notes |
|---|---:|---:|---|---:|---|---|---:|---|---|---|---|---|
| Basic Communication | 1 | `0x5A` | `ReadByte` | `0x78` | `STATUS_BYTE` |  | 10 | 1 byte status | Mandatory | Confirm PMBus status-byte read path. | Low | If CML bit is set, read `STATUS_CML`. |
| Basic Communication | 2 | `0x5A` | `ReadWord` | `0x79` | `STATUS_WORD` |  | 10 | 2 byte status, low byte first | Mandatory | Capture full status before other tests. | Low | Correlate high-level bits with detailed `STATUS_xxx`. |
| Basic Communication | 3 | `0x5A` | `ReadByte` | `0x19` | `CAPABILITY` |  | 10 | 1 byte capability | Mandatory | Confirm PEC / 400 kHz / SMBALERT capability bits. | Low | M-CRPS expects PEC, 400 kHz, SMBALERT# supported. |
| Basic Communication | 4 | `0x5A` | `ReadByte` | `0x98` | `PMBUS_REVISION` |  | 10 | 1 byte revision | Mandatory | Confirm advertised PMBus revision. | Low | M-CRPS v1.06 table says revision `0x22` for PMBus 1.2. Current M032 sample may return `0x33` for PMBus 1.3/1.3. |
| Basic Communication | 5 | `0x5A` | `BlockRead` | `0x99` | `MFR_ID` |  | 10 | Block string | Mandatory | Read manufacturer ID. | Low | M-CRPS v1.06 says length 12 bytes. |
| Basic Communication | 6 | `0x5A` | `BlockRead` | `0x9A` | `MFR_MODEL` |  | 10 | Block string | Mandatory | Read PSU model. | Low | M-CRPS v1.06 says length 16 bytes including terminator / padding. |
| Basic Communication | 7 | `0x5A` | `BlockRead` | `0x9B` | `MFR_REVISION` |  | 10 | Block string | Mandatory | Read revision string. | Low | M-CRPS v1.06 says length 16 bytes including terminator / padding. |
| Basic Communication | 8 | `0x5A` | `BlockRead` | `0x9E` | `MFR_SERIAL` |  | 10 | Block string | Mandatory | Read serial number. | Low | M-CRPS v1.06 says length 14 bytes. |
| PAGE / Config | 9 | `0x5A` | `ReadByte` | `0x00` | `PAGE` |  | 10 | 1 byte page | Mandatory | Confirm current PMBus page. | Low | M-CRPS uses page `00h` / `01h` for several status groups. |
| PAGE / Config | 10 | `0x5A` | `WriteByte` | `0x00` | `PAGE` | `00` | 10 | ACK | Medium | Force page 0 for following status reads. | Medium | Only safe if writing `PAGE=00h` is allowed by DUT policy. |
| PAGE / Config | 11 | `0x5A` | `ReadByte` | `0x01` | `OPERATION` |  | 10 | 1 byte | Mandatory | Read output operation state. | Low | Do not write OPERATION in the default script. |
| PAGE / Config | 12 | `0x5A` | `ReadByte` | `0x02` | `ON_OFF_CONFIG` |  | 10 | 1 byte | Mandatory | Read on/off policy. | Low | Do not write in the default script. |
| PAGE / Config | 13 | `0x5A` | `ReadByte` | `0x10` | `WRITE_PROTECT` |  | 10 | 1 byte | Mandatory | Confirm write protection state. | Low | M-CRPS table lists read/write byte. |
| Telemetry | 14 | `0x5A` | `ReadWord` | `0x88` | `READ_VIN` |  | 10 | 2 bytes | Mandatory | Input voltage telemetry. | Low | PMBus linear format. |
| Telemetry | 15 | `0x5A` | `ReadWord` | `0x89` | `READ_IIN` |  | 10 | 2 bytes | Mandatory | Input current telemetry. | Low | PMBus linear format. |
| Telemetry | 16 | `0x5A` | `ReadWord` | `0x8A` | `READ_VCAP` |  | 10 | 2 bytes | Mandatory | Bulk capacitor / internal voltage telemetry. | Low | If not implemented by product, expect NACK or CML unsupported. |
| Telemetry | 17 | `0x5A` | `ReadWord` | `0x8B` | `READ_VOUT` |  | 10 | 2 bytes | Mandatory | Output voltage telemetry. | Low | PMBus format depends on product; M032 sample provides deterministic placeholder. |
| Telemetry | 18 | `0x5A` | `ReadWord` | `0x8C` | `READ_IOUT` |  | 10 | 2 bytes | Mandatory | Output current telemetry. | Low | M-CRPS total 12V current. |
| Telemetry | 19 | `0x5A` | `ReadWord` | `0x8D` | `READ_TEMPERATURE_1` |  | 10 | 2 bytes | Mandatory | Ambient / inlet temperature. | Low | M-CRPS names this Ambient. |
| Telemetry | 20 | `0x5A` | `ReadWord` | `0x8E` | `READ_TEMPERATURE_2` |  | 10 | 2 bytes | Mandatory | Hot spot temperature. | Low | M-CRPS names this Hot Spot. |
| Telemetry | 21 | `0x5A` | `ReadWord` | `0x8F` | `READ_TEMPERATURE_3` |  | 10 | 2 bytes | Optional | Exhaust temperature. | Low | M-CRPS v1.06 table lists Exhaust. |
| Telemetry | 22 | `0x5A` | `ReadWord` | `0x90` | `READ_FAN_SPEED_1` |  | 10 | 2 bytes | Mandatory | Fan 1 speed. | Low | Accuracy depends on product fan sensor. |
| Telemetry | 23 | `0x5A` | `ReadWord` | `0x91` | `READ_FAN_SPEED_2` |  | 10 | 2 bytes | Optional | Fan 2 speed. | Low | Product may have only one fan. |
| Telemetry | 24 | `0x5A` | `ReadWord` | `0x94` | `READ_DUTY_CYCLE` |  | 10 | 2 bytes | Optional | PWM duty telemetry. | Low | M-CRPS table typo spells `READ_DUTTY_CYCLE`. |
| Telemetry | 25 | `0x5A` | `ReadWord` | `0x96` | `READ_POUT` |  | 10 | 2 bytes | Mandatory | Output power telemetry. | Low | PMBus linear format. |
| Telemetry | 26 | `0x5A` | `ReadWord` | `0x97` | `READ_PIN` |  | 10 | 2 bytes | Mandatory | Input power telemetry. | Low | PMBus linear format. |
| Status Detail | 27 | `0x5A` | `ReadWord` | `0x79` | `STATUS_WORD` |  | 10 | 2 bytes | Mandatory | Capture root status before detail reads. | Low | `STATUS_WORD` bits point to detailed registers. |
| Status Detail | 28 | `0x5A` | `ReadByte` | `0x7A` | `STATUS_VOUT` |  | 10 | 1 byte | Mandatory | Detail VOUT fault/warning. | Low | M-CRPS maps bit 7 OV fault, bit 6 OV warning, bit 5 UV warning, bit 4 UV fault, bit 2 TON_MAX fault. |
| Status Detail | 29 | `0x5A` | `ReadByte` | `0x7B` | `STATUS_IOUT` |  | 10 | 1 byte | Mandatory | Detail IOUT / POUT fault/warning. | Low | Correlates with `STATUS_WORD.IOUT/POUT`. |
| Status Detail | 30 | `0x5A` | `ReadByte` | `0x7C` | `STATUS_INPUT` |  | 10 | 1 byte | Mandatory | Detail input fault/warning. | Low | Correlates with `STATUS_WORD.INPUT`. |
| Status Detail | 31 | `0x5A` | `ReadByte` | `0x7D` | `STATUS_TEMPERATURE` |  | 10 | 1 byte | Mandatory | Detail temperature fault/warning. | Low | Correlates with `STATUS_WORD.TEMPERATURE`. |
| Status Detail | 32 | `0x5A` | `ReadByte` | `0x7E` | `STATUS_CML` |  | 10 | 1 byte | Mandatory | Communication / memory / logic fault detail. | Low | PEC error should set bit 5 if product reports PEC faults. |
| Status Detail | 33 | `0x5A` | `ReadByte` | `0x7F` | `STATUS_OTHER` |  | 10 | 1 byte | Mandatory | Miscellaneous status. | Low | M-CRPS includes cold redundancy event in bit 0. |
| Status Detail | 34 | `0x5A` | `ReadByte` | `0x80` | `STATUS_MFR_SPECIFIC` |  | 10 | 1 byte | Mandatory | Manufacturer status detail. | Low | M-CRPS maps line status / derating indicators here. |
| Status Detail | 35 | `0x5A` | `ReadByte` | `0x81` | `STATUS_FANS_1_2` |  | 10 | 1 byte | Mandatory | Fan status detail. | Low | Correlates with `STATUS_WORD.FANS`. |
| Status Detail | 36 | `0x5A` | `ReadByte` | `0x82` | `STATUS_FANS_3_4` |  | 10 | 1 byte | Optional / Probe | Fan 3/4 status detail. | Low | M-CRPS summary does not emphasize Fan 3/4; PMBus base/M032 sample implements placeholder. |

## Controlled Fault Clear Phase

Run only after the status capture above has been saved.

| Phase | Order | Device Address | SMBus Type | Command Code Hex | Command Name | Data Hex | Delay ms | Expected Response | Mandatory / Optional / MFR | Purpose | Risk | Notes |
|---|---:|---:|---|---:|---|---|---:|---|---|---|---|---|
| Fault Clear | 1 | `0x5A` | `ReadWord` | `0x79` | `STATUS_WORD` |  | 10 | 2 bytes | Mandatory | Pre-clear status snapshot. | Low | Save this value before clearing. |
| Fault Clear | 2 | `0x5A` | `SendByte` | `0x03` | `CLEAR_FAULTS` |  | 20 | ACK | Mandatory | Clear latched fault bits. | Medium | If the fault source is still active, status and SMBALERT# must re-assert or remain asserted. |
| Fault Clear | 3 | `0x5A` | `ReadWord` | `0x79` | `STATUS_WORD` |  | 10 | 2 bytes | Mandatory | Post-clear status snapshot. | Low | Compare with pre-clear status. |
| Fault Clear | 4 | `0x5A` | `ReadByte` | `0x7E` | `STATUS_CML` |  | 10 | 1 byte | Mandatory | Confirm communication error state. | Low | Useful after PEC / unsupported-command tests. |

SMBALERT# rule:

- `CLEAR_FAULTS` must not be treated as unconditional SMBALERT# release.
- If the underlying event is still present, SMBALERT# remains low.
- M-CRPS v1.06 states that the PSU shall not support ARA and shall keep its normal address after asserting SMBALERT#.

## Optional Extended Read Sequence

These commands expand coverage while staying read-only. Some are M-CRPS mandatory, some are inventory/rating support, and some are optional probes depending on PSU capability.

| Phase | Order | Device Address | SMBus Type | Command Code Hex | Command Name | Data Hex | Delay ms | Expected Response | Mandatory / Optional / MFR | Purpose | Risk | Notes |
|---|---:|---:|---|---:|---|---|---:|---|---|---|---|---|
| Extended Read | 1 | `0x5A` | `BlockRead` | `0x86` | `READ_EIN` |  | 10 | Block response | Mandatory / if supported | Input energy accumulator. | Low | M-CRPS lists block read with PEC. |
| Extended Read | 2 | `0x5A` | `BlockRead` | `0x87` | `READ_EOUT` |  | 10 | Block response | Mandatory / if supported | Output energy accumulator. | Low | M-CRPS lists block read with PEC. |
| Extended Read | 3 | `0x5A` | `BlockRead` | `0x9C` | `MFR_LOCATION` |  | 10 | 16 byte block string | Optional / MFR | Manufacturing location. | Low | M-CRPS v1.06 says 16 bytes including terminator / padding. |
| Extended Read | 4 | `0x5A` | `BlockRead` | `0x9D` | `MFR_DATE` |  | 10 | 6 byte block string | Optional / MFR | Manufacturing date. | Low | Date format is product-defined. |
| Extended Read | 5 | `0x5A` | `ReadByte` | `0x9F` | `APP_PROFILE_SUPPORT` |  | 10 | 1 byte | Mandatory / MFR | Confirm CRPS application profile. | Low | M-CRPS v1.06 expects `0x05`. |
| Extended Read | 6 | `0x5A` | `ReadWord` | `0xA0` | `MFR_VIN_MIN` |  | 10 | 2 bytes | Optional / MFR | Rated min input voltage. | Low | PMBus linear format. |
| Extended Read | 7 | `0x5A` | `ReadWord` | `0xA1` | `MFR_VIN_MAX` |  | 10 | 2 bytes | Optional / MFR | Rated max input voltage. | Low | PMBus linear format. |
| Extended Read | 8 | `0x5A` | `ReadWord` | `0xA2` | `MFR_IIN_MAX` |  | 10 | 2 bytes | Optional / MFR | Rated max input current. | Low | PMBus linear format. |
| Extended Read | 9 | `0x5A` | `ReadWord` | `0xA3` | `MFR_PIN_MAX` |  | 10 | 2 bytes | Optional / MFR | Rated max input power. | Low | PMBus linear format. |
| Extended Read | 10 | `0x5A` | `ReadWord` | `0xA4` | `MFR_VOUT_MIN` |  | 10 | 2 bytes | Optional / MFR | Rated min output voltage. | Low | PMBus linear format. |
| Extended Read | 11 | `0x5A` | `ReadWord` | `0xA5` | `MFR_VOUT_MAX` |  | 10 | 2 bytes | Optional / MFR | Rated max output voltage. | Low | PMBus linear format. |
| Extended Read | 12 | `0x5A` | `ReadWord` | `0xA6` | `MFR_IOUT_MAX` |  | 10 | 2 bytes | Mandatory / MFR | Rated output current. | Low | M-CRPS marks read word with PEC. |
| Extended Read | 13 | `0x5A` | `ReadWord` | `0xA7` | `MFR_POUT_MAX` |  | 10 | 2 bytes | Mandatory / MFR | Rated output power. | Low | M-CRPS marks read word with PEC. |
| Extended Read | 14 | `0x5A` | `ReadWord` | `0xA8` | `MFR_TAMBIENT_MAX` |  | 10 | 2 bytes | Optional / MFR | Max ambient threshold. | Low | Product binding required for real threshold. |
| Extended Read | 15 | `0x5A` | `ReadWord` | `0xA9` | `MFR_TAMBIENT_MIN` |  | 10 | 2 bytes | Optional / MFR | Min ambient threshold. | Low | Product binding required for real threshold. |
| Extended Read | 16 | `0x5A` | `BlockRead` | `0xAD` | `IC_DEVICE_ID` |  | 10 | Block response | Optional / MFR | Controller device ID. | Low | Product-specific content. |
| Extended Read | 17 | `0x5A` | `BlockRead` | `0xAE` | `IC_DEVICE_REV` |  | 10 | Block response | Optional / MFR | Controller device revision. | Low | Product-specific content. |

## M-CRPS Manufacturer / Profile Command Coverage

This section maps the M-CRPS profile commands from v1.06 Table 12-38 and the current M032 M-CRPS command overlay.

Default ScriptForm policy:

- Include read-only rows in the optional extended sequence.
- Do not include write/control rows in the default sequence unless product owner approves the exact value.
- Do not run firmware upload commands against a real PSU without a signed/valid firmware image and recovery plan.

| Phase | Order | Device Address | SMBus Type | Command Code Hex | Command Name | Data Hex | Delay ms | Expected Response | Mandatory / Optional / MFR | Purpose | Risk | Notes |
|---|---:|---:|---|---:|---|---|---:|---|---|---|---|---|
| M-CRPS MFR | 1 | `0x5A` | `ReadByte` | `0xD0` | `MFR_COLD_REDUNDANCY_CONFIG` |  | 10 | 1 byte | MFR | Read cold redundancy role/config. | Low | Write path exists but changes redundancy behavior. |
| M-CRPS MFR | 2 | `0x5A` | `WriteByte` | `0xD0` | `MFR_COLD_REDUNDANCY_CONFIG` | `<byte>` | 20 | ACK | MFR | Set cold redundancy config. | High | Do not run by default; wrong value may change PSU redundancy behavior. |
| M-CRPS MFR | 3 | `0x5A` | `BlockRead` | `0xD1` | `MFR_READ_CONFIG_FILE_SIZE` |  | 10 | Block response | MFR | Read config file size. | Low | Current M032 sample returns deterministic placeholder block. |
| M-CRPS MFR | 4 | `0x5A` | `ReadWord` | `0xD2` | `MFR_READ_CONFIG_BLOCK_SIZE` |  | 10 | 2 bytes | MFR | Read config block size. | Low | Use before attempting config-file block reads. |
| M-CRPS MFR | 5 | `0x5A` | Unsupported | `0xD3` | `MFR_READ_CONFIG_FILE` |  | 0 | Block Write-Block Read Process Call | MFR | Read a config file block. | Medium | TI ScriptForm transaction list lacks Block Write-Block Read Process Call. Custom host required. |
| M-CRPS MFR | 6 | `0x5A` | `ReadWord` | `0xD4` | `MFR_HW_COMPATIBILITY` |  | 10 | 2 bytes | MFR | Read hardware compatibility code. | Low | Two ASCII bytes in spec. |
| M-CRPS MFR | 7 | `0x5A` | `ReadByte` | `0xD5` | `MFR_FWUPLOAD_CAPABILITY` |  | 10 | 1 byte | MFR | Read firmware-upload capability bits. | Low | Bit 3 indicates `MFR_FWUPLOAD_BLOCK_SIZE` support. |
| M-CRPS MFR | 8 | `0x5A` | `ReadByte` | `0xD6` | `MFR_FWUPLOAD_MODE` |  | 10 | 1 byte | MFR | Read firmware-upload mode. | Low | `0` normal mode, `1` upload mode. |
| M-CRPS MFR | 9 | `0x5A` | `WriteByte` | `0xD6` | `MFR_FWUPLOAD_MODE` | `00` | 50 | ACK | MFR | Request normal mode / exit upload mode. | High | Only run if DUT owner approves. If image corrupt, PSU may stay in upload mode. |
| M-CRPS MFR | 10 | `0x5A` | `WriteByte` | `0xD6` | `MFR_FWUPLOAD_MODE` | `01` | 50 | ACK | MFR | Enter firmware-upload mode. | High | Destructive/control phase only. Requires valid FW image and recovery procedure. |
| M-CRPS MFR | 11 | `0x5A` | `BlockWrite` | `0xD7` | `MFR_FWUPLOAD` | `<image block without count>` | 100 | ACK | MFR | Send firmware image header/data block. | High | Do not run in default script. Data must include sequence/header format required by M-CRPS. |
| M-CRPS MFR | 12 | `0x5A` | `ReadWord` | `0xD8` | `MFR_FWUPLOAD_STATUS` |  | 20 | 2 bytes | MFR | Read firmware upload status. | Low | May also support write word in current M032 sample; write is not default-safe. |
| M-CRPS MFR | 13 | `0x5A` | `BlockRead` | `0xD9` | `MFR_FW_REVISION` |  | 10 | 7 byte block | MFR | Read firmware revision. | Low | M-CRPS v1.06 states 7 bytes. |
| M-CRPS MFR | 14 | `0x5A` | Unsupported | `0xDA` | `MFR_SPDM` |  | 0 | Block Write-Block Read Process Call | MFR | SPDM transport placeholder. | High | TI ScriptForm transaction list lacks required combined block process call. |
| M-CRPS MFR | 15 | `0x5A` | `ReadByte` | `0xDB` | `MFR_FRU_PROTECTION` |  | 10 | 1 byte | MFR | Read FRU protection state. | Low | Write path can affect FRU protection. |
| M-CRPS MFR | 16 | `0x5A` | `WriteByte` | `0xDB` | `MFR_FRU_PROTECTION` | `<byte>` | 20 | ACK | MFR | Set FRU protection. | High | Do not run without product policy. |
| M-CRPS MFR | 17 | `0x5A` | `BlockRead` | `0xDC` | `MFR_BLACKBOX` |  | 10 | Block response | MFR | Read blackbox record. | Low | Spec says 230 bytes; SMBus block read is normally limited to 32 bytes, so check actual product/tool capability. Current M032 sample uses bounded placeholder. |
| M-CRPS MFR | 18 | `0x5A` | `BlockRead` | `0xDD` | `MFR_REAL_TIME_BLACK_BOX` |  | 10 | 4 byte block | MFR | Read real-time black box. | Low | Spec states 4 bytes. |
| M-CRPS MFR | 19 | `0x5A` | `BlockWrite` | `0xDD` | `MFR_REAL_TIME_BLACK_BOX` | `<4 bytes>` | 20 | ACK | MFR | Write real-time black box selector/control data. | Medium | Product-specific. Do not run by default. |
| M-CRPS MFR | 20 | `0x5A` | `BlockRead` | `0xDE` | `MFR_SYSTEM_BLACK_BOX` |  | 10 | 40 byte block per spec | MFR | Read system black box. | Low | SMBus block read normally maxes at 32 bytes; current M032 placeholder is bounded to 32 bytes. Verify product/tool support. |
| M-CRPS MFR | 21 | `0x5A` | `BlockWrite` | `0xDE` | `MFR_SYSTEM_BLACK_BOX` | `<block>` | 20 | ACK | MFR | Write system black box selector/control data. | Medium | Product-specific. Do not run by default. |
| M-CRPS MFR | 22 | `0x5A` | `ReadByte` | `0xDF` | `MFR_BLACK_BOX_CONFIG` |  | 10 | 1 byte | MFR | Read blackbox configuration. | Low | M-CRPS profile name differs from base `MFR_SPECIFIC_DF`. |
| M-CRPS MFR | 23 | `0x5A` | `WriteByte` | `0xDF` | `MFR_BLACK_BOX_CONFIG` | `<byte>` | 20 | ACK | MFR | Set blackbox configuration. | High | Spec says state should be saved in non-volatile storage; do not run by default. |
| M-CRPS MFR | 24 | `0x5A` | `WriteByte` | `0xE0` | `MFR_CLEAR_BLACK_BOX` | `<selector>` | 50 | ACK | MFR | Clear selected blackbox content. | High | Destructive; do not run by default. |
| M-CRPS MFR | 25 | `0x5A` | `ReadByte` | `0xE1` | `MFR_LINE_STATUS` |  | 10 | 1 byte | MFR | Read line status. | Low | Current M032 sample also allows write for simulation/shadow. |
| M-CRPS MFR | 26 | `0x5A` | `WriteByte` | `0xE1` | `MFR_LINE_STATUS` | `<byte>` | 20 | ACK | MFR | Set/simulate line status. | High | Product-specific; not default-safe. |
| M-CRPS MFR | 27 | `0x5A` | `ReadWord` | `0xE2` | `MFR_SYSTEM_LED_CNTL` |  | 10 | 2 bytes | MFR | Read system LED control state. | Low | Hardware LED binding is product-specific. |
| M-CRPS MFR | 28 | `0x5A` | `WriteWord` | `0xE2` | `MFR_SYSTEM_LED_CNTL` | `<lo hi>` | 20 | ACK | MFR | Set system LED control state. | High | Do not run unless LED behavior is understood. |
| M-CRPS MFR | 29 | `0x5A` | `ReadWord` | `0xE3` | `MFR_FWUPLOAD_BLOCK_SIZE` |  | 10 | 2 bytes | MFR | Read supported upload block size. | Low | Use before `MFR_FWUPLOAD`. |
| M-CRPS MFR | 30 | `0x5A` | `ReadByte` | `0xE4` | `MFR_EN_STATUS_SIMULATION_CMD` |  | 10 | 1 byte | MFR | Read status simulation enable. | Low | Used by validation/test firmware. |
| M-CRPS MFR | 31 | `0x5A` | `WriteByte` | `0xE4` | `MFR_EN_STATUS_SIMULATION_CMD` | `<byte>` | 20 | ACK | MFR | Enable/disable status simulation. | High | Can intentionally assert status / SMBALERT#. Keep out of default script. |
| M-CRPS MFR | 32 | `0x5A` | `BlockRead` | `0xE9` | `MFR_PEAK_CURRENT_RECORD` |  | 10 | Block response | MFR | Read peak current record. | Low | Current M032 sample exposes bounded block placeholder. |
| M-CRPS MFR | 33 | `0x5A` | `BlockWrite` | `0xE9` | `MFR_PEAK_CURRENT_RECORD` | `<block>` | 20 | ACK | MFR | Write/clear peak current record control data. | Medium | Product-specific. |
| M-CRPS MFR | 34 | `0x5A` | `BlockRead` | `0xEB` | `MFR_COMPONENT_ID` |  | 10 | Block response | MFR | Read component identity. | Low | Current M032 sample uses 12-byte placeholder. |
| M-CRPS MFR | 35 | `0x5A` | `ReadWord` | `0xEC` | `MFR_TOT_POUT_MAX` |  | 10 | 2 bytes | MFR | Read total output power based on line status. | Low | Dual-rated supplies may return line-status-dependent value. |
| M-CRPS MFR | 36 | `0x5A` | `ReadWord` | `0xED` | `MFR_VOUT_MARGINING` |  | 10 | 2 bytes | MFR | Read VOUT margining control. | Low | Write path affects output margining. |
| M-CRPS MFR | 37 | `0x5A` | `WriteWord` | `0xED` | `MFR_VOUT_MARGINING` | `<lo hi>` | 20 | ACK | MFR | Set VOUT margining. | High | Can affect output voltage. Do not run by default. |
| M-CRPS MFR | 38 | `0x5A` | `BlockRead` | `0xEE` | `MFR_OCWPL1_SETTING` |  | 10 | Block response | MFR | Read OCWPL1 setting. | Low | Current M032 sample uses bounded block placeholder. |
| M-CRPS MFR | 39 | `0x5A` | `BlockWrite` | `0xEE` | `MFR_OCWPL1_SETTING` | `<block>` | 20 | ACK | MFR | Set OCWPL1 setting. | High | Protection threshold/control behavior. Do not run by default. |
| M-CRPS MFR | 40 | `0x5A` | `ReadWord` | `0xF0` | `MFR_PWOK_WARNING_TIME` |  | 10 | 2 bytes | MFR | Read PWOK warning time. | Low | PMBus linear format. |
| M-CRPS MFR | 41 | `0x5A` | `WriteWord` | `0xF0` | `MFR_PWOK_WARNING_TIME` | `<lo hi>` | 20 | ACK | MFR | Set PWOK warning time. | High | Timing behavior change. |
| M-CRPS MFR | 42 | `0x5A` | `BlockRead` | `0xF1` | `MFR_MAX_IOUT_CAPABILITY` |  | 10 | 14 byte block | MFR | Read output current capability table. | Low | M-CRPS v1.06 says 14 bytes. |
| M-CRPS MFR | 43 | `0x5A` | `ReadWord` | `0xF2` | `MFR_FPC_MAIN_MIN_OFF_TIME` |  | 10 | 2 bytes | MFR | Read main output min-off timing. | Low | Product-specific timing. |
| M-CRPS MFR | 44 | `0x5A` | `WriteWord` | `0xF2` | `MFR_FPC_MAIN_MIN_OFF_TIME` | `<lo hi>` | 20 | ACK | MFR | Set main output min-off timing. | High | Timing/control behavior change. |
| M-CRPS MFR | 45 | `0x5A` | `ReadWord` | `0xF3` | `MFR_FPC_12VSB_MIN_OFF_TIME` |  | 10 | 2 bytes | MFR | Read standby output min-off timing. | Low | Product-specific timing. |
| M-CRPS MFR | 46 | `0x5A` | `WriteWord` | `0xF3` | `MFR_FPC_12VSB_MIN_OFF_TIME` | `<lo hi>` | 20 | ACK | MFR | Set standby output min-off timing. | High | Timing/control behavior change. |

## Unsupported By ScriptForm Transaction List

These are valid PMBus / M-CRPS command forms, but they require transactions outside the prompt's ScriptForm capability list.

| Command Code Hex | Command Name | Required Transaction | Why unsupported in this ScriptForm list | Suggested handling |
|---:|---|---|---|---|
| `0x05` | `PAGE_PLUS_WRITE` | Block Write | Tool supports `BlockWrite`, but the command's payload must include PAGE, target command, and clear/mask value. Use only after product-specific payload is defined. |
| `0x06` | `PAGE_PLUS_READ` | Block Write-Block Read Process Call | Combined block write/read is not in the allowed ScriptForm transaction list. | Custom host / different tool required. |
| `0x1A` | `QUERY` | Block Write-Block Read Process Call | Combined block write/read is not in the allowed ScriptForm transaction list. | Custom host / different tool required. |
| `0x1B` | `SMBALERT_MASK` read | Block Write-Block Read Process Call | Read requires PAGE_PLUS-style combined transaction. | Custom host / different tool required. |
| `0x21` | `VOUT_COMMAND` process-call form | Process Call | The log includes Process Call probes, but `ProcessCall` is not in the allowed ScriptForm type list. | Use `WriteWord` / `ReadWord` only for separate non-process-call coverage, or use a custom host for true Process Call. |
| `0x30` | `COEFFICIENTS` | Block Write-Block Read Process Call | Combined block write/read is not in the allowed ScriptForm transaction list. | Custom host / different tool required. |
| `0x83` | `READ_KWH_IN` | Read 32 / ReadDWord | `ReadDWord` is not in the allowed ScriptForm type list. | Custom host / different tool required, or skip in ScriptForm CSV. |
| `0x84` | `READ_KWH_OUT` | Read 32 / ReadDWord | `ReadDWord` is not in the allowed ScriptForm type list. | Custom host / different tool required, or skip in ScriptForm CSV. |
| `0xD3` | `MFR_READ_CONFIG_FILE` | Block Write-Block Read Process Call | Combined block write/read is not in the allowed ScriptForm transaction list. | Custom host / different tool required. |
| `0xDA` | `MFR_SPDM` | Block Write-Block Read Process Call | Combined block write/read is not in the allowed ScriptForm transaction list. | Custom host / different tool required. |

## Optional Probe Commands From Prompt But Not Strict CRPS Default

These commands are useful for PMBus-base or current M032 sample probing, but M-CRPS v1.06 Table 12-38 marks some related codes as reserved or does not list them as required CRPS default commands.

| Phase | Order | Device Address | SMBus Type | Command Code Hex | Command Name | Data Hex | Delay ms | Expected Response | Mandatory / Optional / MFR | Purpose | Risk | Notes |
|---|---:|---:|---|---:|---|---|---:|---|---|---|---|---|
| Optional Probe | 1 | `0x5A` | `ReadByte` | `0x20` | `VOUT_MODE` |  | 10 | 1 byte | Optional / Probe | PMBus base voltage format probe. | Low | Prompt requested this, but M-CRPS v1.06 Table 12-38 shows `0x20` as reserved. Use only if DUT supports PMBus base behavior. |
| Optional Probe | 2 | `0x5A` | `ReadWord` | `0x95` | `READ_FREQUENCY` |  | 10 | 2 bytes | Optional / Probe | Switching frequency telemetry. | Low | Current M032 sample supports it; M-CRPS v1.06 Table 12-38 does not list it in the CRPS summary. |
| Optional Probe | 3 | `0x5A` | `ReadByte` | `0x82` | `STATUS_FANS_3_4` |  | 10 | 1 byte | Optional / Probe | PMBus base fan 3/4 status probe. | Low | Current M032 sample returns placeholder. |

## Optional High-Risk Control Phase

Keep this out of the default external script. Run only on bench equipment with electronic load, output monitoring, and recovery path.

| Phase | Order | Device Address | SMBus Type | Command Code Hex | Command Name | Data Hex | Delay ms | Expected Response | Mandatory / Optional / MFR | Purpose | Risk | Notes |
|---|---:|---:|---|---:|---|---|---:|---|---|---|---|---|
| High Risk | 1 | `0x5A` | `WriteByte` | `0x01` | `OPERATION` | `<byte>` | 50 | ACK | Optional | Change output operation state. | High | Do not run unless exact operation byte is approved. |
| High Risk | 2 | `0x5A` | `WriteByte` | `0x02` | `ON_OFF_CONFIG` | `<byte>` | 50 | ACK | Optional | Change on/off control policy. | High | Can change PSU enable behavior. |
| High Risk | 3 | `0x5A` | `WriteWord` | `0x3B` | `FAN_COMMAND_1` | `<lo hi>` | 50 | ACK | Optional | Change fan command. | High | Can alter cooling behavior. |
| High Risk | 4 | `0x5A` | `WriteWord` | `0x51` | `OT_WARN_LIMIT` | `<lo hi>` | 50 | ACK | Optional | Change over-temperature warning threshold. | High | Protection threshold behavior. |
| High Risk | 5 | `0x5A` | `WriteWord` | `0x5D` | `IIN_OC_WARN_LIMIT` | `<lo hi>` | 50 | ACK | Optional | Change input over-current warning threshold. | High | Protection threshold behavior. |
| High Risk | 6 | `0x5A` | `WriteByte` | `0xE4` | `MFR_EN_STATUS_SIMULATION_CMD` | `<byte>` | 50 | ACK | MFR | Enable status simulation. | High | Intended for validation firmware only; may assert status and SMBALERT#. |
| High Risk | 7 | `0x5A` | `WriteByte` | `0xD6` | `MFR_FWUPLOAD_MODE` | `01` | 100 | ACK | MFR | Enter firmware upload mode. | High | Requires valid image and recovery plan. |

## SMBALERT# / ARA Test Notes

M-CRPS v1.06 states:

- The PSU shall not support ARA.
- After asserting SMBALERT#, the PSU shall keep its standard PMBus address and not change to the ARA address.
- SMBALERT# is level-driven by unmasked events.
- SMBALERT# shall not de-assert while the event that caused assertion is still present.
- SMBALERT# can be cleared/re-armed by clearing the causing status bits, power cycling, or masking the event with `SMBALERT_MASK`.

Therefore, for strict M-CRPS product testing:

| Step | ScriptForm command | Expected result |
|---|---|---|
| 1 | Capture `STATUS_WORD` and detailed `STATUS_xxx`. | Fault/warning source is visible before clear. |
| 2 | Observe SMBALERT# pin with logic analyzer / GPIO. | Low when an unmasked event is active. |
| 3 | Do not use ARA as the normal release mechanism. | ARA is not supported by M-CRPS v1.06. |
| 4 | Send `CLEAR_FAULTS` only after capture. | Latched bits clear only if underlying event is not active. |
| 5 | Re-read `STATUS_WORD` and detailed `STATUS_xxx`. | If event remains active, bits reassert or remain set. |
| 6 | Re-check SMBALERT#. | Low if event remains active; high only when no unmasked active source remains. |

If a lab firmware intentionally provides ARA for SMBus-layer validation, keep it in a separate non-CRPS lab-validation script and use address `0x0C`. Do not merge that behavior into the default M-CRPS script.

## Negative Tests

Do not put these in the default script. Run after the positive path is stable and after the bus recovery procedure is verified.

| Phase | Order | Device Address | SMBus Type | Command Code Hex | Command Name | Data Hex | Delay ms | Expected Response | Mandatory / Optional / MFR | Purpose | Risk | Notes |
|---|---:|---:|---|---:|---|---|---:|---|---|---|---|---|
| Negative | 1 | `0x5A` | `ReadByte` | `0xE5` | `Reserved / unsupported probe` |  | 20 | NACK, reject, or `STATUS_CML` unsupported | Optional | Unsupported command handling. | High | M-CRPS v1.06 marks `E5h` reserved. Confirm product does not crash or wedge bus. |
| Negative | 2 | `0x5A` | `BlockWrite` | `0xEE` | `MFR_OCWPL1_SETTING` | `00` | 20 | Reject or CML invalid data | Optional | Wrong block length. | High | Use only if product owner approves testing this command. |
| Negative | 3 | `0x5A` | `WriteByte` | `0x00` | `PAGE` | `FF` | 20 | Product-defined; may accept global page for `CLEAR_FAULTS` semantics | Optional | Invalid/global PAGE behavior. | Medium | M-CRPS discusses `PAGE=FFh` for `CLEAR_FAULTS`; do not assume invalid. |
| Negative | 4 | `0x5A` | `ReadWord` | `0x78` | `STATUS_BYTE wrong type` |  | 20 | Reject or CML unsupported/wrong type | Optional | Wrong transaction type. | High | ReadByte is the legal type. |
| Negative | 5 | `0x5A` | `WriteByte` | `0xD6` | `MFR_FWUPLOAD_MODE invalid value` | `02` | 20 | Reject or CML invalid data | Optional | Invalid data value. | High | Can affect upload mode; bench only. |
| Negative | 6 | `0x5A` | `WriteByte` | `0xD0` | `MFR_COLD_REDUNDANCY_CONFIG invalid value` | `FF` | 20 | Reject or CML invalid data | Optional | Invalid redundancy config. | High | Can affect redundancy behavior; bench only. |
| Negative | 7 | `0x5A` | PEC injection | any safe command | wrong PEC | 20 | Reject, NACK, or `STATUS_CML.PACKET_ERROR_CHECK_FAILED` | Optional | PEC error handling. | Medium | Requires ScriptForm wrong-PEC support or custom host. |

## Commands Found In M-CRPS But Not Executable With This ScriptForm List

These are not necessarily missing from firmware; they are missing from the allowed ScriptForm transaction set in the prompt.

| Command Code Hex | Command Name | Reason |
|---:|---|---|
| `0x06` | `PAGE_PLUS_READ` | Requires Block Write-Block Read Process Call. |
| `0x1A` | `QUERY` | Requires Block Write-Block Read Process Call. |
| `0x1B` | `SMBALERT_MASK` read | Requires Block Write-Block Read Process Call. Write side can use `WriteWord` only after page/status payload is defined. |
| `0x30` | `COEFFICIENTS` | Requires Block Write-Block Read Process Call. |
| `0xD3` | `MFR_READ_CONFIG_FILE` | Requires Block Write-Block Read Process Call. |
| `0xDA` | `MFR_SPDM` | Requires Block Write-Block Read Process Call. |

## Commands In Current M032 Sample But Not Strict M-CRPS Default Script

The current M032 PMBus sample intentionally implements a broader PMBus base surface than this strict M-CRPS ScriptForm sequence.

Examples:

| Command Code Hex | Command Name | Why not in strict default script |
|---:|---|---|
| `0x20` | `VOUT_MODE` | Prompt requested it and M032 supports it, but M-CRPS v1.06 Table 12-38 marks `0x20` reserved. Put it in optional probe only. |
| `0x21` | `VOUT_COMMAND` | Can affect output voltage behavior; not default-safe. |
| `0x22` to `0x2A` | VOUT programming/config commands | M-CRPS v1.06 marks related range reserved in Table 12-38; not default CRPS script coverage. |
| `0x95` | `READ_FREQUENCY` | M032 supports it; M-CRPS v1.06 summary does not list it in the default CRPS command table. |
| `0x82` | `STATUS_FANS_3_4` | PMBus base/M032 placeholder exists; M-CRPS v1.06 focuses on `STATUS_FANS_1_2`. |

## Missing Information Before Final External Script

Fill or confirm these before using the script on a real CRPS PSU:

1. Actual A1/A0 strap state and 7-bit PMBus address.
2. TI ScriptForm PEC mode: available, forced, disabled, or automatic.
3. Whether ScriptForm can inject bad PEC; if not, PEC negative tests need a custom host.
4. Whether ScriptForm can perform Block Write-Block Read Process Call; the prompt says no, so `QUERY`, `PAGE_PLUS_READ`, `SMBALERT_MASK` read, `MFR_READ_CONFIG_FILE`, and `MFR_SPDM` need another host.
5. Product-approved values for every write command.
6. Expected MFR strings and fixed lengths for the actual PSU.
7. Actual telemetry scaling and expected nominal ranges.
8. SMBALERT# observation method and whether the test fixture can induce safe faults.
9. Firmware upload image format, signature, recovery path, and whether upload mode is permitted.
10. Whether blackbox block sizes larger than 32 bytes are supported by the tool/DUT combination.

## Logic Analyzer Checklist

Use this checklist when validating the external script against the bus.

| Check | Expected waveform / decode |
|---|---|
| Address | 7-bit `0x5A` should decode as write byte `0xB4` and read byte `0xB5`. |
| ReadByte | `S, 0xB4(W), cmd, Sr, 0xB5(R), data, PEC if enabled, P`. |
| ReadWord | Same command-read flow; response data is low byte then high byte, followed by PEC if enabled. |
| BlockRead | First response byte is block count, followed by exactly count data bytes, then PEC if enabled. |
| SendByte | `S, 0xB4(W), cmd, PEC if enabled, P`. |
| WriteByte | `S, 0xB4(W), cmd, data, PEC if enabled, P`. |
| WriteWord | `S, 0xB4(W), cmd, low byte, high byte, PEC if enabled, P`. |
| BlockWrite | `S, 0xB4(W), cmd, count, payload bytes, PEC if enabled, P`; ScriptForm `Data(hex)` normally excludes count. |
| PEC read frame | PEC covers `0xB4, cmd, 0xB5, response data`. |
| PEC write frame | PEC covers `0xB4, cmd, data bytes`. |
| SMBALERT# | For strict M-CRPS, no ARA address switch. Device remains at normal address. Alert stays low while active unmasked event remains present. |
| Recovery | After negative tests, bus should ACK normal `STATUS_WORD` read again. |

## Minimal Safe ScriptForm Entry List

Use this compact list when manually entering the first pass:

```text
Device Address: 0x5A, Type: ReadByte,  Code(hex): 78, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadWord,  Code(hex): 79, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadByte,  Code(hex): 19, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadByte,  Code(hex): 98, Data(hex):, Delay: 10
Device Address: 0x5A, Type: BlockRead, Code(hex): 99, Data(hex):, Delay: 10
Device Address: 0x5A, Type: BlockRead, Code(hex): 9A, Data(hex):, Delay: 10
Device Address: 0x5A, Type: BlockRead, Code(hex): 9B, Data(hex):, Delay: 10
Device Address: 0x5A, Type: BlockRead, Code(hex): 9E, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadByte,  Code(hex): 00, Data(hex):, Delay: 10
Device Address: 0x5A, Type: WriteByte, Code(hex): 00, Data(hex): 00, Delay: 10
Device Address: 0x5A, Type: ReadByte,  Code(hex): 01, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadByte,  Code(hex): 02, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadByte,  Code(hex): 10, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadWord,  Code(hex): 88, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadWord,  Code(hex): 89, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadWord,  Code(hex): 8A, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadWord,  Code(hex): 8B, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadWord,  Code(hex): 8C, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadWord,  Code(hex): 8D, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadWord,  Code(hex): 8E, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadWord,  Code(hex): 8F, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadWord,  Code(hex): 90, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadWord,  Code(hex): 91, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadWord,  Code(hex): 94, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadWord,  Code(hex): 96, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadWord,  Code(hex): 97, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadWord,  Code(hex): 79, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadByte,  Code(hex): 7A, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadByte,  Code(hex): 7B, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadByte,  Code(hex): 7C, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadByte,  Code(hex): 7D, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadByte,  Code(hex): 7E, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadByte,  Code(hex): 7F, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadByte,  Code(hex): 80, Data(hex):, Delay: 10
Device Address: 0x5A, Type: ReadByte,  Code(hex): 81, Data(hex):, Delay: 10
```
