
PROJECT     := main
SRC_DIR     := Src
INC_DIR     := Inc
OBJ_DIR     := Objects
BUILD_DIR   := Build
DRIVER_DIR  := Drivers
HAL_DRIVER_F  := $(DRIVER_DIR)/stm32f1xx-hal-driver

CMSIS_DIR   := $(DRIVER_DIR)/CMSIS
STARTUP_DIR := Startup
LDSCRIPT    := linkerscript.ld

PREFIX   ?= arm-none-eabi-
CC       := $(PREFIX)gcc
CXX      := $(PREFIX)g++
LD       := $(PREFIX)gcc
AR       := $(PREFIX)ar
AS       := $(PREFIX)as
OBJCOPY  := $(PREFIX)objcopy
OBJDUMP  := $(PREFIX)objdump
GDB      := $(PREFIX)gdb
STFLASH  := $(shell which st-flash)

OPT       := -Og
DEBUG     := -ggdb3
CSTD      := -std=c99

CFLAGS    = -mcpu=cortex-m3 -mthumb -mlittle-endian \
            -Wall -ffunction-sections -fdata-sections \
            -I$(HAL_DRIVER_F)/Inc \
			-I$(HAL_DRIVER_F)/Legacy \
			-IDrivers/CMSIS/Device/ST/STM32F1xx/Include \
			-IDrivers/CMSIS/Include \
            -I$(INC_DIR) \
            -ICore/Inc \
            -DSTM32F103xB \
			-DHAL_TIM_MODULE_ENABLED\
			-DHAL_UART_MODULE_ENABLED\
			-DHAL_ADC_MODULE_ENABLED\
			-DHAL_SPI_MODULE_ENABLED\
			-D__DEBUG_EN\
            $(OPT) $(DEBUG) $(CSTD)
LIBS = -lm
LDFLAGS = -T $(LDSCRIPT) --static -Wl,--gc-sections --specs=nosys.specs


# Flash Address
FL_ADDR   ?= 0x8000000



SRC_FILES = $(wildcard $(SRC_DIR)/*.c)

HAL_FILES = $(HAL_DRIVER_F)/Src/stm32f1xx_hal_gpio_ex.c \
		$(HAL_DRIVER_F)/Src/stm32f1xx_hal_tim.c \
		$(HAL_DRIVER_F)/Src/stm32f1xx_hal_tim_ex.c \
		$(HAL_DRIVER_F)/Src/stm32f1xx_hal_adc.c \
		$(HAL_DRIVER_F)/Src/stm32f1xx_hal.c \
		$(HAL_DRIVER_F)/Src/stm32f1xx_hal_rcc.c \
		$(HAL_DRIVER_F)/Src/stm32f1xx_hal_rcc_ex.c \
		$(HAL_DRIVER_F)/Src/stm32f1xx_hal_spi.c \
		$(HAL_DRIVER_F)/Src/stm32f1xx_hal_gpio.c \
		$(HAL_DRIVER_F)/Src/stm32f1xx_hal_dma.c \
		$(HAL_DRIVER_F)/Src/stm32f1xx_hal_cortex.c \
		$(HAL_DRIVER_F)/Src/stm32f1xx_hal_pwr.c \
		$(HAL_DRIVER_F)/Src/stm32f1xx_hal_flash.c \
		$(HAL_DRIVER_F)/Src/stm32f1xx_hal_flash_ex.c \
		$(HAL_DRIVER_F)/Src/stm32f1xx_hal_exti.c \
		$(HAL_DRIVER_F)/Src/stm32f1xx_hal_uart.c \

CMSIS_FILES = $(CMSIS_DIR)/Device/ST/STM32F1xx/Source/Templates/system_stm32f1xx.c

SOURCES = $(SRC_FILES) $(HAL_FILES) $(CMSIS_FILES)


# # Objects
OBJS := $(patsubst %.c, $(OBJ_DIR)/%.o, $(notdir $(SOURCES)))
OBJS += $(OBJ_DIR)/startup_stm32f103c8tx.o

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
# ================================
# Rules
# ================================
all: build flash

# Build ELF
build: $(BUILD_DIR)/$(PROJECT).elf

$(BUILD_DIR)/$(PROJECT).elf: $(OBJ_DIR) $(BUILD_DIR) $(OBJS) 
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS) $(LIBS)
	$(OBJCOPY) -O ihex   $@ $(BUILD_DIR)/$(PROJECT).hex
	$(OBJCOPY) -O binary $@ $(BUILD_DIR)/$(PROJECT).bin
	@echo "Build complete: $@"

# Compile C sources

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c 
	$(CC) -c $(CFLAGS) $< -o $@
$(OBJ_DIR)/%.o: $(HAL_DRIVER_F)/Src/%.c | $(OBJ_DIR)
	$(CC) -c $(CFLAGS) $< -o $@
$(OBJ_DIR)/startup_stm32f103c8tx.o: startup_stm32f103c8tx.s | $(OBJ_DIR)
	$(CC) -c -mcpu=cortex-m3 -mthumb -mlittle-endian $< -o $@

$(OBJ_DIR)/%.o: Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/%.c | $(OBJ_DIR)
	$(CC) -c $(CFLAGS) $< -o $@
# Assemble startup


# Clean
clean:
	rm -rf $(OBJ_DIR) $(BUILD_DIR)

# Flash to STM32
flash: $(BUILD_DIR)/$(PROJECT).bin
	sudo $(STFLASH) write $< $(FL_ADDR)
show_sources:
	@echo "SOURCES: $(SOURCES) $()"
	
.PHONY: all build clean flash
