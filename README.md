# rtos-sensor-logger

FreeRTOS firmware that samples an MPU6500 IMU at 100 Hz on an
**STM32F401CCU6 (Black Pill, Cortex-M4F)** and logs every sample as a
CRC-checked binary record, without the slow storage ever delaying the
sampling. Samples that cannot be stored are counted, never lost silently.
A health task plus the hardware watchdog recover the board from hung or
runaway tasks.

The same application code builds for three targets:

1. **STM32F401** - register-level drivers written from the reference manual (RM0368), no HAL
2. **STM32 firmware in QEMU** (netduinoplus2, Cortex-M4) - the real ARM binary, with a fake IMU
3. **Linux simulation** (FreeRTOS POSIX port) - used for fault injection tests and unit tests

> **Status:** verified in the Linux simulation and in QEMU. Not yet run on
> the physical board (bring-up planned when I have my hardware and an
> ST-Link). Timing numbers from the simulation are not hardware numbers,
> see [docs/RESULTS.md](docs/RESULTS.md).

## Features

- 4 tasks with fixed priorities: sampling (10 ms, `xTaskDelayUntil`), command shell, logger, health
- non-blocking producer: `xQueueSend(..., 0)`, 100-entry queue (1 s of data), drop counter + sequence numbers
- 24-byte records with CRC-16/CCITT, written in batches of 32
- UART command shell (`stats`, `start`, `stop`, `stall <ms>`, `hang`, `spin`) fed by an RX interrupt and a lock-free ring buffer
- heartbeat supervision; the watchdog is only kicked when every task is alive
- stack overflow hook, malloc failed hook, HardFault handler that saves the faulting PC
- priority inversion demo: binary semaphore vs mutex (priority inheritance)
- 24 unit tests on the host, including the MPU6500 driver against a fake I2C device
- GitHub Actions: builds both targets, runs the tests, the fault scenarios and a QEMU run

## Results (from `tools/run_scenarios.py` and `tools/run_qemu.sh`)

| test | result |
|---|---|
| 600 s baseline (sim) | 53,300 samples, 0 dropped, 0 CRC errors, 0 late wakeups |
| 300 ms / 900 ms storage stalls (sim) | 0 samples lost, queue peaked at 30 / 90 of 100 |
| 1500 ms storage stall (sim and QEMU) | exactly 50 dropped, drop counter = sequence gaps in the log |
| logger blocked forever | detected in 2.4 s, watchdog reset at 2.9 s |
| logger stuck in a loop | watchdog reset in < 1 s |
| priority inversion demo | high-priority wait 23 ms (binary semaphore) vs 3 ms (mutex) |
| firmware size (STM32F401) | ~20 KB flash, 24.6 KB RAM |
| unit tests | 24 tests, 85 checks, all pass |

Full tables, how they were measured and what they do **not** show: [docs/RESULTS.md](docs/RESULTS.md).
Raw logs: [docs/results/](docs/results/).

## Build and run

Needs Linux or WSL with `gcc`, `cmake`, `python3`. For the STM32 build also
`gcc-arm-none-eabi`, for QEMU `qemu-system-arm`. FreeRTOS-Kernel V11.1.0 is
downloaded by CMake.

```bash
# simulation + unit tests
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure

./build/logger_sim                       # interactive, type: help
./build/logger_sim --duration 30 --at 10000:"stall 1500"
python3 tools/analyze_log.py log.bin     # check CRCs and sequence gaps

python3 tools/run_scenarios.py           # all fault scenarios (~12 min), --quick for ~2 min
./build/inversion_demo semaphore
./build/inversion_demo mutex

# STM32F401 firmware
cmake -B build-arm -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
cmake --build build-arm                  # -> build-arm/firmware.elf / firmware.bin

# same firmware in QEMU
bash tools/run_qemu.sh
```

Flashing the Black Pill without a debugger: hold BOOT0, tap NRST, release
BOOT0, then load `firmware.bin` at `0x08000000` with STM32CubeProgrammer (USB DFU).

## Wiring (Black Pill)

| signal | pin |
|---|---|
| console TX / RX (115200) | PA2 / PA3 (USART2) to a USB-serial adapter |
| log data TX (460800) | PA9 (USART1) |
| MPU6500 SCL / SDA | PB6 / PB7 (I2C1, 400 kHz) |
| status LED | PC13 (blinks every 500 ms while healthy) |

## Layout

```
app/        tasks, the same on every target
lib/        hardware independent code: CRC, record format, ring buffer, CLI parser,
            period statistics, MPU6500 driver, fake MPU6500
bsp/        bsp.h interface
  sim/        Linux: FreeRTOS POSIX port, fake UART/watchdog, priority inversion demo
  stm32f401/  startup code, linker script, clock, USART, I2C, TIM2, IWDG
tests/      unit tests
tools/      log analyzer, scenario runner, QEMU runner
docs/       DESIGN.md, RESULTS.md
```

## What I learned / what is next

See the end of [docs/DESIGN.md](docs/DESIGN.md).
