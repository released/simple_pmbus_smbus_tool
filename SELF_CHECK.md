# SELF_CHECK.md

## Current Verification Results

Date: 2026-07-06

## PC Tool

Command:

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build_mfc.ps1 -Configuration Release -Platform x64
```

Result: PASS

Expected output:

- Bumped PC build version to `1.0.0.247`
- Built `build\PmbusSmbusHidTool.exe`
- MSBuild result: 0 warnings, 0 errors

## PMBus Contract Validation

Command:

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\validate_pmbus_contract.ps1
```

Result: PASS

- Rows: 224
- Bridge transport ready: 224
- Target policy gaps: 0
- Host-visible shadows/placeholders: 168
- Concrete/non-placeholder behavior rows: 56
- GUI/spec name mismatch warnings: 0

## Firmware Syntax Check

Commands:

```bat
arm-none-eabi-gcc -mcpu=cortex-m0 -mthumb -std=gnu99 -fsyntax-only ... m031_bridge_i2c.c
arm-none-eabi-gcc -mcpu=cortex-m0 -mthumb -std=gnu99 -fsyntax-only ... hid_tool_api.c
```

Result: PASS

- `m031_bridge_i2c.c` passed GNU ARM `gnu99` syntax-only.
- `hid_tool_api.c` passed GNU ARM `gnu99` syntax-only.

## Firmware Keil Build

Result: NOT RUN

Reason: Keil `UV4`/`armclang` command line is not available in this shell.

Expected firmware info after rebuild and flash:

- `m032-pmbus-smbus-bridge/1.1.3`

## Hardware Test

Result: NOT RUN in this workspace session.

Recommended hardware smoke tests:

1. Flash `Template.uvprojx` firmware to M032 EVB.
2. Launch `build\PmbusSmbusHidTool.exe`.
3. Scan/connect VID `0x0416`, PID `0x5020`.
4. Click `Get Info`; expect firmware string `m032-pmbus-smbus-bridge/1.1.3`.
5. Select each supported I2C0/I2C1 pin pair and run master enable.
6. Run SMBus Quick and PMBus read/write tests against a known-good device.
7. Run PMBus bus recovery preflight.
8. Open the `I2C` tab, enable master mode, and run `Write`, `Read`, and `Write Then Read`.
9. Open the `I2C` tab, enable slave mode, preload `Set TX`, and verify an external master can read it.
10. In slave mode, have an external master write bytes to the selected address and verify `Get RX`.

## Known Gaps

- Hardware validation is still required after flashing bridge firmware `1.1.3`.
- Generic I2C slave support is implemented but must be hardware-validated on the selected pin pair.
- M48X support is not implemented yet.
- FW Upload validation belongs to the sibling `simple_programming_tool` workspace.
