VCOMDIR = ../Virtual_COM_Port
ifeq ($(ENABLE_VCOMPORT),)
VCOMSRC= $(VCOMDIR)/usb_istr.c $(VCOMDIR)/usb_pwr.c
else
VCOMSRC= $(VCOMDIR)/usb_endp.c $(VCOMDIR)/usb_istr.c $(VCOMDIR)/usb_pwr.c
endif
