# Bridge HID IAP Multi-Tool Rollout

## Purpose

This note records the reusable M031/M032 and M261 bridge HID IAP architecture and the work needed to roll the same firmware-update model into sibling GUI tool projects.

The bootloader should stay functionally stable across tools. The application image must change per tool because each GUI tool exposes a different bridge feature set.

## Current Alignment Status

`simple_pmbus_smbus_tool` now has aligned M031/M032 and M261 boot/application flows:

| Item | M031BSP / M032 EVB | M261 EVB / M263KIAAE |
| --- | --- | --- |
| Bootloader path | `demo_code\M031BSP_USB_HID_PMBus_SMBus\SampleCode\ISP_HID` | `demo_code\M261BSP_USB_HID_PMBus_SMBus\SampleCode\ISP_HID` |
| Application path | `demo_code\M031BSP_USB_HID_PMBus_SMBus\SampleCode\Template` | `demo_code\M261BSP_USB_HID_PMBus_SMBus\SampleCode\Template` |
| Bootloader region | `0x00000000..0x00003FFF` (`16 KB`) | `0x00000000..0x0000FFFF` (`64 KB`) |
| Application region | `0x00004000..0x0001FFFF` (`112 KB`) | `0x00010000..0x0007FFFF` (`448 KB`) |
| CRC word | `0x0001FFFC` | `0x0007FFFC` |
| Runtime SRAM | `0x20000000..0x20003FBF` | `0x20000000..0x20017FBF` |
| IAP mailbox | `0x20003FC0..0x20003FFF` | `0x20017FC0..0x20017FFF` |
| Bootloader USB HID | `VID 0x0416`, `PID 0x3F00` | `VID 0x0416`, `PID 0x3F00` |
| Application USB HID | `VID 0x0416`, `PID 0x5020` | `VID 0x0416`, `PID 0x5020` |
| Bootloader LED | `PB14`, active low | `PB10`, active low |
| App entry command | `BRIDGE_CMD_ENTER_IAP` (`0x04`) | `BRIDGE_CMD_ENTER_IAP` (`0x04`) |

The implementation is aligned at the behavior level:

- Bootloader checks an app-to-bootloader mailbox for compatibility.
- Bootloader validates application vector and CRC before jumping to app.
- Invalid, missing, or bad-CRC app stays in USB HID ISP mode.
- ISP erase/program commands reject the bootloader region.
- `CMD_RUN_APROM`, `CMD_RUN_LDROM`, and `CMD_RESET` return through the bootloader checksum gate.
- No `DetectPin` boot strap is used.
- The application-side `BRIDGE_CMD_ENTER_IAP` erases the app CRC word, returns OK, and resets through the bootloader path.
- Template post-build pads the application region with `0xFF`, writes CRC32 at the last word, and can generate a full `boot_app.bin`.

The remaining differences are expected MCU differences: APROM size, SRAM size, bootloader size, application base, CRC address, LED pin, and peripheral driver details.

## Shared Bootloader Contract

When porting to another tool project, keep these bootloader behaviors unchanged unless a new MCU requires a documented hardware-specific change:

1. Enumerate the bootloader as USB HID `VID 0x0416`, `PID 0x3F00`.
2. Accept the NuvoISP-style HID command flow used by `simple_programming_tool`.
3. Treat ISP APROM update offset `0` as the configured application base.
4. Prevent erase/program operations from touching the bootloader region.
5. Verify application vector and CRC before app jump.
6. Stay in HID ISP mode when the application is missing or invalid.
7. Do not rely on `DetectPin`.
8. Keep the status LED pattern:
   - checksum check: LED on
   - valid app jump: LED off before vector remap and reset
   - invalid/missing app or waiting for HID update: 3 short blinks, 250 ms high/low per step, then 1 second off, repeated
   - HID packet activity while waiting: short pulse

The bootloader does not need to know whether the application is PMBus, SMBus, SGPIO, or a dashboard tool. It only needs a valid app image at the configured application base with a valid CRC word at the configured end address.

## Application Porting Contract

Each GUI tool needs its own application firmware, but every application should carry the same IAP entry contract:

1. Link the app at the MCU-specific application base:
   - M031/M032: `0x00004000`
   - M261/M263KIAAE: `0x00010000`
2. Reserve the top 64 bytes of SRAM for the shared IAP mailbox.
3. Include the matching `SampleCode\ISP_HID\iap_layout.h` constants.
4. Expose the tool-specific USB HID app VID/PID expected by that GUI.
5. Implement `BRIDGE_CMD_ENTER_IAP` or an equivalent app-side command:
   - erase only the application CRC word
   - return an OK HID response
   - reset through the bootloader vector path
6. Keep the Keil post-build checksum flow:
   - generate raw app binary
   - pad unused app space with `0xFF`
   - write CRC32 little-endian at the app CRC address
   - emit the padded app update binary
   - optionally emit merged `boot_app.hex` and `boot_app.bin`
7. Keep app feature code project-specific. Do not copy PMBus/SMBus command handlers into SGPIO-only projects unless that tool explicitly needs them.

The PC GUI for each tool should provide the app-side `Return Bootloader` command only if it connects to the running application HID device. `simple_programming_tool` should stay bootloader-only and should not need to know application VID/PID values.

## Target Project Inventory

### `simple_pmbus_gui_tool`

Observed state:

- `D:\SourceCode\python\simple_pmbus_gui_tool\demo_code` currently only has `README.md`.
- No M031/M032 firmware folder exists.
- No M261 firmware folder exists.

Needed work:

- Add M031/M032 bootloader folder based on the aligned `ISP_HID` flow.
- Add M031/M032 PMBus GUI bridge application linked at `0x00004000`.
- Add M261 bootloader folder based on the aligned `ISP_HID` flow.
- Add M261 PMBus GUI bridge application linked at `0x00010000`.
- Keep the application behavior focused on the PMBus GUI tool scope; do not import unrelated SMBus/SGPIO test flows unless the product scope changes.
- Add a GUI-side `Return Bootloader` command if the tool owns the running bridge app HID connection.

### `simple_sgpio_test_tool`

Observed state:

- `D:\SourceCode\python\simple_sgpio_test_tool\demo_code\M031BSP_USB_HID_SGPIO` exists.
- It currently has the SGPIO Template application shape, but no aligned `ISP_HID` bootloader folder.
- No M261 boot/application folder exists.

Needed work:

- Add M031/M032 `ISP_HID` bootloader and convert the existing SGPIO Template application to the `0x00004000` app layout.
- Add M031/M032 checksum post-build and merged image generation.
- Add app-side enter-IAP command to the SGPIO HID command protocol.
- Add M261 `ISP_HID` bootloader and M261 SGPIO Template application linked at `0x00010000`.
- Keep SGPIO pin and command behavior from this project; only the IAP shell should be shared.
- Add a GUI-side `Return Bootloader` command if the tool owns the running SGPIO app HID connection.

### `simple_smbus_sgpio_tool`

Observed state:

- `D:\SourceCode\python\simple_smbus_sgpio_tool\demo_code\M031BSP_USB_HID_SGPIO` exists.
- It currently has the combined Template application shape, but no aligned `ISP_HID` bootloader folder.
- No M261 boot/application folder exists.

Needed work:

- Add M031/M032 `ISP_HID` bootloader and convert the existing combined Template application to the `0x00004000` app layout.
- Add M031/M032 checksum post-build and merged image generation.
- Add app-side enter-IAP command to the combined SMBus/SGPIO HID command protocol.
- Add M261 `ISP_HID` bootloader and M261 combined SMBus/SGPIO Template application linked at `0x00010000`.
- Preserve the combined project command surface: SMBus behavior from `simple_pmbus_smbus_tool`, SGPIO behavior from `simple_sgpio_test_tool`.
- Add a GUI-side `Return Bootloader` command if the tool owns the running combined app HID connection.

## Recommended Rollout Order

1. Treat `simple_pmbus_smbus_tool` as the reference implementation for M031/M032 and M261 IAP layout.
2. For each target project, first port only M031/M032:
   - add `ISP_HID`
   - move app linker base
   - add post-build checksum
   - add app-side enter-IAP command
   - build and program through `simple_programming_tool`
3. After M031/M032 passes, add the M261 bootloader/application pair for the same tool.
4. Only after firmware passes, add the GUI `Return Bootloader` button for that tool.
5. Re-test the original non-IAP tool features after the app layout change, because moving app base and SRAM reservation can expose hidden assumptions.

## Verification Checklist

Run this checklist for each tool and MCU pair:

- `ISP_HID.uvprojx` XML parse: PASS.
- Template application Keil project XML parse: PASS.
- Bootloader build: `0 Error(s), 0 Warning(s)`.
- Bootloader binary fits inside its configured bootloader region.
- Application build: `0 Error(s), 0 Warning(s)`.
- Application update binary size equals the configured application region size.
- CRC word exists at the final 4 bytes of the padded app image.
- Merged `boot_app.bin` size equals the full APROM size.
- Bootloader with erased/invalid app stays in HID ISP mode.
- Programming the padded app binary through `simple_programming_tool` succeeds.
- After programming, bootloader jumps to app.
- GUI connects to the running app HID device.
- GUI `Return Bootloader` invalidates the app CRC and causes the bootloader PID `0x3F00` to enumerate.
- Original tool feature smoke test still passes.

## Risks

- Copying the bootloader alone is not enough. The app must be linked at the new base and must generate a padded CRC image.
- `simple_programming_tool` cannot send app-side return commands because it intentionally does not know application VID/PID values.
- Canceling after `Return Bootloader` leaves the app CRC erased; the board will remain in bootloader mode until a valid app is programmed.
- Each application may have different HID app PID, report command IDs, and endpoint usage. Keep the bootloader shared, but review the app-side command namespace per project.
- SRecord paths and Keil post-build commands are easy to break when folder names differ. Verify from the generated binary size and final CRC word, not only from build success.
