CFLAGS := -std=c99 -Wall -Wextra -O2
CPPFLAGS := -D_FILE_OFFSET_BITS=64
LDFLAGS := -lrt

dvsource-file.o : CFLAGS += $(shell pkg-config --cflags libdv)
dvsource-file : LDFLAGS += $(shell pkg-config --libs libdv)

all : dvsource-file dvsource-firewire

install :
	mkdir -p -m755 $(DESTDIR)/usr/bin
	install dvsource-file dvsource-firewire $(DESTDIR)/usr/bin

clean :
	rm -f dvsource-file dvsource-firewire *.o

dvsource-file : dvsource-file.o config.o socket.o

dvsource-firewire : dvsource-firewire.o config.o socket.o

.PHONY : all install clean
