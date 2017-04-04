#include"mcp2200-lib.h"


#define MCP2200_CMD_GPIO_ON_OFF    8
#define MCP2200_CMD_CONFIGURE     16
#define MCP2200_CMD_READ_EEPROM   32
#define MCP2200_CMD_WRITE_EEPROM  64
#define MCP2200_CMD_READ_ALL     128

static unsigned char report[16];

#define FAIL(n) {ret=n; goto fail;}
int mcp2200_open(struct mcp2200 *m, long vid, long pid, long bus, long port){
  libusb_device **devs;
  ssize_t ndevs;
  libusb_device_handle *handle=0;
  struct libusb_device_descriptor dev_desc;
  struct libusb_config_descriptor *conf_desc=0;
//  unsigned char buf[256];
  int ret=0;
  ssize_t i;
  int j, k, l;

  m->interface=-1;

  if(vid<0)vid=MCP2200_VID;
  if(pid<0)pid=MCP2200_PID;

  libusb_init(0);

  ndevs=libusb_get_device_list(0, &devs);
  for(i=0; i<ndevs; ++i){
    libusb_get_device_descriptor(devs[i], &dev_desc);
    if(   dev_desc.idVendor!=vid
       || dev_desc.idProduct!=pid
       || (bus>=0 && libusb_get_bus_number(devs[i])!=bus)
       || (port>=0 && libusb_get_port_number(devs[i])!=port) ) continue;

    if(libusb_open(devs[i], &handle))FAIL(1)
    m->handle=handle;

    libusb_get_active_config_descriptor(devs[i], &conf_desc);

    const struct libusb_interface *interface=conf_desc->interface;
    for(j=0; j<conf_desc->bNumInterfaces; ++j, ++interface){
      const struct libusb_interface_descriptor *alt=interface->altsetting;
      for(k=0; k<conf_desc->interface[j].num_altsetting; ++k, ++alt){
        if(alt->bInterfaceClass==3){
          m->interface=alt->bInterfaceNumber;

          libusb_detach_kernel_driver(handle, m->interface);
          libusb_claim_interface(handle, m->interface);
          libusb_set_interface_alt_setting(
            handle, m->interface, alt->bAlternateSetting);

          const struct libusb_endpoint_descriptor *ep=alt->endpoint;
          for(l=0; l<alt->bNumEndpoints; ++l, ++ep){
            if((ep->bmAttributes&3) == LIBUSB_TRANSFER_TYPE_INTERRUPT){
              if((ep->bEndpointAddress&0x80) == LIBUSB_ENDPOINT_IN)
                m->intin=ep->bEndpointAddress;
              else
                m->intout=ep->bEndpointAddress;
              }
            }
/*
          libusb_control_transfer(
            handle,
            LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD
              | LIBUSB_RECIPIENT_INTERFACE,
            LIBUSB_REQUEST_GET_DESCRIPTOR,
            (LIBUSB_DT_REPORT << 8), m->interface, buf, sizeof(buf), 5000);
*/
          if(libusb_reset_device(handle))FAIL(2)
          goto done;
          }
        }
      }
    }
  ret=1;

fail:
  if(m->interface>=0){
    libusb_release_interface(handle, m->interface);
    libusb_attach_kernel_driver(handle, m->interface);
    }
  if(handle)libusb_close(handle);
done:
  libusb_free_device_list(devs, 1);
  libusb_free_config_descriptor(conf_desc);
  return ret;
  }

void mcp2200_close(struct mcp2200 *m){
  libusb_release_interface(m->handle, m->interface);
  libusb_attach_kernel_driver(m->handle, m->interface);
  libusb_close(m->handle);
  }

int mcp2200_gpio_on_off(struct mcp2200 *mcp2200, int onmask, int offmask){
  int sent;

  report[0]=MCP2200_CMD_GPIO_ON_OFF;
  report[11]=onmask;
  report[12]=offmask;

  return libusb_interrupt_transfer(
    mcp2200->handle, mcp2200->intout, report, 16, &sent, 5000
    );
  }

int mcp2200_gpio_on(struct mcp2200 *mcp2200, int onmask){
  return mcp2200_gpio_on_off(mcp2200, onmask, 0);
  }

int mcp2200_gpio_off(struct mcp2200 *mcp2200, int offmask){
  return mcp2200_gpio_on_off(mcp2200, 0, offmask);
  }

int mcp2200_gpio_set(struct mcp2200 *mcp2200, int bits){
  return mcp2200_gpio_on_off(mcp2200, bits, ~bits);
  }

int mcp2200_read_all(struct mcp2200 *mcp2200, struct mcp2200_conf *conf){
  int sent, ret;

  report[0]=MCP2200_CMD_READ_ALL;

  ret=libusb_interrupt_transfer(
    mcp2200->handle, mcp2200->intout, report, 16, &sent, 5000
    );
  if(ret)return ret;

  ret=libusb_interrupt_transfer(
    mcp2200->handle, mcp2200->intin, report, 16, &sent, 5000
    );
  if(ret)return ret;

  conf->pindir=report[4];
  conf->pinalts=report[5];
  conf->pindefaults=report[6];
  conf->pinopts=report[7];
  conf->baudrate=12000000L/(((report[8]<<8)|report[9])+1);
  conf->pinvals=report[10];

  return ret;
  }

int mcp2200_gpio_get(struct mcp2200 *mcp2200, int *bits){
  struct mcp2200_conf conf;
  int ret;

  if((ret=mcp2200_read_all(mcp2200, &conf)))return ret;
  *bits=conf.pinvals;

  return 0;
  }

int mcp2200_configure(struct mcp2200 *mcp2200, struct mcp2200_conf *conf){
  int sent;
  unsigned int baudrate_divisor;

  baudrate_divisor=(12000000L/conf->baudrate)-1;

  report[0]=MCP2200_CMD_CONFIGURE;
  report[4]=conf->pindir;
  report[5]=conf->pinalts;
  report[6]=conf->pindefaults;
  report[7]=conf->pinopts;
  report[8]=baudrate_divisor>>8;
  report[9]=baudrate_divisor&255;

  return libusb_interrupt_transfer(
    mcp2200->handle, mcp2200->intout, report, 16, &sent, 5000
    );
  }

int mcp2200_read_eeprom(struct mcp2200 *mcp2200, int address){
  int sent, ret;

  report[0]=MCP2200_CMD_READ_EEPROM;
  report[1]=address;

  ret=libusb_interrupt_transfer(
    mcp2200->handle, mcp2200->intout, report, 16, &sent, 5000
    );
  if(ret)return ret;

  ret=libusb_interrupt_transfer(
    mcp2200->handle, mcp2200->intin, report, 16, &sent, 5000
    );
  if(ret)return ret;

  return report[3];
  }

int mcp2200_write_eeprom(struct mcp2200 *mcp2200, int address, int data){
  int sent;

  report[0]=MCP2200_CMD_WRITE_EEPROM;
  report[1]=address;
  report[2]=data;

  return libusb_interrupt_transfer(
    mcp2200->handle, mcp2200->intout, report, 16, &sent, 5000
    );
  }
