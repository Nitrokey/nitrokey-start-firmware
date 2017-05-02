SRCS = usbip-server.c usb-emu.c
OBJS = $(SRCS:.c=.o)
TARGET=gnuk_emulation
GNUKDIR=../src

GNUK_SRCS = main.c call-rsa.c \
	usb-ccid.c openpgp.c ac.c openpgp-do.c flash.c \
	bn.c mod.c \
	modp256r1.c jpc_p256r1.c ec_p256r1.c call-ec_p256r1.c \
	modp256k1.c jpc_p256k1.c ec_p256k1.c call-ec_p256k1.c \
	mod25638.c ecc-edwards.c ecc-mont.c sha512.c \
	random.c neug.c sha256.c
USB_SRCS=usb_desc.c usb_ctrl.c

GNUK_CSRC = $(addprefix $(GNUKDIR)/, $(GNUK_SRCS))
GNUK_OBJS = $(notdir $(GNUK_CSRC:.c=.o))

USB_CSRC = $(addprefix $(GNUKDIR)/, $(USB_SRCS))
USB_OBJS = $(notdir $(USB_CSRC:.c=.o))

# all:
# 	echo $(GNUK_OBJS)

$(TARGET): $(OBJS) $(USB_OBJS) Makefile
	$(CC) -o $(TARGET) $(OBJS) $(USB_OBJS)

$(GNUK_OBJS): %.o : $(GNUKDIR)/%.c Makefile
	$(CC) -c $(CFLAGS) -I. -I$(GNUKDIR) -I../chopstx $< -o $@

$(USB_OBJS): %.o : $(GNUKDIR)/%.c Makefile
	$(CC) -c $(CFLAGS) -I. -I$(GNUKDIR) -I../chopstx $< -o $@
