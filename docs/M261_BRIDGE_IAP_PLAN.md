# M261 HID Bridge And IAP Plan

## Problem

`demo_code/M261BSP_USB_HID_PMBus_SMBus` has been created from the M261 BSP, but its Template project is still a basic timer/UART sample. It does not yet match the existing `M031BSP_USB_HID_PMBus_SMBus` HID bridge behavior used by the PC tool.

The M261 work also needs a USB HID IAP bootloader for `M263KIAAE` so the bridge firmware can be updated through HID in the future. The target flash is 512 KB, with the first 64 KB reserved for the bootloader and the remaining APROM space reserved for the bridge application.

## Assumptions

- The existing HID bridge protocol remains unchanged: 64-byte HID reports, `0xA5` magic, and the existing 6-byte bridge header.
- The M261 bridge application should remain PC-tool compatible with the existing VID/PID unless a later board-profile change explicitly assigns a new PID.
- The first M261 application image base is `0x00010000`.
- The M261 bootloader occupies APROM `0x00000000` through `0x0000FFFF`.
- The M261 application occupies APROM `0x00010000` through `0x0007FFFF`.
- The existing `SampleCode/ISP_HID` under the M261 project is the starting point for the HID IAP bootloader.
- PMBus/SMBus bridge behavior must stay separate from target-board FW upload behavior in this workspace.

## Missing Information

- Exact M261 EVB I2C pins exposed to the user connector should be confirmed against the EVB schematic.
- The PC GUI now has `M032 EVB` and `M261 EVB` board/pin profiles. Practical M261 EVB use still needs hardware validation against the EVB schematic and real target wiring.
- Final hardware validation on a M261 EVB is still required. Keil/uVision build has been run locally with ARM Compiler 6.22.

## Current Behavior

- `M031BSP_USB_HID_PMBus_SMBus/SampleCode/Template` contains the complete PMBus/SMBus HID bridge:
  - bridge protocol headers
  - HID transfer/descriptors
  - HID dispatcher
  - M031/M032 I2C backend
  - Keil project groups for `i2c.c` and `usbd.c`
- `M261BSP_USB_HID_PMBus_SMBus/SampleCode/Template` now contains the staged M261 PMBus/SMBus HID bridge application and is linked at `0x00010000`.
- `M261BSP_USB_HID_PMBus_SMBus/SampleCode/ISP_HID` now follows the APROM IAP flow: bootloader at `0x00000000`, application at `0x00010000`, and application checksum gate before jump.

## Target Behavior

### M261 Bridge Application

- Keep the same bridge command protocol as the M031 bridge.
- Add a M261-specific I2C backend instead of reusing M031 pin mux code directly.
- Use the M261 USB HID device stack and HIRC48 trim flow.
- Report bridge info as an M261 bridge string so the PC tool can distinguish the hardware.
- Link the bridge application at `0x00010000` for the IAP layout.

### M261 HID IAP Bootloader

- Reserve APROM `0x00000000` to `0x0000FFFF` for bootloader code.
- Jump/run application from `0x00010000` only after the application exists and CRC32 passes.
- Reject APROM update and erase requests that target the bootloader region.
- Report usable application size as 448 KB (`0x70000`) to the ISP host.
- Use a Keil scatter file to constrain bootloader image placement to 64 KB.
- Do not use `DetectPin` as a bootloader entry control.
- Use M261 EVB LED `PB10` as a bootloader status indicator.
- Keep the shared SRAM mailbox check for compatibility, but the bridge application forces bootloader update mode by erasing the application CRC word before reset.
- The PMBus/SMBus tool top-level `Return Bootloader` button is the app-side entry point. It sends `BRIDGE_CMD_ENTER_IAP` while connected to the bridge app HID device. The standalone `simple_programming_tool` stays NuvoISP-style and only connects after the bootloader is already enumerated.

### M261 Bootloader LED Indicator

- `PB10` is active low.
- App checksum check: LED on.
- Valid app jump: LED off before vector remap and CPU reset.
- Invalid or missing app / waiting for HID update: 3 short blinks, 250 ms on/off per step, then LED off for 1 second, repeated.
- HID packet received while waiting for update: short LED pulse.

### M261 Bridge Application Post-Build

- Link the bridge application at `0x00010000` with size `0x70000`.
- Use Keil after-build command `generateChecksum.bat`.
- Use local `srec_cat.exe` to fill unused application space with `0xFF`.
- Write CRC32 little-endian at `0x0007FFFC`.
- Emit `obj\Template.bin` as the padded 448 KB HID IAP update image.
- Emit `boot_app.hex` and `boot_app.bin` when `ISP_HID.hex` is available.

## Flash Map

| Region | Start | Size | Purpose |
| --- | ---: | ---: | --- |
| Bootloader | `0x00000000` | `0x10000` | USB HID IAP |
| Application | `0x00010000` | `0x70000` | PMBus/SMBus HID bridge |
| Runtime SRAM | `0x20000000` | `0x17FC0` | Bootloader/application RAM |
| IAP mailbox | `0x20017FC0` | `0x40` | App-to-bootloader enter-IAP request |

## Implementation Approach

1. Copy the M031 bridge protocol and HID dispatcher files into the M261 Template project.
2. Create `m261_bridge_i2c.c/h` from the M031 backend, then replace the pin validation and pin mux layer with M261-specific code.
3. Update M261 Template `main.c` to initialize USB HID, run the bridge dispatcher, keep the timer tick service, and use M261 HIRC48 USB trim handling.
4. Update M261 Template Keil project:
   - add bridge source files
   - add `i2c.c` and `usbd.c`
   - set application IROM to `0x00010000` / `0x70000`
   - reserve the top 64 bytes of SRAM for mailbox compatibility
   - add `generateChecksum.bat` and local `srec_cat.exe` post-build flow
5. Update M261 ISP_HID:
   - add flash layout constants
   - validate application vector and CRC32 before jumping to `0x00010000`
   - check and clear the SRAM mailbox before checksum jump for compatibility
   - make `CMD_RUN_APROM`, `CMD_RUN_LDROM`, and `CMD_RESET` reset back through the bootloader checksum gate
   - protect `0x00000000` to `0x0000FFFF` from erase/program
   - add a 64 KB bootloader scatter file and wire it into the Keil project
   - add `crc.c` to support hardware CRC32
6. Update README notes so users know M261 support is staged and requires the M261 HID IAP + application split.

## Implementation Status

- M261 Template now uses the same HID bridge protocol files as the M031/M032 bridge.
- M261 Template initializes USB HID, keeps the timer service, uses HIRC48 USB trim handling, and dispatches bridge commands in the main loop.
- M261-specific I2C backend is provided by `m261_bridge_i2c.c/h`.
- M261 Template Keil project links the bridge application at `0x00010000` with `0x70000` bytes of application space.
- M261 Template Keil project reserves SRAM `0x20017FC0..0x20017FFF` for the IAP mailbox.
- M261 Template bridge protocol includes `BRIDGE_CMD_ENTER_IAP` (`0x04`), which erases the application CRC word at `0x0007FFFC` and resets the MCU after returning an OK HID response.
- M261 Template Keil post-build runs `generateChecksum.bat`, pads the app region to 448 KB, and writes CRC32 at `0x0007FFFC`.
- Local `srec_cat.exe` is included beside the Template Keil project for the checksum/padding flow.
- M261 `ISP_HID` uses `iap_layout.h` for the 64 KB bootloader and 448 KB application split.
- M261 `ISP_HID` checks the mailbox first for compatibility. The current bridge app enter-IAP path relies on the erased CRC word, so the bootloader stays in USB HID update mode after reset.
- M261 `ISP_HID` checks application vector sanity and CRC32 before running `0x00010000`; invalid or missing app stays in USB HID update mode.
- M261 `ISP_HID` does not use `DetectPin`.
- M261 `ISP_HID` drives PB10 LED for checksum, waiting-for-update, HID activity, and app-jump indication.
- M261 `ISP_HID` reports the application region as usable APROM and rejects erase/program operations outside that application range.
- M261 `ISP_HID` handles `CMD_RUN_APROM`, `CMD_RUN_LDROM`, and `CMD_RESET` by resetting back to the bootloader checksum gate.
- M261 `ISP_HID` Keil project uses `m261_iap_hid_64k.sct` to constrain bootloader placement.
- PC GUI includes a top-level `Board` selector for `M032 EVB` and `M261 EVB`; the M261 profile uses the same pin-code encoding and omits the M032-only `I2C1 PB.2/PB.3` pair.
- README documents M261 as staged firmware support and calls out the remaining M261 EVB hardware-validation work.

## Verification

- `Template.uvprojx` XML parse: PASS.
- `ISP_HID.uvprojx` XML parse: PASS.
- M261 IRAM reservation scan: PASS, both Keil projects use `0x17FC0` runtime RAM and reserve the top 64 bytes for mailbox.
- M261 residue scan for known M031/M032-only symbols: PASS.
- M261 `DetectPin` residue scan under `SampleCode\ISP_HID`: PASS.
- Keil build, target `Template`: PASS, `0 Error(s), 0 Warning(s)`.
- Template SRecord post-build: PASS, CRC word bytes at app offset `0x6FFFC` are `AD E0 79 50`.
- `obj\Template.bin`: PASS, 458,752 bytes (`0x70000`), padded app image with CRC.
- `boot_app.bin`: PASS, 524,288 bytes (`0x80000`), full bootloader-plus-application image.
- Keil build, target `ISP_HID`: PASS, `0 Error(s), 0 Warning(s)`, `ISP_HID.bin` size 6,104 bytes.
- `generateChecksum.bat` after bootloader rebuild: PASS, regenerated `boot_app.bin` with the latest `ISP_HID.hex`.
- MFC GUI build after adding M261 board/pin profile: PASS, Release x64, `0 Warning(s), 0 Error(s)`.
- The bootloader still links and fits well inside 64 KB after adding the LED indicator.
- Hardware validation on a M261 EVB: NOT RUN.

## Risks

- If the user loads an old INI containing `I2C1_PB2_PB3` while `M261 EVB` is selected, the GUI falls back to the first valid M261 I2C1 pair before sending init commands.
- M261 MFP function numbers must match the BSP headers and actual package pinout.
- HID IAP host tooling must send `obj\Template.bin`, not `Template.raw.bin`. Images built for `0x00000000` or images without the CRC word at `0x0007FFFC` will stay in bootloader mode.
- `BRIDGE_CMD_ENTER_IAP` intentionally invalidates the current application image by erasing the CRC word. If the user cancels programming after entering bootloader mode, the board remains in HID ISP mode until a valid application image is programmed.
- Bootloader size must be checked after Keil build. The scatter file enforces the 64 KB limit, but the final map file is the source of truth.
