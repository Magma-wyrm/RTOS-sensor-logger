#!/usr/bin/env bash
# Runs the real ARM firmware (Cortex-M4F FreeRTOS port) in QEMU for 60 s,
# types "stats" and "stall 1500" on the console, then checks the binary
# log that came out of USART1.
#
# needs: arm-none-eabi-gcc, cmake, qemu-system-arm, python3
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-qemu"
OUT="$ROOT/results/qemu"
mkdir -p "$OUT"

cmake -S "$ROOT" -B "$BUILD" -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/arm-none-eabi.cmake" -DQEMU_BUILD=ON > /dev/null
cmake --build "$BUILD" -j4

# serial 0 = USART1 (binary records), serial 1 = USART2 (console)
( sleep 12; printf 'stats\r\n'; sleep 3; printf 'stall 1500\r\n'; sleep 70 ) | \
timeout 120 qemu-system-arm -M netduinoplus2 -nographic -monitor none \
    -serial file:"$OUT/log.bin" -serial stdio \
    -semihosting-config enable=on,target=native \
    -kernel "$BUILD/firmware.elf" | tee "$OUT/console.txt"

python3 "$ROOT/tools/analyze_log.py" "$OUT/log.bin" "$OUT/console.txt" | tee "$OUT/analysis.txt"
