PROCESSOR := ARM9

TARGET := $(shell basename "$(CURDIR)")

SOURCE := source
BUILD  := build

SUBARCH := -D$(PROCESSOR) -march=armv5te -mtune=arm946e-s -mthumb -mfloat-abi=soft
INCDIRS := source source/common source/filesys source/crypto source/fatfs source/nand source/virtual source/game source/gamecart source/lodepng source/lua source/qrcodegen source/system source/utils
INCLUDE := $(foreach dir,$(INCDIRS),-I"$(shell pwd)/$(dir)")

ASFLAGS += $(SUBARCH) $(INCLUDE)
CFLAGS  += $(SUBARCH) $(INCLUDE) -fno-builtin-memcpy -flto
LDFLAGS += $(SUBARCH) -Wl,--use-blx,-Map,$(TARGET).map -flto
LIBS    += -lm

include ../Makefile.common
include ../Makefile.build

arm9_data.elf: arm9.elf
	$(OBJCOPY) -O elf32-littlearm -j .rodata* -j .data* -j .bss* $< $@

arm9_code.elf: arm9.elf
	$(OBJCOPY) -O elf32-littlearm -j .text* -j .vectors* $< $@
