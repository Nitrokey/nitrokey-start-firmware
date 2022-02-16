R=random.bin
REPORTS=ent.txt ent-b.txt rngtest.txt
all: $(REPORTS)

.PHONY: extended
extended: $(REPORTS) dieharder.txt 

$(R):
	@echo "Requires pynitrokey v0.4.17+"
	nitropy start rng --count 50000 --raw > $@

ent.txt: $(R)
	ent $< >$@

ent-b.txt: $(R)
	ent -b $< >$@

dieharder.txt: $(R)
	@echo "Warning: dieharder needs bigger sample - more than 100MB"
	@echo "This might take some time"
	dieharder -a -g 201 -f $< 2>&1 > $@

rngtest.txt: $(R)
	rngtest -c 1000 < $< 2>$@

.PHONY: show clean setup-ubuntu
show: $(REPORTS)
	cat $(REPORTS)

clean:
	-rm $(REPORTS) $(R)

setup-ubuntu:
	apt update
	apt install ent rng-tools dieharder -qy
