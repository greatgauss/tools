prefix = .
intermediate = ./intermediate
bindir = $(prefix)/bin
export bindir

all: dns install
dns: dns.c
	gcc dns.c -o $(intermediate)/$@ 

install:
	install -m 0755 $(intermediate)/dns  $(bindir)

clean:
	rm -fr $(intermediate)/* 
