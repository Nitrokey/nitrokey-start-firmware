R=random.bin
REPORTS=ent.txt ent-b.txt dieharder.txt rngtest.txt
all: $(REPORTS)

$(R):
	@echo "Requires pynitrokey v0.4.17+"
	nitropy start rng --count 50000 --raw > $@

ent.txt: $(R)
	ent $< >$@

ent-b.txt: $(R)
	ent -b $< >$@

dieharder.txt: $(R)
	@echo "This might take some time"
	dieharder -a -g 201 -f $< 2>&1 > $@

rngtest.txt: $(R)
	rngtest -c 1000 < $< 2>$@

.PHONY: show
show: $(REPORTS)
	cat $(REPORTS)

clean:
	-rm $(REPORTS) $(R)
