################################################################################
# MRS Version: 2.5.0
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../StdPeriphDriver/CH59x_adc.c \
../StdPeriphDriver/CH59x_clk.c \
../StdPeriphDriver/CH59x_flash.c \
../StdPeriphDriver/CH59x_gpio.c \
../StdPeriphDriver/CH59x_i2c.c \
../StdPeriphDriver/CH59x_lcd.c \
../StdPeriphDriver/CH59x_pwm.c \
../StdPeriphDriver/CH59x_pwr.c \
../StdPeriphDriver/CH59x_spi0.c \
../StdPeriphDriver/CH59x_sys.c \
../StdPeriphDriver/CH59x_uart0.c \
../StdPeriphDriver/CH59x_uart1.c 

C_DEPS += \
./StdPeriphDriver/CH59x_adc.d \
./StdPeriphDriver/CH59x_clk.d \
./StdPeriphDriver/CH59x_flash.d \
./StdPeriphDriver/CH59x_gpio.d \
./StdPeriphDriver/CH59x_i2c.d \
./StdPeriphDriver/CH59x_lcd.d \
./StdPeriphDriver/CH59x_pwm.d \
./StdPeriphDriver/CH59x_pwr.d \
./StdPeriphDriver/CH59x_spi0.d \
./StdPeriphDriver/CH59x_sys.d \
./StdPeriphDriver/CH59x_uart0.d \
./StdPeriphDriver/CH59x_uart1.d 

OBJS += \
./StdPeriphDriver/CH59x_adc.o \
./StdPeriphDriver/CH59x_clk.o \
./StdPeriphDriver/CH59x_flash.o \
./StdPeriphDriver/CH59x_gpio.o \
./StdPeriphDriver/CH59x_i2c.o \
./StdPeriphDriver/CH59x_lcd.o \
./StdPeriphDriver/CH59x_pwm.o \
./StdPeriphDriver/CH59x_pwr.o \
./StdPeriphDriver/CH59x_spi0.o \
./StdPeriphDriver/CH59x_sys.o \
./StdPeriphDriver/CH59x_uart0.o \
./StdPeriphDriver/CH59x_uart1.o 

DIR_OBJS += \
./StdPeriphDriver/*.o \

DIR_DEPS += \
./StdPeriphDriver/*.d \

DIR_EXPANDS += \
./StdPeriphDriver/*.234r.expand \


# Each subdirectory must supply rules for building sources it contributes
StdPeriphDriver/%.o: ../StdPeriphDriver/%.c
	@	riscv-none-embed-gcc -march=rv32imac -mabi=ilp32 -mcmodel=medany -msmall-data-limit=8 -mno-save-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g -DDEBUG=1 -DCLK_OSC32K=0 -DHAL_KEY=1 -DCONFIG_RISCV -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/StdPeriphDriver/inc" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/RVMSIS" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/services" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/src" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/Startup" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HAL/include" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/Ld" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/LIB" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/includes" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HARDWARE/EPD" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HARDWARE/GUI" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HARDWARE/Fonts" -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

