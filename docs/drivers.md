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

## GPDMA transfers

GPDMA1 channel 0 moves SPI1 RX bytes into task-owned storage and channel 1 moves the TX
command bytes to SPI1. The ISR snapshots and clears both channels, aborts both sides on
error, deasserts chip select, executes memory barriers, and notifies the waiting
acquisition task on notification index 1. A bounded wait resets both channels on
timeout. TIM3 produces the 1 kHz sample notification on index 0, keeping timer and DMA
events distinct. The routing still requires electrical and timing validation.

## SPI1 and ICM-42688-P

`firmware/drivers/spi1.c` owns PA5/PA6/PA7, PD14, SPI1, and the DWT cycle counter. It
uses 8-bit mode-0 full-duplex transactions, selects a prescaler below the requested bus
ceiling, and has a polling path for pre-scheduler initialization plus a GPDMA path for
runtime samples. Both paths bound transfers and recover on timeout. The portable sensor driver checks identity,
performs the documented reset/startup sequence, configures 1 kHz low-noise accel/gyro
modes, and decodes six big-endian signed axes. Select this backend with
`DRATS_REAL_SENSOR=ON`; physical electrical and timing validation is pending.

## TIM2 and watchdog

TIM2 is a 1 MHz free-running 32-bit counter extended to 64 bits by its overflow ISR. A
short interrupt-masked read makes the overflow boundary coherent. The IWDG driver
starts LSI, validates its reload range, captures RCC reset flags, and only receives a
refresh from the supervisor after every required task meets its heartbeat deadline.
IWDG enablement is a build option until its timing is measured on the board.

TIM3 owns the real sensor's 1 kHz sample cadence. Its ISR only clears the update flag
and notifies the acquisition task; the task starts the SPI/GPDMA transaction and uses
TIM2 for the first-sample timestamp.

## Fault handling

HardFault records CFSR, HFSR, MMFAR and BFAR in a `.noinit` record before stopping.
RCC reset flags are captured before initialization. Persistence to flash remains to be
implemented. The watchdog policy is host-tested, while physical timeout and reset
behavior remain unvalidated.
