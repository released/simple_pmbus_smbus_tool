# SELF_CHECK.md

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
