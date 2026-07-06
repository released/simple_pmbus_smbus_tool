# AGENTS.md

## Project Purpose

This project builds a PMBus/SMBus HID test tool:

- M032 EVB firmware exposes a USB HID bridge.
- The EVB I2C controller acts as PMBus/SMBus host.
- The Windows MFC GUI provides PMBus and SMBus tabs for testing existing PMBus/SMBus device projects.

Future MCU targets may include M48X series, so keep bridge protocol and GUI state portable.

## Local Rules

- Keep the project layout aligned with `simple_hid_test_tool` and `simple_joystick_test_tool`:
  - `src\`
  - `scripts\`
  - `docs\`
  - `demo_code\`
  - root `README.md`, `AGENTS.md`, `SELF_CHECK.md`, `HANDOFF.md`
- Make the smallest safe change.
- Do not refactor copied PMBus/SMBus GUI code unless the task requires it.
- Preserve the HID bridge report format: 64-byte report, `0xA5` magic, 6-byte header.
- Keep firmware C compatible with C90 style where practical:
  - declare variables at the beginning of blocks
  - use English comments
  - avoid C++ comments in new firmware code

## PC Tool

- Solution: `PmbusSmbusHidTool.sln`
- Project: `PmbusSmbusHidTool.vcxproj`
- Build script:

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build_mfc.ps1 -Configuration Release -Platform x64
```

- Output: `build\PmbusSmbusHidTool.exe`
- Visible GUI scope is PMBus, SMBus, I2C, Script, and HID connection controls.
- PMBus and SMBus GUI scope is master mode only in the current phase.
- Firmware upload is intentionally out of scope for this workspace. Use the sibling `simple_programming_tool` project for target programming workflows.

## Firmware

- Keil project: `demo_code\M031BSP_USB_HID_PMBus_SMBus\SampleCode\Template\Keil\Template.uvprojx`
- Bridge entry point: `hid_tool_api.c`
- M031 I2C implementation: `m031_bridge_i2c.c`
- Version header: `bridge_version.h`
- HID VID/PID are in `hid_transfer.h`.

Current firmware bridge supports:

- Ping
- Get Info
- Reset MCU
- I2C master init/deinit
- I2C master write/read/write-read
- staged write/read helpers
- PMBus block read
- PMBus group write
- bus status/recovery
- SMBus Quick

PMBus/SMBus master and generic I2C master/slave workflows are in the current GUI scope. Do not add target ISP or FW upload behavior back into this workspace's M032 bridge firmware.

## Verification Expectations

For every code change, report:

1. Changed files
2. What changed
3. Why it changed
4. How to verify
5. Actual verification result, or NOT RUN with reason
