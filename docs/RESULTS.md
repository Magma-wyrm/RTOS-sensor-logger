# Measured results

All numbers below come from actual runs. The raw console output and the
analyzer output for every run are in [docs/results/](results/).
To reproduce them:

```bash
python3 tools/run_scenarios.py      # Linux simulation, ~12 min
bash tools/run_qemu.sh              # STM32 firmware in QEMU, ~1.5 min
ctest --test-dir build              # unit tests
```

**Where the numbers come from:** the Linux simulation (FreeRTOS V11.1.0 POSIX
port) and the real ARM firmware running in QEMU (netduinoplus2, Cortex-M4).
**Nothing here was measured on the physical STM32 board yet.**

## 1. Data integrity under storage stalls (Linux simulation)

The checker (`tools/analyze_log.py`) decodes every record in the log file,
checks the CRC, and finds gaps in the sequence numbers. It then compares
those with the counters the firmware printed in its `SUMMARY` line.

| scenario | samples | dropped (firmware counter) | seq gaps (in log file) | records in file | max queue fill | CRC errors | late wakeups |
|---|---|---|---|---|---|---|---|
| baseline, 600 s, no faults | 53,300 | 0 | 0 | 53,300 | 1 / 100 | 0 | 0 |
| 300 ms stall every 2 s | 10,850 | 0 | 0 | 10,850 | 30 / 100 | 0 | 0 |
| 900 ms stall every 3 s | 10,900 | 0 | 0 | 10,900 | 90 / 100 | 0 | 0 |
| one 1500 ms stall | 2,750 | 50 | 50 | 2,700 | 100 / 100 | 0 | 0 |
| one 1900 ms stall | 2,750 | 90 | 90 | 2,660 | 100 / 100 | 0 | 0 |

- 80,550 samples in total, 0 CRC errors. In every run the drop counter equals the sequence gaps.
- The queue holds 1 s of data, so stalls up to 900 ms lose nothing. The max fill is 90 of 100, which matches 900 ms at 10 ms per sample.
- A 1500 ms stall drops exactly 500 ms worth of samples (50), and a 1900 ms stall drops 90. Every drop is counted.
- `late` counts `xTaskDelayUntil` returning `pdFALSE`. It was 0 in all runs: the sampling task never missed a wakeup, even while the logger was blocked.

## 2. Watchdog and health monitor (Linux simulation)

Time is measured in RTOS ticks (1 tick = 1 ms), from the moment the fault command runs.

| fault | what happens | detected by health | watchdog reset after |
|---|---|---|---|
| `hang`: logger blocks forever | health reports "logger heartbeat lost" and stops kicking | 2437 ms | 2939 ms |
| `stall 3000`: storage silent for 3 s | treated as a hung logger (by design, see DESIGN.md) | 2239 ms | 2741 ms |
| `spin`: logger loops forever at higher priority than health | health cannot run at all, so nobody kicks | (health starved) | 577 ms |

Expected:
- **Detection:** 2000 ms heartbeat timeout plus up to 500 ms health period, so 2000–2500 ms.
- **Reset after detection:** at most 1000 ms after the last kick, so 0.5–1 s later.
- **`spin` case:** the reset comes at most 1000 ms after the last good kick.

## 3. Priority inversion: binary semaphore vs mutex (Linux simulation, 100 rounds each)

`bsp/sim/inversion_demo.c` sets up the following:
- **LOW** (priority 1) holds the lock for 4 ms.
- **HIGH** (priority 3) wants the lock 1 ms later.
- **MED** (priority 2) runs 20 ms of work without the lock.

| lock | HIGH wait min / avg / max | LOW's priority while holding the lock |
|---|---|---|
| binary semaphore | 23 / 23.00 / 23 ms | 1 (no inheritance, MED preempts LOW) |
| mutex | 0 / 2.97 / 3 ms | 3 (raised to HIGH's priority) |

## 4. ARM firmware in QEMU (Cortex-M4, FreeRTOS ARM_CM4F port)

This is the same `firmware.elf` code as for the board, built with
`QEMU_BUILD` because QEMU emulates neither I2C nor RCC, so it uses the fake
IMU. It ran for 60 s, with `stall 1500` typed into USART2 at about 15 s.
Records were read from the USART1 output file.

| | value |
|---|---|
| samples / written / dropped | 6,000 / 5,950 / 50 |
| sequence gaps in USART1 log | 50 (matches) |
| CRC errors | 0 |
| sampling task stack | 128 of 256 words still free (stack high-water mark) |
| FreeRTOS heap free | 11,232 of 20,480 bytes |

## 5. Memory use, real board build (`build-arm`, -Og)

| | |
|---|---|
| flash | about 20 KB of 256 KB (19,800 - 19,970 B, the size changes a little with the build folder path because assert messages contain file names) |
| RAM | 24,640 B of 64 KB (37.6 %), of which 20 KB is the FreeRTOS heap |

## 6. Unit tests

24 tests and 85 checks, all passing. They cover CRC-16, the record encoder and decoder, the ring buffer, the CLI parser, period statistics, and the MPU6500 driver against the fake I2C device.

## What these numbers do NOT show

- **Timing jitter.** In the simulation, the POSIX port tick comes from a `usleep` loop on a shared PC, so the period measured in host time averages about 11.2 ms instead of 10.0 ms and has spikes up to 107 ms. QEMU averaged 10.09 ms, but QEMU timing is not cycle accurate either. Real jitter has to be measured on the Black Pill with TIM2 (1 µs resolution). That is the next step.
- **The board drivers.** The STM32 I2C, clock and IWDG code has not been run on hardware yet.
