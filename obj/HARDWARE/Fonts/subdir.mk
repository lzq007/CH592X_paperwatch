################################################################################
# MRS Version: 2.5.0
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../HARDWARE/Fonts/font12.c \
../HARDWARE/Fonts/font12CN.c \
../HARDWARE/Fonts/font16.c \
../HARDWARE/Fonts/font20.c \
../HARDWARE/Fonts/font24.c \
../HARDWARE/Fonts/font24CN.c \
../HARDWARE/Fonts/font8.c 

C_DEPS += \
./HARDWARE/Fonts/font12.d \
./HARDWARE/Fonts/font12CN.d \
./HARDWARE/Fonts/font16.d \
./HARDWARE/Fonts/font20.d \
./HARDWARE/Fonts/font24.d \
./HARDWARE/Fonts/font24CN.d \
./HARDWARE/Fonts/font8.d 

OBJS += \
./HARDWARE/Fonts/font12.o \
./HARDWARE/Fonts/font12CN.o \
./HARDWARE/Fonts/font16.o \
./HARDWARE/Fonts/font20.o \
./HARDWARE/Fonts/font24.o \
./HARDWARE/Fonts/font24CN.o \
./HARDWARE/Fonts/font8.o 

DIR_OBJS += \
./HARDWARE/Fonts/*.o \

DIR_DEPS += \
./HARDWARE/Fonts/*.d \

DIR_EXPANDS += \
./HARDWARE/Fonts/*.234r.expand \


# Each subdirectory must supply rules for building sources it contributes
HARDWARE/Fonts/%.o: ../HARDWARE/Fonts/%.c
	@	riscv-none-embed-gcc -march=rv32imac -mabi=ilp32 -mcmodel=medany -msmall-data-limit=8 -mno-save-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g -DDEBUG=1 -DCLK_OSC32K=0 -DHAL_KEY=1 -DCONFIG_RISCV -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/StdPeriphDriver/inc" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/RVMSIS" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/services" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/src" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/Startup" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HAL/include" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/Ld" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/LIB" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/includes" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HARDWARE/EPD" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HARDWARE/GUI" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HARDWARE/Fonts" -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

