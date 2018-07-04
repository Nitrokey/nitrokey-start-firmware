# This is a Bash script to be included.

# Idx Name          Size      VMA       LMA       File off  Algn
# =================
#   2 .text         00004a40  080010f0  080010f0  000110f0  2**4
# 08006550 l     O .text	00000012 device_desc
# =================
# VMA =0x080010f0
# FOFF=0x000110f0
# ADDR=0x08005ad0
# file_off_ADDR = ADDR - VMA + FOFF
#               = 0x08005ad0 - 0x080010f0 + 0x000110f0 = 0x00015ad0

function calc_addr () {
    local line_sym="" VMA FOFF ADDR

    arm-none-eabi-objdump -h -t -j .text $FILE       | \
    egrep -e '(^ +[0-9] +\.text +|device_desc)' | \
    while read -r F0 F1 F2 F3 F4 F5 F6; do
	if [ -z "$line_sym" ]; then
	    VMA=$F3
	    FOFF=$F5
	    line_sym="next is a line for the symbol"
	else
	    ADDR=$F0
	    echo "$((0x$ADDR - 0x$VMA + 0x$FOFF))"
	fi
    done
}

declare -a OFFSETS
OFFSETS=($(calc_addr))
file_off_ADDR=${OFFSETS[0]}
file_off_fraucheky_ADDR=${OFFSETS[1]}

echo "Offset is $file_off_ADDR"
if [ -n "$file_off_fraucheky_ADDR" ]; then
    echo "Offset is $file_off_fraucheky_ADDR"
fi

function replace_file_byte_at () {
    printf "\x$1" | dd of=$FILE bs=1 seek=$2 conv=notrunc >& /dev/null
}

#
# vid_lsb:         8
# vid_msb:         9
# pid_lsb:        10
# pid_msb:        11
# bcd_device_lsb: 12
# bcd_device_msb: 13
#

function replace_vid_lsb () {
    replace_file_byte_at $1 $((addr + 8))
}

function replace_vid_msb () {
    replace_file_byte_at $1 $((addr + 9))
}

function replace_pid_lsb () {
    replace_file_byte_at $1 $((addr + 10))
}

function replace_pid_msb () {
    replace_file_byte_at $1 $((addr + 11))
}

function replace_bcd_device_lsb () {
    replace_file_byte_at $1 $((addr + 12))
}

function replace_bcd_device_msb () {
    replace_file_byte_at $1 $((addr + 13))
}
