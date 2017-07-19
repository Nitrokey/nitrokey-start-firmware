ifndef GNUK_CONFIG
$(warning configuration flags not set in GNUK_CONFIG)
endif

all: ../chopstx docker-build-release
	docker run --user=`id -u` --env GNUK_CONFIG --rm -v `pwd`/..:/gnuk/ -t gnuk:latest

clean: docker-build-release
	docker run --user=`id -u` --env GNUK_CONFIG --rm -v `pwd`/..:/gnuk/ -w /gnuk/src -t gnuk:latest make clean

gdb: docker-build-debug
	docker run --net host  --rm -i -v `pwd`/..:/gnuk/ -t gnuk:latest-debug arm-none-eabi-gdb /gnuk/src/build/gnuk.elf

shellcheck: docker-build-check
	docker run --rm -v `pwd`/..:/gnuk/ -t gnuk:latest-check shellcheck /gnuk/src/configure

CHECKERS=security optin nullability core deadcode alpha.core alpha.security
scan-build: clean docker-build-check
	docker run --user=`id -u` --rm -v `pwd`/..:/gnuk/ -w /gnuk/src -t gnuk:latest-check scan-build -o scan-build \
		-analyze-headers -stats $(addprefix -enable-checker ,$(CHECKERS)) -k \
		--use-cc=arm-none-eabi-gcc \
		make
../chopstx:
	git submodule update --init

docker-build-release:
	docker build -t gnuk:latest -f `pwd`/Dockerfile.release ..

docker-build-debug: docker-build-release
	docker build -t gnuk:latest-debug -f `pwd`/Dockerfile.debug ..

docker-build-check: docker-build-release
	docker build -t gnuk:latest-check -f `pwd`/Dockerfile.check ..

.PHONY: all clean gdb shellcheck scan-build \
	docker-build-release docker-build-debug docker-build-check
