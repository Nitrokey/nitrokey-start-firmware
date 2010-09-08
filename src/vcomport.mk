VCOMDIR = ../Virtual_COM_Port
VCOMSRC= $(VCOMDIR)/usb_istr.c $(VCOMDIR)/usb_pwr.c
ifneq ($(ENABLE_VCOMPORT),)
VCOMSRC += usb_endp.c
endif
