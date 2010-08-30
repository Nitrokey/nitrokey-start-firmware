CRYPTDIR = ../polarssl-0.14.0
CRYPTSRCDIR = $(CRYPTDIR)/library
CRYPTINCDIR = $(CRYPTDIR)/include
CRYPTSRC = $(CRYPTSRCDIR)/bignum.c $(CRYPTSRCDIR)/rsa.c $(CRYPTSRCDIR)/sha1.c \
	$(CRYPTSRCDIR)/aes.c \
	rsa-sign.c
