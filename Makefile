prefix = .
intermediate = ./intermediate
bindir = $(prefix)/bin
export bindir

all: dns text2byte rawsend install
dns: dns.c
	gcc dns.c -o $(intermediate)/$@

text2byte: text2byte.c
	gcc text2byte.c -o $(intermediate)/$@

rawsend: rawsend.c
	gcc rawsend.c -o $(intermediate)/$@

install:
	install -m 0755 $(intermediate)/dns  $(bindir)
	install -m 0755 $(intermediate)/text2byte $(bindir)
	install -m 0755 $(intermediate)/rawsend $(bindir)

clean:
	rm -fr $(intermediate)/*
	rm -fr $(bindir)/*
