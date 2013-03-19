CRYPTDIR = ../polarssl
CRYPTSRCDIR = $(CRYPTDIR)/library
CRYPTINCDIR = $(CRYPTDIR)/include
CRYPTSRC = $(CRYPTSRCDIR)/bignum.c $(CRYPTSRCDIR)/rsa.c \
	$(CRYPTSRCDIR)/aes.c \
	sha256.c call-rsa.c
