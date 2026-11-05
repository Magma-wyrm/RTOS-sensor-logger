# Design notes

## Goal

Sample an IMU at a fixed 100 Hz and log every sample, without the slow
storage ever making the sampling late. If samples are ever lost, the
firmware must know exactly how many, never lose them silently.

## Tasks

| task | priority | period / trigger | job |
|---|---|---|---|
| sampling | 5 (highest) | every 10 ms, `xTaskDelayUntil` | read MPU6500 (14 byte burst), timestamp, `xQueueSend(..., 0)` |
| command | 4 | polls RX ring buffer every 20 ms | console commands, fault injection |
| logger | 3 | blocks on the queue (100 ms timeout) | encode 24 byte records, write in batches of 32 |
| health | 2 (lowest above idle) | every 500 ms | heartbeat check, watchdog kick, stats |

```
 MPU6500 --I2C--> [sampling] --queue (100)--> [logger] --> sink (file / USART1)
                       |                          |
                  heartbeat                  heartbeat
                       v                          v
 USART2 RX ISR --> ring buffer --> [command] --> [health] --kick--> IWDG
```

## Why these choices

**`xTaskDelayUntil` instead of `vTaskDelay`.** `vTaskDelay(10)` waits 10 ms
from *now*, so the time spent reading the sensor adds up and the rate
drifts. `xTaskDelayUntil` wakes at fixed points (last wake + 10 ms). It
also returns `pdFALSE` if the deadline was already missed, which the
task counts as `late`.

**Sampling never blocks.** The queue send uses timeout 0. If the logger
is stuck, a full queue drops the newest sample and increments
`dropped`, but sampling stays on time. Every sample gets a sequence number
before the send, so a drop shows up as a gap in the log too. The test
script checks that `dropped` (firmware counter) equals the gaps found in
the file.

**Queue length 100 = 1 second.** At 100 Hz, a 100 entry queue can absorb a
storage stall of almost 1 s. Tests: 300 ms and 900 ms stalls lose
nothing; 1500 ms loses exactly 50 samples (1500 ms - 1000 ms = 500 ms =
50 samples), 1900 ms loses 90.

**Batching.** SD cards and UART DMA both like big writes. 32 records =
768 bytes per write. A partial batch is written after 500 ms so data is
never stuck for long.

**Only the logger touches the sink.** One owner means no mutex is needed
for the storage. The console UART is shared by several tasks, so it has a
mutex (`xSemaphoreCreateMutex`, which has priority inheritance). The
sampling task never prints.

**Health is the lowest priority on purpose.** Two failure types:
1. A task blocks forever (driver waiting for an interrupt that never comes).
   Its heartbeat gets older than 2 s, health logs which task it is and
   stops kicking the watchdog. Reset about 0.5-1 s later.
2. A task loops forever (bug). If it has higher priority than health,
   health never runs, nobody kicks the watchdog, reset after 1 s.
   This is why the kick is in the lowest priority task and not in a timer
   interrupt: an interrupt would keep kicking even when the tasks are dead.

A 3 s storage stall is treated like case 1 (the logger is silent for more
than 2 s). This is on purpose: a storage write that takes that long is
most likely a hung card.

**Counters without locks.** Each counter in `g_stats` is written by only one
task. A 32-bit aligned store is atomic on Cortex-M4, so a reader never
sees half a value. When health prints, it copies the whole struct inside
`taskENTER_CRITICAL` so all numbers belong to the same moment.

**Priority inversion.** `bsp/sim/inversion_demo.c` shows why the console lock
is a mutex and not a binary semaphore: LOW holds the lock, HIGH waits for
it, MED (no lock) preempts LOW. With a binary semaphore HIGH waited 23 ms
(LOW's 4 ms + MED's 20 ms - 1 ms offset). With a mutex LOW is raised to
HIGH's priority while it holds the lock and HIGH waited 3 ms.

## Record format (24 bytes, little endian)

| bytes | field |
|---|---|
| 0 | sync 0xA5 |
| 1 | flags (bit0 IMU error, bit1 late) |
| 2-5 | seq |
| 6-9 | t_us |
| 10-21 | ax ay az gx gy gz (int16) |
| 22-23 | CRC-16/CCITT-FALSE of bytes 0-21 |

Written byte by byte, not with `memcpy` of a struct, so padding and
endianness of the compiler do not matter.

## Board support layer

`bsp/bsp.h` is the only thing the tasks know about the hardware.

| | `bsp/sim` (Linux) | `bsp/stm32f401` (real board) | `bsp/stm32f401` + `QEMU_BUILD` |
|---|---|---|---|
| CPU | PC, FreeRTOS POSIX port | Cortex-M4F @ 84 MHz | Cortex-M4 emulated |
| IMU | fake MPU6500 (`imu_model.c`) | real MPU6500 on I2C1 | fake MPU6500 |
| console | stdin/stdout | USART2 115200 + RX interrupt | USART2 |
| sink | binary file | USART1 460800 to PC | USART1 to a file |
| time | `clock_gettime` | TIM2 at 1 MHz | TIM2 |
| watchdog | host thread counting ticks | IWDG | (not emulated) |

## What is not done yet

- Not run on the real Black Pill yet (hardware not with me). The STM32 BSP
  is written from RM0368, builds, and boots in QEMU, but the I2C1 driver,
  the 84 MHz clock setup and the IWDG have only been checked by reading
  the reference manual. QEMU does not emulate RCC, I2C or IWDG.
- Timing numbers (jitter) from the simulation and QEMU are NOT real
  hardware numbers. The POSIX port tick comes from a `usleep` loop on the
  PC. Real jitter will be measured on the board with TIM2.
- SD card (SPI + FatFS) instead of the UART sink.
- Debugging with an ST-Link (SWD), the HardFault handler already saves the
  faulting PC in `g_fault_pc` for that.
