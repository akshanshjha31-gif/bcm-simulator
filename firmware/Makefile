# ==========================================================================
#  Automotive Body Control Module (BCM) Simulator - firmware build system
#  Target : STM32F103C8T6 "Blue Pill" (Cortex-M3, 72 MHz, 64K flash/20K RAM)
#  Lang   : Embedded C++14 (application) + C (CMSIS/HAL/startup)
#  Chain  : arm-none-eabi-gcc  (fetched per-user via xpm, no admin)
# ==========================================================================

TARGET      := bcm_firmware
BUILD_DIR   := build
VENDOR_DIR  := vendor

# ---- Vendor sources (fetched by `make deps`) -----------------------------
CUBE_DIR    := $(VENDOR_DIR)/STM32CubeF1
HAL_DIR     := $(CUBE_DIR)/Drivers/STM32F1xx_HAL_Driver
CMSIS_DIR   := $(CUBE_DIR)/Drivers/CMSIS
DEVICE_DIR  := $(CMSIS_DIR)/Device/ST/STM32F1xx

CUBE_REPO   := https://github.com/STMicroelectronics/STM32CubeF1.git

# ---- Toolchain -----------------------------------------------------------
PREFIX      := arm-none-eabi-
CC          := $(PREFIX)gcc
CXX         := $(PREFIX)g++
AS          := $(PREFIX)gcc -x assembler-with-cpp
OBJCOPY     := $(PREFIX)objcopy
SIZE        := $(PREFIX)size

# ---- MCU flags (Cortex-M3, no FPU) ---------------------------------------
CPU         := -mcpu=cortex-m3
MCU         := $(CPU) -mthumb

DEFS        := -DUSE_HAL_DRIVER -DSTM32F103xB

# ---- Include paths -------------------------------------------------------
INCLUDES := \
  -Icommon \
  -Ibsp \
  -Icore \
  -Iapp \
  -I$(HAL_DIR)/Inc \
  -I$(CMSIS_DIR)/Include \
  -I$(DEVICE_DIR)/Include

# ---- C sources (CMSIS/HAL/startup glue) ----------------------------------
SRC_C := \
  core/stm32f1xx_it.c \
  core/stm32f1xx_hal_msp.c \
  core/syscalls.c \
  $(DEVICE_DIR)/Source/Templates/system_stm32f1xx.c

# ---- HAL sources we use --------------------------------------------------
SRC_C += \
  $(HAL_DIR)/Src/stm32f1xx_hal.c \
  $(HAL_DIR)/Src/stm32f1xx_hal_cortex.c \
  $(HAL_DIR)/Src/stm32f1xx_hal_rcc.c \
  $(HAL_DIR)/Src/stm32f1xx_hal_rcc_ex.c \
  $(HAL_DIR)/Src/stm32f1xx_hal_gpio.c \
  $(HAL_DIR)/Src/stm32f1xx_hal_flash.c \
  $(HAL_DIR)/Src/stm32f1xx_hal_flash_ex.c \
  $(HAL_DIR)/Src/stm32f1xx_hal_pwr.c

# ---- C++ sources (application / framework) -------------------------------
SRC_CXX := \
  core/main.cpp \
  core/cpp_runtime.cpp \
  bsp/bsp.cpp

# ---- Assembly (startup) --------------------------------------------------
SRC_ASM := $(DEVICE_DIR)/Source/Templates/gcc/startup_stm32f103xb.s

# ---- Linker --------------------------------------------------------------
LDSCRIPT := bsp/STM32F103C8Tx_FLASH.ld
LIBS     := -lc -lm -lnosys
LDFLAGS  := $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBS) \
            -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections \
            -Wl,--no-warn-rwx-segments

# ---- Common compiler flags -----------------------------------------------
OPT      := -Og -g3
COMMON   := $(MCU) $(DEFS) $(INCLUDES) $(OPT) \
            -Wall -Wextra -fdata-sections -ffunction-sections \
            -fno-common -MMD -MP

CFLAGS   := $(COMMON) -std=gnu11
# C++14, freestanding conventions: no exceptions / RTTI, no thread-safe
# statics (single-core, cooperative init), no heavy libstdc++ pull-in.
CXXFLAGS := $(COMMON) -std=gnu++14 \
            -fno-exceptions -fno-rtti -fno-threadsafe-statics \
            -fno-use-cxa-atexit -Wno-register

# ---- Objects -------------------------------------------------------------
OBJECTS  := $(addprefix $(BUILD_DIR)/,$(notdir $(SRC_C:.c=.o)))
OBJECTS  += $(addprefix $(BUILD_DIR)/,$(notdir $(SRC_CXX:.cpp=.o)))
OBJECTS  += $(addprefix $(BUILD_DIR)/,$(notdir $(SRC_ASM:.s=.o)))
vpath %.c   $(sort $(dir $(SRC_C)))
vpath %.cpp $(sort $(dir $(SRC_CXX)))
vpath %.s   $(sort $(dir $(SRC_ASM)))

# ==========================================================================
.PHONY: all deps flash monitor clean size

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin size

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

# Link with g++ so the (nano) C++ runtime resolves cleanly.
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O binary -S $< $@

$(BUILD_DIR):
	mkdir -p $@

size: $(BUILD_DIR)/$(TARGET).elf
	$(SIZE) $<

# ---- Dependency fetch ----------------------------------------------------
# STM32CubeF1 ships the HAL driver and CMSIS device as git submodules.
# We fetch ONLY those two (not the ~50 BSP/middleware submodules).
deps:
	@mkdir -p $(VENDOR_DIR)
	@test -d $(CUBE_DIR) || git clone --depth 1 $(CUBE_REPO) $(CUBE_DIR)
	@echo "Fetching required submodules (HAL driver + CMSIS device)..."
	@cd $(CUBE_DIR) && git submodule update --init --depth 1 \
	    Drivers/STM32F1xx_HAL_Driver \
	    Drivers/CMSIS/Device/ST/STM32F1xx
	@echo "Vendor dependencies ready."

# ---- Flash / debug -------------------------------------------------------
# Blue Pill via ST-Link: `make flash`  (uses st-flash from stlink-tools)
flash: $(BUILD_DIR)/$(TARGET).bin
	st-flash write $(BUILD_DIR)/$(TARGET).bin 0x08000000

monitor:
	@echo "Open a 115200 8N1 serial terminal on the diagnostic UART (USART1: PA9/PA10)."
	@echo "e.g.  picocom -b 115200 /dev/ttyUSB0"

clean:
	rm -rf $(BUILD_DIR)

-include $(wildcard $(BUILD_DIR)/*.d)
