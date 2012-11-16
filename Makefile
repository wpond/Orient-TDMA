####################################################################
# Makefile                                                         #
####################################################################

.SUFFIXES:				# ignore builtin rules
.PHONY: all debug release clean

####################################################################
# Definitions                                                      #
####################################################################

DEVICE = EFM32GG990F1024
PROJECTNAME = orient

OBJ_DIR = build
EXE_DIR = exe
LST_DIR = lst

####################################################################
# Definitions of toolchain.                                        #
# You might need to do changes to match your system setup          #
####################################################################

# Change path to CodeSourcery tools according to your system configuration
WINDOWSCS = C:/CodeSourcery/Sourcery_CodeBench_Lite_for_ARM_EABI
LINUXCS   = /cad/codesourcery/arm-none-eabi/arm-2010q1
GCCVERSION = $(shell $(CC) -dumpversion)

TOOLDIR = C:/CodeSourcery/Sourcery_CodeBench_Lite_for_ARM_EABI
RM = rm -rf

CC      = $(QUOTE)$(TOOLDIR)/bin/arm-none-eabi-gcc$(QUOTE)
LD      = $(QUOTE)$(TOOLDIR)/bin/arm-none-eabi-ld$(QUOTE)
AR      = $(QUOTE)$(TOOLDIR)/bin/arm-none-eabi-ar$(QUOTE)
OBJCOPY = $(QUOTE)$(TOOLDIR)/bin/arm-none-eabi-objcopy$(QUOTE)
DUMP    = $(QUOTE)$(TOOLDIR)/bin/arm-none-eabi-objdump$(QUOTE) --disassemble
GDB     = $(QUOTE)$(TOOLDIR)/bin/arm-none-eabi-gdb$(QUOTE)
                                                      
####################################################################
# Flags                                                            #
####################################################################

# -MMD : Don't generate dependencies on system header files.
# -MP  : Add phony targets, useful when a h-file is removed from a project.
# -MF  : Specify a file to write the dependencies to.
DEPFLAGS = -MMD -MP -MF $(@:.o=.d)

# Add -Wa,-ahld=$(LST_DIR)/$(@F:.o=.lst) to CFLAGS to produce assembly list files
CFLAGS += -std=c99 -D$(DEVICE) -mcpu=cortex-m3 -mthumb -ffunction-sections -fno-short-enums -fdata-sections \
-mfix-cortex-m3-ldrd -fomit-frame-pointer -Wall -fwide-exec-charset=UTF-16LE -fshort-wchar $(DEPFLAGS)

ASMFLAGS += -Ttext 0x0                        

LDFLAGS += -Xlinker -Map=$(LST_DIR)/$(PROJECTNAME).map -mcpu=cortex-m3 -mthumb \
-TCMSIS/CM3/DeviceSupport/EnergyMicro/EFM32/startup/cs3/efm32gg.ld -L"$(TOOLDIR)/arm-none-eabi/lib/thumb2" \
-L"$(TOOLDIR)/lib/gcc/arm-none-eabi/$(GCCVERSION)/thumb2" \
-Wl,--gc-sections -Wl,--no-wchar-size-warning

LIBS += -lc -lcs3 -lcs3unhosted

INCLUDEPATHS += \
-I.. \
-I. \
-ICMSIS/CM3/CoreSupport \
-ICMSIS/CM3/DeviceSupport/EnergyMicro/EFM32 \
-Iefm32lib/inc \
-Iefm32usb/inc \
-Ifatfs/src \
-Idrivers \
-Itasks \
-Isystem \
-Iincludes

####################################################################
# Files                                                            #
####################################################################

C_SRC +=  \
CMSIS/CM3/CoreSupport/core_cm3.c \
CMSIS/CM3/DeviceSupport/EnergyMicro/EFM32/system_efm32.c \
efm32lib/src/efm32_assert.c \
efm32lib/src/efm32_system.c \
efm32lib/src/efm32_gpio.c \
efm32lib/src/efm32_cmu.c \
efm32lib/src/efm32_usart.c \
efm32lib/src/efm32_i2c.c \
efm32lib/src/efm32_dma.c \
efm32lib/src/efm32_timer.c \
efm32lib/src/efm32_letimer.c \
efm32lib/src/efm32_int.c \
efm32lib/src/efm32_emu.c \
efm32lib/src/efm32_adc.c \
efm32lib/src/efm32_rtc.c \
efm32usb/src/efm32_usbtimer.c \
efm32usb/src/efm32_usbd.c \
efm32usb/src/efm32_usbdch9.c \
efm32usb/src/efm32_usbdep.c \
efm32usb/src/efm32_usbdint.c \
efm32usb/src/efm32_usbhal.c \
system/usart.c \
system/usb.c \
system/queue.c \
tasks/radio_init_task.c \
tasks/basestation_radio_task.c \
tasks/node_radio_task.c \
main.c \
led.c \
scheduler.c 

S_SRC +=  \
CMSIS/CM3/DeviceSupport/EnergyMicro/EFM32/startup/cs3/startup_efm32gg.s

####################################################################
# Rules                                                            #
####################################################################

C_FILES = $(notdir $(C_SRC) )
S_FILES = $(notdir $(S_SRC) )
#make list of source paths, sort also removes duplicates
C_PATHS = $(sort $(dir $(C_SRC) ) )
S_PATHS = $(sort $(dir $(S_SRC) ) )

C_OBJS = $(addprefix $(OBJ_DIR)/, $(C_FILES:.c=.o))
S_OBJS = $(addprefix $(OBJ_DIR)/, $(S_FILES:.s=.o))
C_DEPS = $(addprefix $(OBJ_DIR)/, $(C_FILES:.c=.d))

vpath %.c $(C_PATHS)
vpath %.s $(S_PATHS)

# Default build is debug build
all:      debug

debug:    CFLAGS += -DDEBUG -O0 -g3
debug:    $(OBJ_DIR) $(LST_DIR) $(EXE_DIR) $(EXE_DIR)/$(PROJECTNAME).bin

release:  CFLAGS += -DNDEBUG -O3 
release:  $(OBJ_DIR) $(LST_DIR) $(EXE_DIR) $(EXE_DIR)/$(PROJECTNAME).bin

# Create directories
$(OBJ_DIR):
	mkdir $(OBJ_DIR)
	@echo "Created build directory."

$(EXE_DIR):
	mkdir $(EXE_DIR)
	@echo "Created executable directory."

$(LST_DIR):
	mkdir $(LST_DIR)
	@echo "Created list directory."

# Create objects from C SRC files
$(OBJ_DIR)/%.o: %.c
	@echo "Building file: $<"
	$(CC) $(CFLAGS) $(INCLUDEPATHS) -c -o $@ $<

# Assemble .s files
$(OBJ_DIR)/%.o: %.s
	@echo "Assembling $<"
	$(CC) $(ASMFLAGS) $(INCLUDEPATHS) -c -o $@ $<

# Link
$(EXE_DIR)/$(PROJECTNAME).elf: $(C_OBJS) $(S_OBJS)
	@echo "Linking target: $@"
	$(CC) $(LDFLAGS) $(C_OBJS) $(S_OBJS) $(LIBS) -o $(EXE_DIR)/$(PROJECTNAME).elf

# Create binary file
$(EXE_DIR)/$(PROJECTNAME).bin: $(EXE_DIR)/$(PROJECTNAME).elf
	@echo "Creating binary file"
	$(OBJCOPY) -O binary $(EXE_DIR)/$(PROJECTNAME).elf $(EXE_DIR)/$(PROJECTNAME).bin
# Uncomment next line to produce assembly listing of entire program
#	$(DUMP) $(EXE_DIR)/$(PROJECTNAME).out>$(LST_DIR)/$(PROJECTNAME)out.lst

program: $(EXE_DIR)/$(PROJECTNAME).elf
	@echo "Programming"
	$(GDB) --se $(EXE_DIR)/$(PROJECTNAME).elf

clean:
	$(RM) $(OBJ_DIR) $(LST_DIR) $(EXE_DIR)

# include auto-generated dependency files (explicit rules)
ifneq (clean,$(findstring clean, $(MAKECMDGOALS)))
-include $(C_DEPS)
endif