#!/bin/bash
# Compile le projet avec arm-none-eabi-gcc -> build/firmware.bin
set -e

CUBE=~/.platformio/packages/framework-stm32cubel4
HAL=$CUBE/Drivers/STM32L4xx_HAL_Driver
DEV=$CUBE/Drivers/CMSIS/Device/ST/STM32L4xx
LD=~/.platformio/packages/tool-ldscripts-ststm32/stm32l4/STM32L476RGTX_FLASH.ld

mkdir -p build

arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
  -DSTM32L476xx -DUSE_HAL_DRIVER \
  -Iinclude -Ilib/cesi_grove/include -I$HAL/Inc -I$DEV/Include -I$CUBE/Drivers/CMSIS/Include \
  -Os -Wall -ffunction-sections -fdata-sections \
  src/*.c lib/cesi_grove/src/*.c \
  $(ls $HAL/Src/stm32l4xx_hal*.c | grep -v template) \
  $DEV/Source/Templates/system_stm32l4xx.c \
  $DEV/Source/Templates/gcc/startup_stm32l476xx.S \
  -T$LD --specs=nano.specs --specs=nosys.specs -u _printf_float \
  -Wl,--gc-sections -o build/firmware.elf

arm-none-eabi-objcopy -O binary build/firmware.elf build/firmware.bin
arm-none-eabi-size build/firmware.elf
