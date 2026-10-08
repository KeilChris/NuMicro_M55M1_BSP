# Camera component 1.2.0 — development candidate

BSP 3.1.5-rc.1 requires DFP 3.1.5. Only the camera component changes behavior.
The sensor tables, sensor clock, board pin routing, LCD and HyperRAM code remain
unchanged. Keep serialized capture, LCD polling and the existing USB/SDS transport.

`ImageSensor.h` defines `IMAGE_SENSOR_CAPTURE_CONTRACT_VERSION=2` and these rules:

- One foreground caller owns the API. Callbacks run in the CCAP IRQ and must not
  reenter capture/configuration/abort operations.
- A nonzero, 32-byte-aligned DMA-accessible buffer must have enough capacity for
  the configured output and whole cache-line padding. Caller owns cache cleaning
  before capture and invalidation after successful completion.
- Trigger rejects overlapping capture. A frame callback alone does not release
  the buffer: Poll/WaitCaptureDone must report successful completion first.
- PollCaptureDone returns BUSY until one-shot capture disables itself, then OK
  for a complete frame or ERROR for a stopped invalid frame. ERROR frames must
  not be processed. Sticky FIFO overflow prevents further captures.
- AbortCapture requests a graceful frame-boundary stop. OK discards/releases the
  frame. TIMEOUT/FAULT retains ownership and quarantines the buffer; do not reuse
  it or start another capture until a system reset/debugger reload.
- The poll limit is an iteration budget, not milliseconds. Abort performs at
  most limit + 1 stop checks. WaitCaptureDone uses a bounded wait followed, on
  timeout, by one bounded abort wait.
- Sensor initialization fails at the first table-write or checked-ID-read NACK.
  The software-I2C byte helper's legacy nonzero-for-ACK convention is retained.
  No hardware-I2C, clock-stretch recovery or live-module-removal support is added.

This is a vendor Board Support API, not a standardized CMSIS camera driver.
Only RGB565 416x416 is the current application integration target. Unsupported
1-bit output and invalid sizes/strides are rejected before hardware setup.

Evidence: M55M1 TRM Rev 1.02, section 6.25, pp.1670-1671, 1679-1680, 1683-1684,
1692 (documentation id `user/nuvoton/numicrom55-dfp/en-us-trm-m55m1-series-en-rev1-02`).
SHUTTER disables capture after a frame; the manual does not establish a forced
reset/disable DMA-drain handshake. Consequently abort failure quarantines rather
than claiming force-stop success. Physical late-write exclusion needs testing.

Host fault-injection tests cover software ownership, callbacks, sensor NACKs,
abort/restart, bounded failure and adapter cache-call ordering. They do not prove
hardware DMA timing, electrical I2C behavior, cache coherence or image quality.
Before qualification, build/load the candidate and repeat normal camera/LCD,
paired SDS recording/playback, then isolated abort/timeout and guard-buffer tests.
Disconnect modules only with power removed. Preserve BSP 3.1.4 for rollback.
