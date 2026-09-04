################################################################################
# MRS Version: 2.5.0
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../HARDWARE/EPD/Display_EPD_W21.c \
../HARDWARE/EPD/Display_EPD_W21_spi.c \
../HARDWARE/EPD/EPD_Display.c 

C_DEPS += \
./HARDWARE/EPD/Display_EPD_W21.d \
./HARDWARE/EPD/Display_EPD_W21_spi.d \
./HARDWARE/EPD/EPD_Display.d 

OBJS += \
./HARDWARE/EPD/Display_EPD_W21.o \
./HARDWARE/EPD/Display_EPD_W21_spi.o \
./HARDWARE/EPD/EPD_Display.o 

DIR_OBJS += \
./HARDWARE/EPD/*.o \

DIR_DEPS += \
./HARDWARE/EPD/*.d \

DIR_EXPANDS += \
./HARDWARE/EPD/*.234r.expand \


# Each subdirectory must supply rules for building sources it contributes
HARDWARE/EPD/%.o: ../HARDWARE/EPD/%.c
	@	riscv-none-embed-gcc -march=rv32imac -mabi=ilp32 -mcmodel=medany -msmall-data-limit=8 -mno-save-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g -DDEBUG=1 -DCLK_OSC32K=0 -DHAL_KEY=1 -DCONFIG_RISCV -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/StdPeriphDriver/inc" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/RVMSIS" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/services" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/src" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/Startup" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HAL/include" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/Ld" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/LIB" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/includes" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HARDWARE/EPD" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HARDWARE/GUI" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HARDWARE/Fonts" -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

