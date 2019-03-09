#ifndef MCP2200_LIB_H
#define MCP2200_LIB_H

#include "ac_cfg.h"

#if defined(HAVE_LIBUSB_1_0_LIBUSB_H)
#include <libusb-1.0/libusb.h>
#else
#include <libusb.h>
#endif

#define MCP2200_VID 0x04d8
#define MCP2200_PID 0x00df

#define MCP2200_PINALT_TXLED     4
#define MCP2200_PINALT_RXLED     8
#define MCP2200_PINALT_CONFIG   64
#define MCP2200_PINALT_SUSPEND 128

#define MCP2200_PINOPT_RXTGL   128
#define MCP2200_PINOPT_TXTGL    64
#define MCP2200_PINOPT_LEDX     32
#define MCP2200_PINOPT_INVERT    2
#define MCP2200_PINOPT_HWFLOW    1

struct mcp2200{
  libusb_device_handle *handle;
  int intin, intout, interface;
  };

struct mcp2200_conf{
  int pindir, pinalts, pindefaults, pinopts, pinvals;
  long baudrate;
  };

int mcp2200_open(struct mcp2200 *m, long vid, long pid, long bus, long port);
void mcp2200_close(struct mcp2200 *m);
int mcp2200_gpio_on_off(struct mcp2200 *mcp2200, int onmask, int offmask);
int mcp2200_gpio_on(struct mcp2200 *mcp2200, int onmask);
int mcp2200_gpio_off(struct mcp2200 *mcp2200, int offmask);
int mcp2200_gpio_set(struct mcp2200 *mcp2200, int bits);
int mcp2200_gpio_get(struct mcp2200 *mcp2200, int *bits);
int mcp2200_read_all(struct mcp2200 *mcp2200, struct mcp2200_conf *conf);
int mcp2200_configure(struct mcp2200 *mcp2200, struct mcp2200_conf *conf);
int mcp2200_read_eeprom(struct mcp2200 *mcp2200, int address);
int mcp2200_write_eeprom(struct mcp2200 *mcp2200, int address, int data);
int mcp2200_set_manufacturer_string(struct mcp2200 *mcp2200, char *str);
int mcp2200_set_product_string(struct mcp2200 *mcp2200, char *str);
int mcp2200_set_pid_vid(struct mcp2200 *mcp2200, int pid, int vid);

#endif
