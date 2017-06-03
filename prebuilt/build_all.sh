#!/bin/bash

# This script builds all firmware versions for Nitrokey Start
# whether it is an upgrade from RTM.1 or RTM.2
# it might be run with the parallel tool:
# parallel bash build_all.sh ::: RTM.2 RTM.3 ::: green red
# synopsis: bash build_all.sh <firmware tag> [green|red]
# where green means upgrade from RTM.1 and red - upgrade from RTM.2

set -x

tags="RTM.2 RTM.3"
#tag="RTM.3"
gtag=$1
#upgrade_from_rtm1= green or red
upgrade_from_rtm1=$2

tag="${gtag}_${upgrade_from_rtm1}"

rm -rf ./$tag
git clone -b $gtag git@github.com:Nitrokey/nitrokey-start-firmware.git --recursive $tag

pushd $tag/chopstx
if [ ${upgrade_from_rtm1} == "green" ] ; then
  cp board/board-nitrokey-start{-green_LED,}.h -v -b
fi
popd

pushd $tag/src
./configure --enable-factory-reset --enable-certdo --target=NITROKEY_START --vidpid=20a0:4211
make -j2
popd
pushd $tag/regnual
make -j2
popd

dirname=build/$gtag
if [ ${upgrade_from_rtm1} == "green" ] ; then
  dirname=build/RTM.1_to_$gtag
fi

mkdir -p $dirname
cp $tag/src/build/gnuk.bin $dirname
cp $tag/regnual/regnual.bin $dirname
arm-none-eabi-objcopy -Oihex $tag/src/build/gnuk.elf $dirname/gnuk.hex
gzip -k $dirname/gnuk.hex

pushd build
flock sha.lock -c "sha512sum */* > sums.sha512"
popd
