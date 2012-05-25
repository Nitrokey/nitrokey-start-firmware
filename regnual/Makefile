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
TOPT = -mthumb -DTHUMB -mno-thumb-interwork
# Define C warning options here
CWARN = -Wall -Wextra -Wstrict-prototypes
MCFLAGS= -mcpu=$(MCU) -mfix-cortex-m3-ldrd
DEFS = -DFREE_STANDING

CFLAGS = -O2 -g
CFLAGS += $(CWARN) -I ../src -fno-common $(MCFLAGS) $(TOPT) $(DEFS)

LDFLAGS = -T$(LDSCRIPT) -nostartfiles $(MCFLAGS) $(TOPT)


####################

all: regnual.hex

regnual.o: regnual.c usb_lld.h

regnual.hex: regnual.elf
	$(OBJCOPY) -Obinary regnual.elf regnual.bin
	$(OBJCOPY) -Oihex regnual.elf regnual.hex

usb_lld.c: ../src/usb_lld.c usb_lld.h
	cp -p ../src/usb_lld.c .

usb_lld.h: ../src/usb_lld.h
	cp -p ../src/usb_lld.h .

regnual.elf: $(OBJS) $(LDSCRIPT)
	$(CC) $(LDFLAGS) -o regnual.elf $(OBJS)

clean:
	-rm -f $(OBJS) regnual.elf regnual.hex regnual.bin

distclean: clean
	-rm -f usb_lld.c usb_lld.h sys.h
