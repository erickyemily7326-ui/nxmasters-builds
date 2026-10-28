ifeq ($(strip $(DEVKITPRO)),)
$(error "DEVKITPRO no definido")
endif
APP_TITLE ?= Gate
APP_AUTHOR ?= NX masters
TARGET ?= gate
ICON ?= ../asce_installer/icon.jpg
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CC := aarch64-none-elf-gcc
CFLAGS := -O2 -ffunction-sections $(ARCH) -D__SWITCH__ -I$(DEVKITPRO)/libnx/include -I$(DEVKITPRO)/portlibs/switch/include
LDFLAGS := -specs=$(DEVKITPRO)/libnx/switch.specs $(ARCH) -L$(DEVKITPRO)/libnx/lib
all: $(TARGET).nro
$(TARGET).elf: gate.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -lnx -o $@
$(TARGET).nacp:
	$(DEVKITPRO)/tools/bin/nacptool --create "$(APP_TITLE)" "$(APP_AUTHOR)" 1.0.0 $@
$(TARGET).nro: $(TARGET).elf $(TARGET).nacp
	$(DEVKITPRO)/tools/bin/elf2nro $(TARGET).elf $@ --nacp=$(TARGET).nacp --icon=$(ICON)
clean:
	rm -f *.elf *.nro *.nacp
