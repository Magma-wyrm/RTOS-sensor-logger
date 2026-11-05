cmake_minimum_required(VERSION 3.20)

# Two builds from the same source:
#   host (default):  cmake -B build                -> logger_sim, inversion_demo, unit_tests
#   STM32F401:       cmake -B build-arm -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
#   STM32 for QEMU:  same as above plus -DQEMU_BUILD=ON

project(rtos_sensor_logger C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

include(FetchContent)
FetchContent_Declare(freertos_kernel
    GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
    GIT_TAG        V11.1.0
    GIT_SHALLOW    TRUE
)

set(WARNINGS -Wall -Wextra -Wshadow -Wno-unused-parameter)

# hardware independent code, also used by the unit tests
add_library(common STATIC
    lib/crc16.c
    lib/record.c
    lib/ringbuf.c
    lib/cli.c
    lib/period_stats.c
    lib/mpu6500.c
    lib/imu_model.c
)
target_include_directories(common PUBLIC lib)
target_compile_options(common PRIVATE ${WARNINGS} -Werror)

set(APP_SOURCES
    app/app_main.c
    app/sampling_task.c
    app/logger_task.c
    app/command_task.c
    app/health_task.c
)

if(CMAKE_CROSSCOMPILING)
    # ---------------- STM32F401 firmware ----------------
    option(QEMU_BUILD "build for QEMU netduinoplus2 instead of the real board" OFF)

    add_library(freertos_config INTERFACE)
    target_include_directories(freertos_config SYSTEM INTERFACE bsp/stm32f401)
    set(FREERTOS_PORT GCC_ARM_CM4F CACHE STRING "" FORCE)
    set(FREERTOS_HEAP 4 CACHE STRING "" FORCE)
    FetchContent_MakeAvailable(freertos_kernel)

    add_executable(firmware
        ${APP_SOURCES}
        bsp/stm32f401/startup_stm32f401.c
        bsp/stm32f401/system_clock.c
        bsp/stm32f401/bsp_stm32f401.c
        bsp/stm32f401/i2c1.c
        bsp/stm32f401/syscalls.c
        bsp/stm32f401/main_stm32.c
    )
    target_include_directories(firmware PRIVATE app bsp bsp/stm32f401)
    target_link_libraries(firmware PRIVATE common freertos_kernel)
    target_compile_options(firmware PRIVATE ${WARNINGS})
    if(QEMU_BUILD)
        target_compile_definitions(firmware PRIVATE QEMU_BUILD=1)
    endif()
    target_link_options(firmware PRIVATE
        -T${CMAKE_SOURCE_DIR}/bsp/stm32f401/stm32f401cc.ld
        -Wl,-Map=${CMAKE_BINARY_DIR}/firmware.map
        -Wl,--print-memory-usage
    )
    set_target_properties(firmware PROPERTIES SUFFIX ".elf")
    add_custom_command(TARGET firmware POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:firmware> ${CMAKE_BINARY_DIR}/firmware.bin
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:firmware>
    )
else()
    # ---------------- Linux simulation + tests ----------------
    add_library(freertos_config INTERFACE)
    target_include_directories(freertos_config SYSTEM INTERFACE bsp/sim)
    set(FREERTOS_PORT GCC_POSIX CACHE STRING "" FORCE)
    set(FREERTOS_HEAP 4 CACHE STRING "" FORCE)
    FetchContent_MakeAvailable(freertos_kernel)

    add_executable(logger_sim
        ${APP_SOURCES}
        bsp/sim/bsp_sim.c
        bsp/sim/main_sim.c
    )
    target_include_directories(logger_sim PRIVATE app bsp bsp/sim)
    target_link_libraries(logger_sim PRIVATE common freertos_kernel)
    target_compile_options(logger_sim PRIVATE ${WARNINGS})

    add_executable(inversion_demo bsp/sim/inversion_demo.c)
    target_link_libraries(inversion_demo PRIVATE freertos_kernel)
    target_compile_options(inversion_demo PRIVATE ${WARNINGS})

    add_executable(unit_tests
        tests/test_main.c
        tests/test_lib.c
        tests/test_mpu6500.c
    )
    target_link_libraries(unit_tests PRIVATE common)
    target_compile_options(unit_tests PRIVATE ${WARNINGS})

    enable_testing()
    add_test(NAME unit_tests COMMAND unit_tests)
endif()
