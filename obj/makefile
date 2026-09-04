################################################################################
# MRS Version: 2.5.0
# Automatically-generated file. Do not edit!
################################################################################
-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/subdir.mk
-include services/subdir.mk
-include StdPeriphDriver/subdir.mk
-include Startup/subdir.mk
-include RVMSIS/subdir.mk
-include LIB/subdir.mk
-include HARDWARE/GUI/subdir.mk
-include HARDWARE/Fonts/subdir.mk
-include HARDWARE/EPD/subdir.mk
-include HAL/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(ASM_UPPER_DEPS)),)
-include $(ASM_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
SECONDARY_FLASH += \
CH592X_paperwatch.hex \

SECONDARY_LIST += \
CH592X_paperwatch.lst \

SECONDARY_SIZE += \
CH592X_paperwatch.siz \


# All Target
all: 
	$(MAKE) --no-print-directory main-build 

main-build: CH592X_paperwatch.elf secondary-outputs

# Tool invocations
CH592X_paperwatch.elf: $(OBJS) $(USER_OBJS_ESCAPE)
	@	riscv-none-embed-gcc -march=rv32imac -mabi=ilp32 -mcmodel=medany -msmall-data-limit=8 -mno-save-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g -T "e:\ch592f_lowpower_paperwatch\CH592X_paperwatch\Ld\Link.ld" -nostartfiles -Xlinker --gc-sections -L"e:\ch592f_lowpower_paperwatch\CH592X_paperwatch" -L"e:\ch592f_lowpower_paperwatch\CH592X_paperwatch\StdPeriphDriver" -L"e:\ch592f_lowpower_paperwatch\CH592X_paperwatch\LIB" -Xlinker --print-memory-usage -Wl,-Map,"CH592X_paperwatch.map" --specs=nano.specs --specs=nosys.specs -o "CH592X_paperwatch.elf" $(OBJS) $(LIBS)
CH592X_paperwatch.hex: CH592X_paperwatch.elf
	@	riscv-none-embed-objcopy -O ihex "CH592X_paperwatch.elf" "CH592X_paperwatch.hex"
CH592X_paperwatch.lst: CH592X_paperwatch.elf
	@	riscv-none-embed-objdump --source --all-headers --demangle --line-numbers --wide "CH592X_paperwatch.elf" > "CH592X_paperwatch.lst"
CH592X_paperwatch.siz: CH592X_paperwatch.elf
	riscv-none-embed-size --format=berkeley "CH592X_paperwatch.elf"

# Other Targets
clean:
	-$(RM) $(DIR_OBJS) $(SECONDARY_FLASH)$(SECONDARY_LIST)$(SECONDARY_SIZE) CH592X_paperwatch.elf
	-$(RM) $(DIR_EXPANDS) $(CALLGRAPH_DOT)
	-$(RM) $(DIR_DEPS)
secondary-outputs: $(SECONDARY_FLASH) $(SECONDARY_LIST) $(SECONDARY_SIZE)

.PHONY: all clean dependents

-include ../makefile.targets