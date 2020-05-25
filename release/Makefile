FIRMWARE_DIR=$(shell readlink -f last-build)
FLASH_FILE=$(FIRMWARE_DIR)/gnuk.hex
TOOLS=../tool
PASS=12345678
all: update

.PHONY: update
update:
	cd $(TOOLS) && ./upgrade_by_passwd.py -p ${PASS} $(FIRMWARE_DIR)/{regnual.bin,gnuk.bin} 
	@echo "Note: power cycle might be needed due to setting RDP flag on MCU, which stalls it"

.PHONY: flash
flash: $(FLASH_FILE)
	STM32_Programmer_CLI -c port=SWD -halt -e all --readunprotect
	STM32_Programmer_CLI -c port=SWD -halt -d $< -rst
	@echo "Written: $<"

.PHONY: reset
reset:
	STM32_Programmer_CLI -c port=SWD -halt -rst

.PHONY: build
build:
	cd ../docker/ && sudo env GNUK_CONFIG="--target=NITROKEY_START --vidpid=20a0:4211 --enable-factory-reset --enable-certdo"  make

