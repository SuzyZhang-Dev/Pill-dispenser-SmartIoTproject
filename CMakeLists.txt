# Set minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Include build functions from Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Set board type because we are building for PicoW
set(PICO_BOARD pico_w)

# Set name of project (as PROJECT_NAME) and C/C   standards
project(PillDispenser C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Creates a pico-sdk subdirectory in our project for the libraries
pico_sdk_init()

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        -Wno-maybe-uninitialized
)

set(SOURCE_FILES
    src/config.h
    src/main.c
    src/drivers/motor.c
    src/drivers/sensor.c
    src/drivers/lora.c
    src/drivers/eeprom.c
    src/drivers/led.c

        src/logic/dispenser.c
        src/logic/dispenser.h
        src/drivers/oled.c
        src/drivers/oled.h
        src/drivers/encoder.c
        src/drivers/encoder.h
        src/drivers/appkey.h
        src/drivers/appkey_templete.h
        src/drivers/iuart.c
        src/drivers/iuart.h
)

# Tell CMake where to find the executable source file
add_executable(${PROJECT_NAME} ${SOURCE_FILES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
        src
        src/drivers
        src/logic
)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})

# Link to pico_stdlib (gpio, time, etc. functions)
target_link_libraries(${PROJECT_NAME} 
        pico_stdlib
        hardware_pwm
        hardware_gpio
        hardware_i2c
        hardware_uart
        hardware_adc
)

# Disable usb output, enable uart output
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)