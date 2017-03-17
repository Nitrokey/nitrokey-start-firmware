#make in src
#make in regnual
stm32flash  -k /dev/ttyUSB0
stm32flash  -u /dev/ttyUSB0
#stm32flash  -w src/build/gnuk.bin /dev/ttyUSB0
stm32flash -v -w $1 /dev/ttyUSB0
stm32flash  -j /dev/ttyUSB0
