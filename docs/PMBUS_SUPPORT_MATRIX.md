# PMBus Support Matrix

Last updated: 2026-06-27

## Scope

This project provides a Windows MFC PMBus/SMBus test tool that talks to an M032 EVB USB HID bridge. The bridge firmware exposes the EVB I2C controller as a PMBus/SMBus master.

Current scope:

- Bridge board: M032 EVB
- Future bridge board: M487 EVB, not implemented yet
- GUI tabs: `PMBus` and `SMBus`
- Bus role: master only
- Active I2C instance: one selected I2C port/pin pair at a time
- PMBus/SMBus command behavior: inherited from the existing GUI command model; only the hardware I2C port/pin selection changes

Out of current scope:

- Non-master PMBus/SMBus bus roles
- Bridge-side PMBus device emulation
- Fixed GPIO side-band wiring for ALERT# or CONTROL
- AVSBus physical/link-layer support

## Bridge And I2C Selection

The GUI owns I2C selection. Firmware must initialize the active I2C controller from the GUI-provided tuple:

- I2C port: `I2C0` or `I2C1`
- SDA pin
- SCL pin
- bus speed

Supported M032 pin pairs:

- I2C0: `PB.4/PB.5`, `PF.2/PF.3`, `PA.4/PA.5`, `PC.0/PC.1`
- I2C1: `PB.2/PB.3`, `PB.0/PB.1`, `PA.6/PA.7`, `PA.2/PA.3`, `PF.1/PF.0`, `PC.4/PC.5`, `PB.10/PB.11`

Only one I2C owner should be active at a time. The GUI pin registry displays M032 pin names as `PA.x`, `PB.x`, `PC.x`, or `PF.x`.

## Transport Supported By PMBus Tab

Supported master transactions:

- `Send Byte`
- `Receive Byte`
- `Write Byte`
- `Write Word`
- `Read Byte`
- `Read Word`
- `Read 32`
- `Block Write`
- `Block Read`
- `Process Call`
- `Block Write-Read Process Call`
- staged long block transfers
- PMBus group write

Supported helpers:

- PEC enable/disable
- Force Bad PEC transmit option for negative-path validation against a target device
- ARA receive-byte helper at address `0x0C`
- I2C bus idle/recover preflight
- per-command legal transaction validation from PMBus 1.3.1 Part II
- `SMBALERT_MASK` read/write helper
- extended command mode through `FEh MFR_SPECIFIC_COMMAND_EXT` and `FFh PMBUS_COMMAND_EXT`

The ARA and `SMBALERT_MASK` helpers are I2C/PMBus transactions. The current tool does not claim a fixed ALERT# GPIO monitor or CONTROL GPIO driver.

## SMBus Tab Scope

Supported master workflows:

- Quick command
- byte/word read and write
- block read and write where exposed by the GUI
- optional PEC handling
- user-selected I2C port/pin pair

No additional SMBus bus role is exposed.

## Profile Policy

The GUI uses one `PMBus` tab with a `Profile` combo.

- `PMBus Base` exposes the public PMBus command names.
- `M-CRPS` overlays public M-CRPS command names.
- `TI UCD90xxx` overlays TI vendor command names and transaction hints.

Checklist scope:

- `Scan`, `Basic`, `PEC`, `Error`, and `Telemetry` remain common PMBus/SMBus validation flows.
- `MFR` validates the selected profile namespace.
- `Full` includes common validation plus the selected profile's MFR/User command suite.

## Command Contract

Command-level synchronization source:

- `docs/PMBUS_COMMAND_CONTRACT.csv`
- `docs/PMBUS_COMMAND_CONTRACT.md`
- generated GUI include: `src/core/pmbus_contract.generated.inc`

When PMBus command support changes, update the CSV first, regenerate the include, and run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\validate_pmbus_contract.ps1
```

## Validation Boundary

A feature may be called supported for this tool when all relevant pieces agree:

- GUI command surface and legal transaction mask
- M032 HID bridge master transport
- target-device behavior used by the test flow
- this support matrix
- the validation checklist

Full product compliance still requires target-device firmware behavior, electrical timing checks, bus-level hardware validation, and product-specific PMBus policy review.
