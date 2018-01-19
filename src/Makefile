# Makefile for Gnuk

# Define project name here
PROJECT = gnuk

CHOPSTX = ../chopstx

CSRC = main.c call-rsa.c \
	usb_desc.c usb_ctrl.c \
	usb-ccid.c openpgp.c ac.c openpgp-do.c flash.c \
	bn.c mod.c \
	modp256r1.c jpc_p256r1.c ec_p256r1.c call-ec_p256r1.c \
	modp256k1.c jpc_p256k1.c ec_p256k1.c call-ec_p256k1.c \
	mod25638.c ecc-edwards.c ecc-mont.c sha512.c \
	random.c neug.c sha256.c

INCDIR =

CRYPTDIR = ../polarssl
CRYPTSRCDIR = $(CRYPTDIR)/library
CRYPTINCDIR = $(CRYPTDIR)/include
CRYPTSRC = $(CRYPTSRCDIR)/bignum.c $(CRYPTSRCDIR)/rsa.c $(CRYPTSRCDIR)/aes.c

CSRC += $(CRYPTSRC)
INCDIR += $(CRYPTINCDIR)

include config.mk

USE_SYS = yes
USE_USB = yes
USE_ADC = yes
USE_EVENTFLAG = yes

ifeq ($(EMULATION),)
DEFS += -DFLASH_UPGRADE_SUPPORT
else
DEFS += -DBN256_C_IMPLEMENTATION
endif

ifneq ($(ENABLE_DEBUG),)
CSRC += debug.c
endif

ifneq ($(ENABLE_PINPAD),)
CSRC += pin-$(ENABLE_PINPAD).c
endif

ifeq ($(ENABLE_PINPAD),dnd)
CSRC += usb-msc.c
endif

ifeq ($(CHIP),stm32f103)
CSRC += mcu-stm32f103.c 
endif

###################################
CC   = $(CROSS)gcc
LD   = $(CROSS)gcc
OBJCOPY   = $(CROSS)objcopy

CWARN = -Wall -Wextra -Wstrict-prototypes
OPT   = -O3 -Os -g

#######################
include $(CHOPSTX)/rules.mk

board.h:
	@echo Please run configure to have a symbolic link \'board.h\'
	@exit 1

sys.c: board.h

build/bignum.o: OPT = -O3 -g

distclean: clean
	-rm -f gnuk.ld config.h board.h config.mk \
	       usb-strings.c.inc usb-vid-pid-ver.c.inc

ifeq ($(EMULATION),)
build/gnuk-vidpid.elf: build/gnuk.elf binary-edit.sh put-vid-pid-ver.sh
	cp -p build/gnuk.elf build/gnuk-vidpid.elf
	bash put-vid-pid-ver.sh
	$(OBJCOPY) -O ihex build/gnuk-vidpid.elf build/gnuk-vidpid.hex
	$(OBJCOPY) -O binary build/gnuk-vidpid.elf build/gnuk-vidpid.bin
else
# By specifying DESTDIR on invocation of "make", you can install
# program to different ROOT.

# The variables prefix, exec_prefix, libexecdir are defined in
# config.mk.

install: build/gnuk
	test -d "$(DESTDIR)$(libexecdir)" || mkdir -p "$(DESTDIR)$(libexecdir)"
	install -c build/gnuk "$(DESTDIR)$(libexecdir)"

endif
