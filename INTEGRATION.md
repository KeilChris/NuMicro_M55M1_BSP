# NuMaker-X-M55M1D BSP 3.1.5-rc.1 integration notes

This source workspace contains the reviewed camera, display, HyperRAM, VIO and
board-layer additions and declares a dependency on NuMicroM55_DFP 3.1.5.
This is a development candidate prepared on 2026-10-02, not a qualified release.
Camera hardening is described in CAMERA-CONTRACT.md. Retain BSP 3.1.4 for rollback.

## Retained behavior

- CMOS-720P V1.0: HM1055 sensor tables, software I2C control, and CCAP
  frame-complete/error callbacks, checked initialization and a bounded graceful
  abort/quarantine contract. A failed abort requires reset/reload.
- NuMaker-TFT-LCD5 V1.2: LT7381 display through EBI, RGB565, polling by default
  (`CONFIG_DISP_USE_PDMA=0`). The optional PDMA path is not an RTX5 adaptation.
- HyperRAM: existing initialization/calibration with startup diagnostics off
  by default. Calibration uses the beginning of the memory window.
- Camera/display remain Board Support APIs. VIO is the existing CMSIS VIO
  interface; no new standard camera/display CMSIS-Driver API is claimed.

Only the camera component changes behavior: Camera 1.2.0, Display 1.1.0,
HyperRAM/VIO 1.0.0. The package version is 3.1.5-rc.1.

## Board-layer integration requirements

`Layers/NuMaker-X-M55M1D/Board.c` supplies a strong `Reset_Handler_PreInit`.
An application must not compile another implementation of that startup hook.
The traffic example already owns the hook in its application board sources;
keep its existing board layer for the first BSP-component migration.

The generic layer is not a complete memory/deployment configuration. Supply a
compatible linker layout and debugger loader. The working traffic example
reserves the first 4 KiB of HyperRAM, loads the model at `0x82001000`, and keeps
startup/calibration code in internal flash. Preserve that reservation and
pre-C-runtime initialization order. HyperRAM is volatile: standalone boot or
recovery after power removal still requires a separate design/debugger reload.

The generic layer's heap declaration and HyperRAM description do not replace
these linker/load requirements. Build a minimal layer example before claiming
the generic layer is self-contained. Avoid duplicate CCAP/PDMA IRQ ownership.

## Preserve the traffic application's working configuration

Keep `TRAFFIC_CAMERA_SERIAL=1`, polling LCD, J13 HS USB with the packaged DFP
driver, and the project-local SDS transport. Do not reintroduce concurrent
capture/inference/LCD transfers during the pack migration. Installing this BSP
alone does not implement the separate ARM SDS transport correction.

## Validation and known limits

Source comparison, XML/file-reference and checksum checks and host camera/event
and display-wrapper regressions are appropriate migration checks. They do not
replace a full CMSIS build, schema/PackChk validation or hardware testing of the
assembled pack.

Before release, assemble the development pack, test a clean installation, build
the application with this BSP and DFP 3.1.5, and repeat:

1. Live camera and LCD: changing scene, no horizontal streaks.
2. Thirty-second SDS recording with every input paired with a detection record.
3. Playback, repeated playback without reset, then recording again.

Record the new pack versions and observed counts/timing. Test disconnect/reset
and long-duration behavior separately; prior successful sessions do not qualify
those cases.

Camera abort/buffer ownership and sensor-write error propagation are implemented
as a candidate and still require hardware qualification. Remaining work includes
bounded LCD waits/rectangle checks, optional-PDMA portability, and public HyperRAM helper bounds.
In particular, `HyperRAM_Erase` is not part of the traffic initialization path
and its inclusive two-byte loop needs a boundary review before general use.

Regenerate `Nuvoton.NuMicro_M55M1_BSP.sha1` whenever pack contents change. It
covers pack files except itself and `.DS_Store`; it is an integrity listing,
not a cryptographic signature or evidence of hardware validation.
