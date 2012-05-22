# Makefile for reGNUal

PROJECT = regnual

SRCS = regnual.c usb_lld.c sys.c
OBJS = regnual.o usb_lld.o sys.o
LDSCRIPT= regnual.ld

###################################
MCU  = cortex-m3

TRGT = arm-none-eabi-
CC   = $(TRGT)gcc
LD   = $(TRGT)ld
OBJCOPY   = $(TRGT)objcopy
OBJDUMP   = $(TRGT)objdump

# THUMB-specific options here
TOPT = -mthumb -DTHUMB
# Define C warning options here
CWARN = -Wall -Wextra -Wstrict-prototypes
MCFLAGS= -mcpu=$(MCU) -mfix-cortex-m3-ldrd
DEFS = -DFREE_STANDING

CFLAGS = -O2 -g
CFLAGS += $(CWARN) -fno-common $(MCFLAGS) $(TOPT) $(DEFS)

LDFLAGS = -T$(LDSCRIPT) -nostartfiles


####################

all: regnual.hex

regnual.hex: regnual.elf
	$(OBJCOPY) -Obinary regnual.elf regnual.bin
	$(OBJCOPY) -Oihex regnual.elf regnual.hex

regnual.elf: $(OBJS) $(LDSCRIPT)
	$(CC) $(LDFLAGS) -o regnual.elf $(OBJS)

clean:
	-rm -f $(OBJS) regnual.elf regnual.hex regnual.bin

distclean: clean
	-rm -f Makefile regnual.ld
