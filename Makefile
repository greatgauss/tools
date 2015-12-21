prefix = .
intermediate = ./intermediate
bindir = $(prefix)/bin
export bindir
flags = -g -Wall
all: dns text2byte rawsend rawrecv igmptools
dns: dns.c
	gcc $(flags) dns.c -o $(intermediate)/$@

text2byte: text2byte.c
	gcc $(flags) text2byte.c -o $(intermediate)/$@

rawsend: rawsend.c
	gcc $(flags) rawsend.c lib/netutils.c -Iinclude -o $(intermediate)/$@

rawrecv: rawrecv.c
	gcc $(flags) rawrecv.c lib/netutils.c -Iinclude -o $(intermediate)/$@

igmptools: igmptools.c
	gcc $(flags) igmptools.c lib/netutils.c -Iinclude -o $(intermediate)/$@

install:
	install -m 0755 $(intermediate)/dns  $(bindir)
	install -m 0755 $(intermediate)/text2byte $(bindir)
	install -m 0755 $(intermediate)/rawsend $(bindir)
	install -m 0755 $(intermediate)/rawrecv $(bindir)
	install -m 0755 $(intermediate)/igmptools $(bindir)

clean:
	rm -fr $(intermediate)/*
	rm -fr $(bindir)/*
