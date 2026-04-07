# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Added
- UART TX ring buffer with DMA-backed non-blocking transmission to replace blocking `HAL_UART_Transmit` calls in `SendTelemetry` and `ProcessMusicCommand`.
- `UartTx_Push()` / `UartTx_Poll()` for asynchronous UART output (256-byte circular buffer, power-of-two sizing).
- `HAL_UART_TxCpltCallback()` to signal DMA transfer completion and release the sending lock.

### Changed
- `SendTelemetry()` now writes formatted telemetry data into the TX ring buffer instead of blocking for ~60 ms.
- `ProcessMusicCommand()` uses the ring buffer for PLAY/STOP commands instead of direct `HAL_UART_Transmit`.
- Dead redundant `snprintf` call removed from `SendTelemetry`.
- GPIO interrupt shared variables (`g_key_pressed`, `g_key_short_event`) now cleared under `__disable_irq()` / `__enable_irq()` critical section to prevent race conditions.

### Fixed
- Race condition between `HAL_GPIO_EXTI_Callback` and main loop when clearing `g_key_pressed` and `g_key_short_event`.
- Wasted `snprintf` call in `SendTelemetry` whose output was immediately overwritten.

### Known Issues
- UART DMA staging buffer uses a single static array; if a second DMA transfer is requested before the first completes, data may overlap. The ring buffer prevents data loss but large bursts could still drop telemetry.
- `SoftI2C` (used by MAX30102 and OLED) has no timeout protection on NACK wait loops.
