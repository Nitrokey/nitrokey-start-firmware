FIRMWARE_DIR=$(shell readlink -f last-build)
FLASH_FILE=$(FIRMWARE_DIR)/gnuk.hex
TOOLS=../tool
PASS=12345678
V=0
all: update

UPDATETOOL=nitropy start update

.PHONY: update
update:
	$(UPDATETOOL) -p ${PASS} --regnual $(FIRMWARE_DIR)/regnual.bin --gnuk ${FIRMWARE_DIR}/gnuk.bin -y -v ${V}
	@echo "Note: power cycle might be needed due to setting RDP flag on MCU, which stalls it"

.PHONY: download
download: 
	STM32_Programmer_CLI -c port=SWD -halt -u 0x8000000 128000 download.hex
	@echo "Written to download.hex"

.PHONY: flash
flash: $(FLASH_FILE)
	STM32_Programmer_CLI -c port=SWD -halt --readunprotect -e all
	STM32_Programmer_CLI -c port=SWD -halt -d $< -rst
	@echo "Written: $<"

.PHONY: reset
reset:
	STM32_Programmer_CLI -c port=SWD -halt -rst

#GNUK_CONFIG_EXTRA=--enable-hid-card-change
GNUK_CONFIG=--target=NITROKEY_START-g --vidpid=20a0:4211 --enable-factory-reset --enable-certdo $(GNUK_CONFIG_EXTRA)
.PHONY: build
build:
	sudo touch last-build
	cd ../docker/ && sudo env GNUK_CONFIG="${GNUK_CONFIG}"  make
	@echo "Built with '${GNUK_CONFIG}'"

.PHONY: release
TAG=$(shell basename $(FIRMWARE_DIR))
release:
	7za a $(TAG).zip $(TAG)
	gpg2 --detach-sign $(TAG).zip
	gpg2 --verify $(TAG).zip.sig
	ls -lh $(TAG).zip*

.PHONY: build-hid
build-hid: GNUK_CONFIG_EXTRA=--enable-hid-card-change
build-hid: build

.PHONY: status
status:
	cd ../tool/ && python3 usb_strings.py
