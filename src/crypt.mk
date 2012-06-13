CRYPTDIR = ../polarssl-0.14.0
CRYPTSRCDIR = $(CRYPTDIR)/library
CRYPTINCDIR = $(CRYPTDIR)/include
CRYPTSRC = $(CRYPTSRCDIR)/bignum.c $(CRYPTSRCDIR)/rsa.c \
	$(CRYPTSRCDIR)/aes.c \
	sha256.c call-rsa.c
