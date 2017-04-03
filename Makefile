CFLAGS=-Wall $(shell pkg-config libusb-1.0 --cflags) -std=c99 -pedantic
LDFLAGS=$(shell pkg-config libusb-1.0 --libs)


all: mcp2200-tool

mcp2200-tool: mcp2200-tool.c mcp2200-lib.c mcp2200-lib.h
	$(CC) $(CFLAGS) -o mcp2200-tool mcp2200-tool.c \
	                                mcp2200-lib.c  $(LDFLAGS)

clean:
	rm -f *.o mcp2200-tool
