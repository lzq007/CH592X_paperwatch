################################################################################
# MRS Version: 2.5.0
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../HAL/BUZZER.c \
../HAL/FONT_CN.c \
../HAL/KEY.c \
../HAL/LED.c \
../HAL/MCU.c \
../HAL/RTC.c \
../HAL/RX8010SJ.c \
../HAL/SLEEP.c 

C_DEPS += \
./HAL/BUZZER.d \
./HAL/FONT_CN.d \
./HAL/KEY.d \
./HAL/LED.d \
./HAL/MCU.d \
./HAL/RTC.d \
./HAL/RX8010SJ.d \
./HAL/SLEEP.d 

OBJS += \
./HAL/BUZZER.o \
./HAL/FONT_CN.o \
./HAL/KEY.o \
./HAL/LED.o \
./HAL/MCU.o \
./HAL/RTC.o \
./HAL/RX8010SJ.o \
./HAL/SLEEP.o 

DIR_OBJS += \
./HAL/*.o \

DIR_DEPS += \
./HAL/*.d \

DIR_EXPANDS += \
./HAL/*.234r.expand \


# Each subdirectory must supply rules for building sources it contributes
HAL/%.o: ../HAL/%.c
	@	riscv-none-embed-gcc -march=rv32imac -mabi=ilp32 -mcmodel=medany -msmall-data-limit=8 -mno-save-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g -DDEBUG=1 -DCLK_OSC32K=0 -DHAL_KEY=1 -DCONFIG_RISCV -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/StdPeriphDriver/inc" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/RVMSIS" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/services" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/src" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/Startup" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HAL/include" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/Ld" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/LIB" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/includes" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HARDWARE/EPD" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HARDWARE/GUI" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HARDWARE/Fonts" -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

