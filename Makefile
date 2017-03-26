CFLAGS=-Wall $(shell pkg-config libusb-1.0 --cflags) -std=c99
LDFLAGS=$(shell pkg-config libusb-1.0 --libs)


all: mcp2200-tool

mcp2200-tool: mcp2200-tool.c
	$(CC) $(CFLAGS) -o mcp2200-tool mcp2200-tool.c $(LDFLAGS)

clean:
	rm -f *.o mcp2200-tool
