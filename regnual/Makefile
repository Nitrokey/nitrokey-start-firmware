# Makefile for reGNUal

PROJECT = regnual

OBJS = regnual.o usb_stm32f103.o sys.o
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
MCFLAGS= -mcpu=$(MCU)
DEFS = -DFREE_STANDING

CFLAGS = -O2 -g
CFLAGS += -Wa,-alms=$(notdir $(<:.c=.lst)) -fpie
CFLAGS += $(CWARN) -I . -I ../src -fno-common $(MCFLAGS) $(TOPT) $(DEFS)

LDFLAGS = -T$(LDSCRIPT) -nostartfiles $(MCFLAGS) $(TOPT)


####################

all: regnual.hex

regnual.o: regnual.c ../src/sys.h

regnual.hex: regnual.elf
	$(OBJCOPY) -Obinary regnual.elf regnual.bin
	$(OBJCOPY) -Oihex regnual.elf regnual.hex

usb_stm32f103.o: ../src/usb_stm32f103.c
	$(CC) $(CFLAGS) -c -o usb_stm32f103.o ../src/usb_stm32f103.c

regnual.elf: $(OBJS) $(LDSCRIPT)
	$(CC) $(LDFLAGS) -o regnual.elf $(OBJS)

clean:
	-rm -f $(OBJS) regnual.elf regnual.hex regnual.bin

distclean: clean
