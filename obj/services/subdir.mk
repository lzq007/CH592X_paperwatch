################################################################################
# MRS Version: 2.5.0
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../services/ancs_app_attr_get.c \
../services/ancs_attr_parser.c \
../services/ancs_client.c \
../services/devinfoservice.c \
../services/timesyncservice.c 

C_DEPS += \
./services/ancs_app_attr_get.d \
./services/ancs_attr_parser.d \
./services/ancs_client.d \
./services/devinfoservice.d \
./services/timesyncservice.d 

OBJS += \
./services/ancs_app_attr_get.o \
./services/ancs_attr_parser.o \
./services/ancs_client.o \
./services/devinfoservice.o \
./services/timesyncservice.o 

DIR_OBJS += \
./services/*.o \

DIR_DEPS += \
./services/*.d \

DIR_EXPANDS += \
./services/*.234r.expand \


# Each subdirectory must supply rules for building sources it contributes
services/%.o: ../services/%.c
	@	riscv-none-embed-gcc -march=rv32imac -mabi=ilp32 -mcmodel=medany -msmall-data-limit=8 -mno-save-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g -DDEBUG=1 -DCLK_OSC32K=0 -DHAL_KEY=1 -DCONFIG_RISCV -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/StdPeriphDriver/inc" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/RVMSIS" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/services" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/src" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/Startup" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HAL/include" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/Ld" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/LIB" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/includes" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HARDWARE/EPD" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HARDWARE/GUI" -I"e:/ch592f_lowpower_paperwatch/CH592X_paperwatch/HARDWARE/Fonts" -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

