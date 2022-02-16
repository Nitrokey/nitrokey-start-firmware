#!/bin/bash

# This script builds all firmware versions for Nitrokey Start
# whether it is an upgrade from RTM.1 or RTM.2
# it might be run with the parallel tool:
# parallel bash build_all.sh ::: RTM.2 RTM.3 ::: green red
# sudo date ; parallel --delay 2s -u bash build_all.sh ::: RTM.7 ::: green red
# synopsis: bash build_all.sh <firmware tag> [green|red]
# where green means upgrade from RTM.1 and red - upgrade from RTM.2

set -x
set -eou pipefail

gtag=$1
upgrade_from_rtm1=$2

tag="${gtag}_${upgrade_from_rtm1}"

sudo rm -rf ./$tag
git clone -b $gtag https://github.com/Nitrokey/nitrokey-start-firmware.git --recursive --depth 1 --shallow-submodules $tag

pushd $tag/chopstx
if [ ${upgrade_from_rtm1} == "green" ] ; then
  cp board/board-nitrokey-start{-green_LED,}.h -v -b
fi
popd


export GNUK_CONFIG="--target=NITROKEY_START-g --vidpid=20a0:4211 --enable-factory-reset --enable-certdo"
	pushd $tag/src/
	./configure ${GNUK_CONFIG}
	popd

touch $tag/release/last-build
pushd $tag/docker/
sudo env GNUK_CONFIG="${GNUK_CONFIG}" make
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

echo "Results are in ${dirname} directory"
ls -la ${dirname}
