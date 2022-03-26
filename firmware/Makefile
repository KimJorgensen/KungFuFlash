ifndef version
version := 1.XX
endif
export version

######################################
# target
######################################
TARGET = KungFuFlash_v$(version)

LAUNCHER_DIR = ../launcher/
LAUNCHER = $(LAUNCHER_DIR)build/KungFuLauncher_v$(version).bin

######################################
# building variables
######################################
# debug build?
DEBUG = 0 #1
# optimization
OPT = -Os #-Og

#######################################
# paths
#######################################
# Build path
BUILD_DIR = build

######################################
# source
######################################
# C sources
C_SOURCES =  \
main.c \
fatfs/ff.c \
fatfs/ffunicode.c

# ASM sources
ASM_SOURCES =  \
stm32f4xx/startup_stm32f405xx.s

#######################################
# binaries
#######################################
PREFIX = arm-none-eabi-
# The gcc compiler bin path can be either defined in make command via GCC_PATH variable (> make GCC_PATH=xxx)
# either it can be added to the PATH environment variable.
ifdef GCC_PATH
CC = $(GCC_PATH)/$(PREFIX)gcc
AS = $(GCC_PATH)/$(PREFIX)gcc -x assembler-with-cpp
CP = $(GCC_PATH)/$(PREFIX)objcopy
SZ = $(GCC_PATH)/$(PREFIX)size
DB = $(GCC_PATH)/$(PREFIX)gdb
else
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
DB = $(PREFIX)gdb
endif
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

#######################################
# CFLAGS
#######################################
# cpu
CPU = -mcpu=cortex-m4

# fpu
FPU = -mfpu=fpv4-sp-d16

# float-abi
FLOAT-ABI = -mfloat-abi=hard

# mcu
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

# macros for gcc
# AS defines
AS_DEFS =

# C defines
C_DEFS =  \
-DSTM32F405xx \
-DVERSION=\"${version}\"

# AS includes
AS_INCLUDES =

# C includes
C_INCLUDES =  \
-I.  \
-Istm32f4xx  \
-Istm32f4xx/cmsis  \
-Istm32f4xx/libusb_stm32  \
-Ifatfs \
-Icartridges

# compile gcc flags
ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fsingle-precision-constant -fomit-frame-pointer -fdata-sections -ffunction-sections -Wno-unused-function

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif

# Generate dependency information
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

#######################################
# LDFLAGS
#######################################
# link script
LDSCRIPT = stm32f4xx/STM32F405RGTx_FLASH.ld

# libraries
LIBS = -lc -lm -lnosys
LIBDIR =
LDFLAGS_COMMON = $(MCU) -specs=nano.specs $(LIBDIR) $(LIBS) -Wl,--gc-sections
LDFLAGS = $(LDFLAGS_COMMON) -T$(LDSCRIPT) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref

# default action: build all
all: $(BUILD_DIR)/$(TARGET).upd

#######################################
# build the application
#######################################
# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.s=.lst)) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf | $(BUILD_DIR)
	$(BIN) --gap-fill 0xff --pad-to 0xc000 $< $@

$(BUILD_DIR)/$(TARGET)_mem.bin: $(BUILD_DIR)/$(TARGET).bin | $(BUILD_DIR)
	@cat $(BUILD_DIR)/$(TARGET).bin > $@

$(BUILD_DIR)/$(TARGET).upd: $(BUILD_DIR)/$(TARGET).bin $(LAUNCHER) | $(BUILD_DIR)
	@cat $(BUILD_DIR)/$(TARGET).bin > $@
	@cat $(LAUNCHER) >> $@

$(BUILD_DIR):
	mkdir -p $@

$(LAUNCHER): $(wildcard $(LAUNCHER_DIR)*)
	$(MAKE) -C $(LAUNCHER_DIR)

#######################################
# Debug
#######################################
debug: all
	@$(DB) --eval-command="target extended-remote :4242" $(BUILD_DIR)/$(TARGET).elf

#######################################
# burn flash
#######################################
burn: all
	@st-flash write $(BUILD_DIR)/$(TARGET).bin 0x8000000
	@#dfu-util -a 0 -s 0x08000000 -D $(BUILD_DIR)/$(TARGET).bin

mem: $(BUILD_DIR)/$(TARGET)_mem.bin
	@st-flash write $< 0x20000000

#######################################
# clean up
#######################################
clean:
	-rm -fR $(BUILD_DIR)
	-$(MAKE) -C $(LAUNCHER_DIR) clean

#######################################
# dependencies
#######################################
-include $(wildcard $(BUILD_DIR)/*.d)

# *** EOF ***
