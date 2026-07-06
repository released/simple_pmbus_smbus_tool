# PMBus Command Contract

Last updated: 2026-06-27

## Purpose

`PMBUS_COMMAND_CONTRACT.csv` is the synchronization table for PMBus command support across:

- MFC GUI PMBus preset names, legal transactions, decode format, and checklist behavior
- M032 EVB HID bridge transport support for PMBus/SMBus master transactions
- Target-device behavior notes used by validation flows
- `docs/PMBUS_SUPPORT_MATRIX.md`

The bridge board is the M032 EVB in the current phase. Future M487 EVB support should add a board catalog and firmware pin-mux implementation without changing the PMBus/SMBus command model.

Normative references:

- `docs/PMBus-Specification-Rev-1-3-1-Part-I-20150313.pdf`, general requirements, transport, electrical, side-band, and SMBus/PMBus extension behavior
- `docs/PMBus-Specification-Rev-1-3-1-Part-II-20150313.pdf`, Appendix I / Table 31
- `docs/PMBus-Specification-Rev-1-3-1-Part-III-20150313.pdf`, AVSBus reference only; AVSBus is not implemented by the current PMBus/SMBus tool or firmware bridge
- `docs/M-CRPS_Base_Specification_version_1p06p00_RC1-draft7_042026.pdf`, CRPS profile overlay rows promoted into the CRPS profile surface
- `docs/UCD90xxx Sequencer and System Health Controller PMBus Command Reference.pdf`, TI vendor profile reference

## Current Counts

- Table 31 non-reserved rows tracked: `224`
- Target behavior rows marked implemented or intentionally host-visible: `224`
- Target policy gaps: `0`
- Host-visible shadows/placeholders: `168`
- Concrete/non-placeholder behavior rows: `56`
- GUI/spec name mismatches currently flagged: `0`

## Status Rules

- `gui_status=Preset` means the GUI exposes an explicit preset.
- `gui_status=RangePreset` means the GUI exposes the command through a generated range preset.
- `gui_status=ExtendedMode` means the GUI supports the command only through the extended-command UI flow.
- `bridge_transport_status=TransportReady` means the M032 HID bridge can generate the SMBus/PMBus master transaction form. It does not prove target-device semantics.
- `device_model_status=Implemented` means the command has a documented target behavior expectation for GUI validation.
- `product_binding_status=HostVisibleShadowOrPlaceholder` means the command has stable host-visible PMBus behavior and is intentionally left as an extension hook for later ADC/GPIO/NVM/control/product-policy binding.
- `device_model_status=PolicyGap` is not allowed for the current command baseline; user/manufacturer/extended namespace rows must have an explicit placeholder/shadow or product policy before being marked implemented.
- `decode_format=VoutModeAwareWord` means the GUI decodes the word using the cached raw `VOUT_MODE` Table 2 selector:
  - ULINEAR16 is numerically decoded with the selected exponent.
  - Direct and IEEE half are decoded/encoded at the PMBus format layer. VID selector `1Eh/1Fh` is validated and retained as a right-justified raw code until a product VID table is assigned.

## Profile Policy

The GUI uses one `PMBus` tab with a `Profile` combo.

- `PMBus Base` exposes public PMBus command names, including `USER_DATA_00..15` and generic `MFR_SPECIFIC_C0..FD` names from the PMBus specification.
- `M-CRPS` overlays public M-CRPS command names from `docs/M-CRPS_Base_Specification_version_1p06p00_RC1-draft7_042026.pdf`.
- `TI UCD90xxx` overlays command names and default transaction hints from `docs/UCD90xxx Sequencer and System Health Controller PMBus Command Reference.pdf`.
- Profile selection controls preset labels, preset visibility, default transaction hints, decode policy, checklist scope, and debug/display names.
- The selected profile is persisted in INI and restored on launch.

The MFC GUI preset/legal-transaction include `src/core/pmbus_contract.generated.inc` is generated from this CSV and must not be edited by hand.

## Required Workflow For Future PMBus Command Changes

1. Update `docs/PMBUS_COMMAND_CONTRACT.csv` first.
2. Update GUI preset/decode/checklist only if the contract says the command is exposed or validated.
3. Update target-device behavior notes only when the expected behavior is defined.
4. Keep placeholders marked as placeholders and preserve clear TODO/extension-hook notes until a product owner binds them to real behavior.
5. Re-run the contract validation command after every PMBus command change.
6. Treat a stale generated include as a validation failure; regenerate it with `scripts\generate_pmbus_contract_header.ps1` or by running the normal MFC build.

## Validation Command

```powershell
powershell -ExecutionPolicy Bypass -File scripts\validate_pmbus_contract.ps1
```
