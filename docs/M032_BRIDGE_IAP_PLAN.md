# M032 HID Bridge IAP Plan

## Problem

The M032 bridge firmware previously built as a single APROM image at `0x00000000`. To support future HID firmware update of the M032 bridge board itself, the firmware now needs the same boot/application update flow already implemented for M261, with a smaller M032 flash map.

This is not target-board FW upload. Target programming workflows remain in the sibling `simple_programming_tool` project.

## Assumptions

- Target device is `M032SE3AE` with 128 KB APROM and 16 KB SRAM.
- The bootloader occupies the first 16 KB of APROM.
- The bridge application starts at `0x00004000`.
- The HID bridge protocol remains unchanged: 64-byte HID reports, `0xA5` magic, and the existing 6-byte bridge header.
- `BRIDGE_CMD_ENTER_IAP` (`0x04`) is a core bridge command and does not affect existing PMBus, SMBus, or I2C command IDs.

## Flash Map

| Region | Start | Size | Purpose |
| --- | ---: | ---: | --- |
| Bootloader | `0x00000000` | `0x4000` | USB HID IAP |
| Application | `0x00004000` | `0x1C000` | PMBus/SMBus HID bridge |
| Runtime SRAM | `0x20000000` | `0x3FC0` | Bootloader/application RAM |
| IAP mailbox | `0x20003FC0` | `0x40` | App-to-bootloader enter-IAP request |

## Target Behavior

- Bootloader checks the shared SRAM mailbox first for compatibility.
- If the mailbox contains a valid enter-IAP request, the bootloader clears it and stays in USB HID update mode.
- If no request exists, the bootloader verifies the application vector and CRC32 before jumping to `0x00004000`.
- If the application is invalid, missing, or has a bad CRC, the bootloader stays in USB HID update mode.
- ISP erase/program commands are restricted to the application region and cannot erase the bootloader region.
- The bridge app handles `BRIDGE_CMD_ENTER_IAP` by erasing the application CRC word and resetting through the bootloader vector path.
- The PMBus/SMBus tool top-level `Return Bootloader` button is the app-side entry point. It sends `BRIDGE_CMD_ENTER_IAP` while connected to the bridge app HID device. The standalone `simple_programming_tool` stays NuvoISP-style and only connects after the bootloader is already enumerated.
- The Template post-build flow pads the app region with `0xFF`, calculates CRC32, writes it at `0x0001FFFC`, and emits both update-only and merged images.

## Implementation Status

- `SampleCode\iap_mailbox.h` defines the shared 64-byte M032 mailbox.
- `SampleCode\ISP_HID\iap_layout.h` defines the 16 KB bootloader / 112 KB application split.
- M032 `ISP_HID` checks mailbox, validates vector and CRC32, and jumps through `FMC_SetVectorPageAddr(0x00004000)`.
- M032 `ISP_HID` no longer uses `DetectPin` as a bootloader entry control.
- M032 `ISP_HID` handles `CMD_RUN_APROM`, `CMD_RUN_LDROM`, and `CMD_RESET` by resetting through the bootloader checksum gate.
- M032 `ISP_HID` reports the application region as usable APROM and rejects erase/program operations outside that application range.
- M032 `ISP_HID` drives PB14 for checksum, waiting-for-update, HID activity, and app-jump indication.
- M032 Template links at `0x00004000` with `0x1C000` bytes of application space.
- M032 Template reserves SRAM `0x20003FC0..0x20003FFF` for the IAP mailbox.
- M032 Template bridge protocol includes `BRIDGE_CMD_ENTER_IAP` (`0x04`), which erases the application CRC word at `0x0001FFFC` and resets after returning an OK HID response.
- M032 Template Keil post-build runs `generateChecksum.bat`, pads the app region to 112 KB, and writes CRC32 at `0x0001FFFC`.
- Local `srec_cat.exe` is included beside the Template Keil project for the checksum/padding flow.

## Verification

- `ISP_HID.uvprojx` XML parse: PASS.
- `Template.uvprojx` XML parse: PASS.
- M032 IRAM reservation scan: PASS, both Keil projects use `0x3FC0` runtime RAM and reserve the top 64 bytes for mailbox.
- M032 `DetectPin` residue scan under `SampleCode\ISP_HID`: PASS.
- Keil build, target `ISP_HID`: PASS, `0 Error(s), 0 Warning(s)`.
- `ISP_HID.bin`: PASS, 8,400 bytes, fits inside the 16 KB bootloader region.
- Keil build, target `Template`: PASS, `0 Error(s), 0 Warning(s)`.
- Template SRecord post-build: PASS, CRC word bytes at app offset `0x1BFFC` are `5B C3 3A 65`.
- `obj\template.bin`: PASS, 114,688 bytes (`0x1C000`), padded app image with CRC.
- `boot_app.bin`: PASS, 131,072 bytes (`0x20000`), full bootloader-plus-application image.
- Hardware validation on a M032 EVB: NOT RUN.

## Risks

- HID IAP host tooling must send `obj\template.bin`, not `template.raw.bin`. Images built for `0x00000000` or images without the CRC word at `0x0001FFFC` will stay in bootloader mode.
- `BRIDGE_CMD_ENTER_IAP` intentionally invalidates the current application image by erasing the CRC word. If the user cancels programming after entering bootloader mode, the board remains in HID ISP mode until a valid application image is programmed.
- The app reset path uses VECMAP-to-bootloader plus CPU reset so the bootloader checksum gate runs immediately after the OK response is returned.
- Bootloader size must be checked after each build. The Keil IROM size enforces the 16 KB region, but the build log and map file are the source of truth.
