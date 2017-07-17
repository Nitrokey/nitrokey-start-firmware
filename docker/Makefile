ifndef GNUK_CONFIG
$(warning configuration flags not set in GNUK_CONFIG)
endif

all: ../chopstx docker-build-release
	docker run --user=`id -u` --env GNUK_CONFIG --rm -v `pwd`/..:/gnuk/ -t gnuk:latest

gdb: docker-build-debug
	docker run --net host  --rm -i -v `pwd`/..:/gnuk/ -t gnuk:latest-debug arm-none-eabi-gdb /gnuk/src/build/gnuk.elf

../chopstx:
	git submodule update --init

docker-build-release:
	docker build -t gnuk:latest -f `pwd`/Dockerfile.release ..

docker-build-debug: docker-build-release
	docker build -t gnuk:latest-debug -f `pwd`/Dockerfile.debug ..

.PHONY: all gdb debug docker-build docker-build-releaes docker-build-debug
