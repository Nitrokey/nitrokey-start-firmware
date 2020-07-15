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

ifneq ($(USE_DFU),)
OBJS_ADD += build/stdaln-sys-bin.o
endif

###################################
CC   = $(CROSS)gcc
LD   = $(CROSS)gcc
OBJCOPY   = $(CROSS)objcopy
SIZE   = $(CROSS)size

CWARN = -Wall -Wextra -Wstrict-prototypes
OPT   = -O3 -Os -g

#######################
include $(CHOPSTX)/rules.mk

board.h:
	@echo Please run configure to have a symbolic link \'board.h\'
	@exit 1

sys.c: board.h

build/bignum.o: OPT = -O3 -g

build/stdaln-sys.elf: build/sys-$(CHIP).o stdaln-sys.ld
	@echo
	$(LD) -v $< $(MCFLAGS) -nostartfiles -Tstdaln-sys.ld -Wl,--no-warn-mismatch,--gc-sections $(LLIBDIR) -o $@

build/stdaln-sys-bin.o: build/stdaln-sys.elf
	@echo
	$(OBJCOPY) -O binary -j .sys $< build/stdaln-sys.bin
	$(OBJCOPY) -I binary -O default --rename-section .data=.rodata \
		build/stdaln-sys.bin $@

distclean: clean
	-rm -f gnuk.ld stdaln-sys.ld config.h board.h config.mk \
	       usb-strings.c.inc put-vid-pid-ver.sh

ifeq ($(EMULATION),)
build/gnuk.elf: build/gnuk-no-vidpid.elf binary-edit.sh put-vid-pid-ver.sh
	cp -p build/gnuk-no-vidpid.elf build/gnuk.elf
	env FILE="build/gnuk.elf" bash put-vid-pid-ver.sh
	$(OBJCOPY) -O ihex build/gnuk.elf build/gnuk.hex
	$(OBJCOPY) -O binary build/gnuk.elf build/gnuk.bin
	$(SIZE) -A -t -x $@
	ls -l build/gnuk.elf build/gnuk.bin build/gnuk.hex
else
# By specifying DESTDIR on invocation of "make", you can install
# program to different ROOT.

# The variables prefix, exec_prefix, libexecdir are defined in
# config.mk.

install: build/gnuk
	test -d "$(DESTDIR)$(libexecdir)" || mkdir -p "$(DESTDIR)$(libexecdir)"
	install -c build/gnuk "$(DESTDIR)$(libexecdir)"

endif
