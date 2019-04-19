# Makefile for reGNUal

PROJECT = regnual-no-vidpid

OBJS = regnual.o usb-stm32f103.o reset.o

include ../src/config.mk

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
DEFS += -DFREE_STANDING

CFLAGS = -O2 -g
CFLAGS += -Wa,-alms=$(notdir $(<:.c=.lst)) -fpie
CFLAGS += $(CWARN) -I . -I ../chopstx -fno-common $(MCFLAGS) $(TOPT) $(DEFS)

LDFLAGS = -T$(LDSCRIPT) -nostartfiles $(MCFLAGS) $(TOPT)


####################

all: regnual.hex

regnual.o: regnual.c ../chopstx/sys.h ../chopstx/usb_lld.h

regnual.hex: regnual.elf
	$(OBJCOPY) -Obinary regnual.elf regnual.bin
	$(OBJCOPY) -Oihex regnual.elf regnual.hex

regnual.elf: regnual-no-vidpid.elf
	cp -p regnual-no-vidpid.elf regnual.elf
	env FILE="regnual.elf" PATH="../src:$$PATH" bash put-vid-pid-ver.sh

usb-stm32f103.o: ../chopstx/mcu/usb-stm32f103.c
	$(CC) $(CFLAGS) -c -o usb-stm32f103.o ../chopstx/mcu/usb-stm32f103.c

regnual-no-vidpid.elf: $(OBJS) $(LDSCRIPT)
	$(CC) $(LDFLAGS) -o regnual-no-vidpid.elf $(OBJS)

clean:
	-rm -f $(OBJS) regnual-no-vidpid.elf regnual.elf regnual.hex regnual.bin \
		*.lst

distclean: clean
