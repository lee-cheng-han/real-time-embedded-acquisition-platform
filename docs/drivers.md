# Target driver contracts

## USART3 telemetry

`firmware/drivers/uart3.c` owns USART3, GPIOD pins 8/9, its NVIC vector, and a 2048-byte
TX ring. Initialization configures AF7, 8-N-1 framing, the caller-supplied peripheral
clock and baud rate. A single telemetry task is the producer and the USART ISR is the
consumer. A submission is accepted atomically only when the entire frame fits; otherwise
the caller retains its source block and retries. Shutdown disables the IRQ and UART.

The ISR clears parity, framing, noise and overrun flags and counts hardware errors. It
disables TX-empty interrupts when the ring drains. Current target code copies encoded
frames into this ring, so the telemetry path is explicitly not zero-copy.

## GPDMA interrupt shell

GPDMA1 channel 0's handler reads status once, clears all observed clearable flags,
executes a data-synchronization barrier, and notifies the acquisition task only for a
transfer-complete condition. Error flags increment a diagnostic counter. Peripheral
configuration and recovery are intentionally absent until SPI/timer request routing and
the sensor's electrical timing are fixed.

## Fault handling

HardFault records CFSR, HFSR, MMFAR and BFAR in a `.noinit` record before stopping.
Hardware reset-reason capture and persistence to flash remain to be implemented. The
watchdog policy is host-tested, but the IWDG is not enabled until the task liveness
window has been physically validated.
