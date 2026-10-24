# HANDOFF.md

## Verified x64 EXE slimming 2026-10-25

- Canonical Release and isolated Debug builds PASS; actual formal EXE hash
  96E1B885B27442B7D89229A1015F488F4FAC6D2CAA59132CF6473AD665FC805D.
- Comparable EXE 3477504 -> 1388032 bytes; required native delivery
  3477504 -> 1388032 bytes.
- Fresh disconnected owned fixtures: identical child-control class counts, native
  file dialog open/cancel, assertion-free Debug and exit 0; five AB/BA pairs PASS.
- Actual clean removed 35 compiler files; 35 protected actual files SHA unchanged.
  Nested 17-file/source fixtures, help/idempotence PASS; root INI hashes preserved.
- Publication commit/remote result is recorded in shared EXE_SLIMMING_PROGRESS.md.
- Raw samples/limits: docs/verification/exe-slimming-20261025.json.
  Physical devices, unrelated firmware, visual/DPI/clean-PC/Win32 NOT RUN.


## Authorized publication preflight (2026-10-21)

- Formatting check: full staged diff --check reports legacy whitespace in imported BSP/library files and three copied M031 boot files; these are not mass-reformatted. The project-owned remainder passes the scoped whitespace check. All six isolated firmware builds completed without errors/warnings.

- The user now authorizes publishing this repository, superseding the earlier deferred-push note. Scope includes the preserved feature work, layout/clean changes, documentation and reviewed native delivery; no unrelated repository is included.
- Build/tests: Debug and Release x64 builds PASS; Debug reported 2 pre-existing unused-variable warnings, Release 0 warnings/errors. Delivery version 1.0.0.258. Isolated native startup/normal-close PASS; PMBus contract validation PASS (224 rows, zero gaps/mismatches).
- Clean PASS: actual scripts/clean_mfc.bat; 35 protected output files unchanged by SHA-256. Expanded fixtures retained 17 delivery/runtime/cache sentinels plus source; object removal, help, repeat clean and reparse rejection PASS.
- Privacy/index: 27 tracked runtime INI/log/Keil-option paths removed from index only; local bytes/hash preserved. New M261 SDK sources/dependencies and required upstream attribution/license included; six vendor library hashes match OpenNuvoton upstream. Unbundled srec_cat.exe and local .uvoptx stay ignored; checksum scripts already support installed SRecord via PATH.
- Firmware/limits: PASS: isolated copies of M032 boot/app and M261 boot/app built with Keil/Arm Compiler 6.22, each 0 errors/warnings; no original project debugger/user files changed. Hardware/flash acceptance NOT RUN.
- Candidate text/EXE checks found no tested private-key, access-token or personal-profile-path patterns; this is not a full secret/license audit. Only ordinary non-force publication to the verified existing main is authorized; exact final commit/remote SHA is recorded in the workspace migration ledger.


## Workspace layout / clean audit (2026-10-21)

- Scope: Moved four screenshots to `docs/images` and updated README links without overwriting files; source/scripts/vendor tree remains intact. Three verified COFF root objects were hash-moved into build/obj/legacy_root then removed by the compiler-only cleaner.
- Build/tests: PASS: `scripts/build_mfc.ps1 -Configuration Release -CloseRunningTool $false`, 0 warnings/errors, normal generated version 1.0.0.256. Shared isolated EXE startup/captioned-frame/normal-close PASS, exit 0; no device connection.
- PASS: shared `verify_clean_preservation.ps1 -ProjectRoot <this-project> -CleanBatch scripts\clean_mfc.bat -CleanActualProject -CheckNestedDelivery -CheckReparseRejection`. Non-destructive help, 17 delivery/runtime sentinels plus source, known object removal, repeat clean, and junction rejection passed; 35 actual protected output files unchanged by SHA-256.
- Additional evidence: The pre-existing private runtime INI hash is unchanged; its values were not read. Firmware/business modifications from before this audit are preserved, not reviewed or staged as migration work.
- Limits/publication: NOT RUN: Keil firmware rebuild, physical bus traffic, protocol acceptance or exhaustive UI/DPI. User confirmed this batch's push is deferred; preserve all uncommitted feature work and local results.


## Current State

The PC GUI builds as `build\PmbusSmbusHidTool.exe` and exposes `PMBus`, `SMBus`, `I2C`, and `Script` tabs with M032 EVB I2C port and pin-pair selection where bus ownership is needed.

Firmware upload was split out to the sibling `simple_programming_tool` workspace. This workspace should stay focused on PMBus/SMBus validation and generic I2C bridge testing.

## Implemented

- HID defaults: VID `0x0416`, PID `0x5020`.
- PMBus/SMBus GUI scope is master mode only.
- I2C port selection supports I2C0/I2C1 and M032 pin-pair dropdowns.
- I2C tab supports master write/read/write-read and simple slave TX/RX validation.
- Script editing is centralized in the `Script` tab using the M032 CSV format.
- PMBus and SMBus tabs have script `Load`/`Run`, row checkboxes, select-all, response panels, progress, repeat controls, and stop/cancel behavior.
- M032 bridge firmware supports the PMBus/SMBus/I2C HID command protocol and reports `m032-pmbus-smbus-bridge/1.1.3`.
- The FW Upload tab, Nuvoton I2C ISP PC helper, and M031 ISP target samples were moved to `simple_programming_tool`.
- The PMBus/SMBus bridge firmware no longer accepts the FW Upload final-byte NACK staged-write exception.

## Important Files

- PC entry: `src\main.cpp`, `src\app.cpp`, `src\main_frame.cpp`
- PMBus tab: `src\ui\pmbus_tab.cpp`
- SMBus tab: `src\ui\smbus_tab.cpp`
- I2C tab: `src\ui\i2c_tab.cpp`
- Script tab: `src\ui\script_tab.cpp`
- Script parser/serializer: `src\core\smbus_script.cpp`
- I2C pin catalog: `src\core\board_i2c_catalog.h`
- HID command constants: `src\core\bridge_commands.h`
- Firmware HID dispatcher: `demo_code\M031BSP_USB_HID_PMBus_SMBus\SampleCode\Template\hid_tool_api.c`
- Firmware I2C bridge: `demo_code\M031BSP_USB_HID_PMBus_SMBus\SampleCode\Template\m031_bridge_i2c.c`
- Firmware version: `demo_code\M031BSP_USB_HID_PMBus_SMBus\SampleCode\Template\bridge_version.h`

## Verification Done

- PC Release x64 build after FW Upload removal and README inventory update: PASS, build version `1.0.0.247`, 0 warnings, 0 errors.
- PMBus contract validation: PASS.
- Firmware ARM GCC syntax-only check for `m031_bridge_i2c.c` and `hid_tool_api.c`: PASS.
- Keil full M032 bridge firmware build: NOT RUN, Keil `UV4`/`armclang` command line is not available in this shell.
- Hardware PMBus/SMBus/I2C validation: use the documented button/script/manual smoke tests.

## Next Recommended Work

1. Build and flash the M032 bridge firmware version `1.1.3`.
2. Launch `build\PmbusSmbusHidTool.exe`, connect HID, and verify `Get Info` reports `m032-pmbus-smbus-bridge/1.1.3`.
3. Re-run PMBus Base, M-CRPS, TI UCD90xxx, SMBus Generic, SMBus UBM button/script smoke tests.
4. Re-run generic I2C master/slave smoke tests.
5. Use `simple_programming_tool` for Nuvoton target firmware upload work.
