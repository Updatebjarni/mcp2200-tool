#include<ctype.h>
#include<string.h>
#include<stdlib.h>
#include<stdio.h>
#include<libusb.h>
#include<getopt.h>

#define MCP2200_VID 0x04d8
#define MCP2200_PID 0x00df

#define MCP2200_PINALT_TXLED     4
#define MCP2200_PINALT_RXLED     8
#define MCP2200_PINALT_CONFIG   64
#define MCP2200_PINALT_SUSPEND 128

struct{int pin; char *name;}altnames[]={
  {MCP2200_PINALT_TXLED, "TXLED"},
  {MCP2200_PINALT_RXLED, "RXLED"},
  {MCP2200_PINALT_CONFIG, "CONFIG"},
  {MCP2200_PINALT_SUSPEND, "SUSPEND"}
  };

#define MCP2200_PINOPT_RXTGL   128
#define MCP2200_PINOPT_TXTGL    64
#define MCP2200_PINOPT_LEDX     32
#define MCP2200_PINOPT_INVERT    2
#define MCP2200_PINOPT_HWFLOW    1

#define MCP2200_CMD_GPIO_ON_OFF    8
#define MCP2200_CMD_CONFIGURE     16
#define MCP2200_CMD_READ_EEPROM   32
#define MCP2200_CMD_WRITE_EEPROM  64
#define MCP2200_CMD_READ_ALL     128


struct mcp2200{
  libusb_device_handle *handle;
  int intin, intout, interface;
  };

struct mcp2200_conf{
  int pindir, pinalts, pindefaults, pinopts, pinvals;
  long baudrate;
  };

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

  m->interface=-1;

  if(vid<0)vid=MCP2200_VID;
  if(pid<0)pid=MCP2200_PID;

  libusb_init(0);

  ndevs=libusb_get_device_list(0, &devs);
  for(ssize_t i=0; i<ndevs; ++i){
    libusb_get_device_descriptor(devs[i], &dev_desc);
    if(   dev_desc.idVendor!=vid
       || dev_desc.idProduct!=pid
       || (bus>=0 && libusb_get_bus_number(devs[i])!=bus)
       || (port>=0 && libusb_get_port_number(devs[i])!=port) ) continue;

    if(libusb_open(devs[i], &handle))FAIL(1)
    m->handle=handle;

    libusb_get_active_config_descriptor(devs[i], &conf_desc);

    const struct libusb_interface *interface=conf_desc->interface;
    for(int i=0; i<conf_desc->bNumInterfaces; ++i, ++interface){
      const struct libusb_interface_descriptor *alt=interface->altsetting;
      for(int j=0; j<conf_desc->interface[i].num_altsetting; ++j, ++alt){
        if(alt->bInterfaceClass==3){
          m->interface=alt->bInterfaceNumber;

          libusb_detach_kernel_driver(handle, m->interface);
          libusb_claim_interface(handle, m->interface);
          libusb_set_interface_alt_setting(
            handle, m->interface, alt->bAlternateSetting);

          const struct libusb_endpoint_descriptor *ep=alt->endpoint;
          for(int k=0; k<alt->bNumEndpoints; ++k, ++ep){
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

int parse_pinlist(char *s){
  int ret=0;
  while(*s){
    if(*s<'0' || *s>'7')return -1;
    ret|=1<<(*s-'0');
    ++s;
    if(*s==',')++s;
    }
  return ret;
  }

long parse_number(char *s, long max){
  long val=0;
  char *endptr;
  val=strtol(s, &endptr, 0);
  if(endptr!=s && !*endptr && val<=max)return (int)val;
  else return -1;
  }

int parse_pins(char *s){
  if(*s=='=')return parse_number(s+1, 255);
  else return parse_pinlist(s);
  }

int find_string(char **l, char *s){
  while(*l){
    if(!strcmp(*l, s))return 1;
    ++l;
    }
  return 0;
  }

int parse_flag(char *s){
  static char *yesses[]={"yes", "y", "on", "true", "1", "t", 0},
              *noes[]={"no", "n", "off", "false", "0", "nil", 0};
  if(find_string(yesses, s))return 1;
  if(find_string(noes, s))return 0;
  return -1;
  }

#define ERRCMDLINE(fmt, ...) {fprintf(stderr, fmt, __VA_ARGS__); err=1; break;}

#define FLAGOPT(confbyte, bitname) \
        val=parse_flag(optarg); \
        if(val<0)ERRCMDLINE("Invalid flag: %s\n", optarg) \
        if(val)c.confbyte|=MCP2200_ ## bitname ; \
        else c.confbyte&=~MCP2200_ ## bitname ; \
        do_configure=1; \
        break;

#define LEDOPT(led) \
        val=parse_flag(optarg); \
        if(val<0){ \
          if(!strcmp(optarg, "toggle")){ \
            c.pinalts|=MCP2200_PINALT_ ## led ## LED; \
            c.pinopts|=MCP2200_PINOPT_ ## led ## TGL; \
            } \
          else ERRCMDLINE("Invalid activity LED mode: %s\n", optarg) \
          } \
        else{ \
          if(val)c.pinalts|=MCP2200_PINALT_ ## led ## LED; \
          else c.pinalts&=~MCP2200_PINALT_ ## led ## LED; \
          c.pinopts&=~MCP2200_PINOPT_ ## led ## TGL; \
          } \
        do_configure=1; \
        break;

int main(int argc, char *argv[]){
  struct mcp2200 m;
  m.handle=0;
  static struct mcp2200_conf c, tempc;

  static struct option long_options[] = {
    {"pins-on", required_argument, 0, 's'},
    {"pins-off", required_argument, 0, 'c'},
    {"write-pins", required_argument, 0, 'o'},
    {"read-pins", no_argument, 0, 'i'},
    {"pin-defaults", required_argument, 0, 'p'},
    {"pin-directions", required_argument, 0, 'P'},
    {"read-eeprom", required_argument, 0, 'r'},
    {"write-eeprom", required_argument, 0, 'w'},
    {"dump-eeprom", no_argument, 0, 'D'},
    {"suspend-led", required_argument, 0, 'S'},
    {"config-led", required_argument, 0, 'C'},
    {"tx-led", required_argument, 0, 'T'},
    {"rx-led", required_argument, 0, 'R'},
    {"blink-rate", required_argument, 0, 'B'},
    {"invert-serial", required_argument, 0, 'I'},
    {"flow-control", required_argument, 0, 'F'},
    {"baudrate", required_argument, 0, 'b'},
    {"dump-config", no_argument, 0, 'd'},
    {"help", no_argument, 0, 'h'},
    {"id", required_argument, 0, 'u'},
    {"port", required_argument, 0, 'U'},
    {0, 0, 0, 0}
    };
  static struct option id_and_port_options[] = {
    {"id", required_argument, 0, 'u'},
    {"port", required_argument, 0, 'U'},
    {0, 0, 0, 0}
    };
  int opt;
  char *optstr="+s:c:o:ip:P:r:w:DS:C:T:R:B:I:F:b:dhu:U:";
  int vid=-1, pid=-1, bus=-1, port=-1;
  char dummy;
  long val, err=0;
  int eewaddr, eedata;
  int do_configure=0;

  opterr=0;
  while((opt=getopt_long(argc, argv, "+u:U:", id_and_port_options, 0))!=-1){
    if(opt=='?')break;
    switch(opt){
      case 'u':
        if(sscanf(optarg, "%x:%x%c", &vid, &pid, &dummy)!=2){
          fprintf(stderr, "Specify device ID as \"VID:PID\"\n");
          return 1;
          }
        break;
      case 'U':
        if(sscanf(optarg, "%i:%i%c", &bus, &port, &dummy)!=2){
          fprintf(stderr, "Specify device address as \"bus:port\"\n");
          return 1;
          }
        break;
      }
    }
  opterr=1;
  optind=0;

  if(mcp2200_open(&m, vid, pid, bus, port)){
    fprintf(stderr, "Could not open device.\n");
    return 1;
    }
  mcp2200_read_all(&m, &c);

  while((opt=getopt_long(argc, argv, optstr, long_options, 0))!=-1){
    if(opt=='?')break;
    switch(opt){
      case 'u':
      case 'U':  // Ignore these options now
        break;
      case 's':  // pins-on
        val=parse_pins(optarg);
        if(val<0)ERRCMDLINE("Invalid pin specifier: %s\n", optarg)
        mcp2200_gpio_on(&m, val);
        break;
      case 'c':  // pins-off
        val=parse_pins(optarg);
        if(val<0)ERRCMDLINE("Invalid pin specifier: %s\n", optarg)
        mcp2200_gpio_off(&m, val);
        break;
      case 'o':  // write-pins
        val=parse_number(optarg, 255);
        if(val<0)ERRCMDLINE("Invalid byte: %s\n", optarg)
        mcp2200_gpio_set(&m, val);
        break;
      case 'i':  // read-pins
        mcp2200_read_all(&m, &tempc);
        printf("GPIO pins   = 0x%.2X\n", tempc.pinvals);
        break;
      case 'p':  // pin-defaults
        val=parse_pins(optarg);
        if(val<0)ERRCMDLINE("Invalid pin specifier: %s\n", optarg)
        c.pindefaults=val;
        do_configure=1;
        break;
      case 'P':  // pin-directions
        if((optarg[0]=='i' || optarg[0]=='o') && (optarg[1]=='=')){
          val=parse_pinlist(optarg+2);
          }
        else
          val=parse_number(optarg, 255);
        if(val>=0){
          if(optarg[0]=='o')val^=255;
          c.pindir=val;
          do_configure=1;
          break;
          }
        ERRCMDLINE("Invalid pin direction specifier: %s\n", optarg)
      case 'r':  // read-eeprom
        val=parse_number(optarg, 255);
        if(val<0)ERRCMDLINE("Invalid EEPROM address: %s\n", optarg)
        printf("EEPROM address %.2X = %.2X\n",
               (unsigned)val, mcp2200_read_eeprom(&m, val));
        break;
      case 'w':  // write-eeprom
        if((sscanf(optarg, "%i=%i%c", &eewaddr, &eedata, &dummy)!=2)
           || eewaddr<0 || eewaddr>255 || eedata<0 || eedata>255)
          ERRCMDLINE("Invalid EEPROM write specifier: %s\n", optarg)
        mcp2200_write_eeprom(&m, eewaddr, eedata);
        break;
      case 'D':  // dump-eeprom
        printf("EEPROM dump:\n");
        for(int i=0; i<16; ++i){
          unsigned char data[16];
          printf("%.2X: ", i*16);
          for(int j=0; j<16; ++j)
            data[j]=mcp2200_read_eeprom(&m, i*16+j);
          for(int j=0; j<16; ++j)
            printf(" %.2X", data[j]);
          printf("  |");
          for(int j=0; j<16; ++j)
            putchar(isprint(data[j])?data[j]:'.');
          printf("|\n");        
          }
        break;
      case 'S':  // suspend-led pinalt
        FLAGOPT(pinalts, PINALT_SUSPEND)
      case 'C':  // config-led pinalt
        FLAGOPT(pinalts, PINALT_CONFIG)
      case 'T':  // tx-led pinalt+pinopt
        LEDOPT(TX)
      case 'R':  // rx-led pinalt+pinopt
        LEDOPT(RX)
      case 'B':  // blink-rate pinopt
        if(!strcmp(optarg, "fast"))c.pinopts&=~MCP2200_PINOPT_LEDX;
        else if(!strcmp(optarg, "slow"))c.pinopts|=MCP2200_PINOPT_LEDX;
        else ERRCMDLINE("Invalid blink rate: %s\n", optarg)
        do_configure=1;
        break;
      case 'I':  // invert-serial pinopt
        FLAGOPT(pinopts, PINOPT_INVERT)
      case 'F':  // flow-control pinopt
        FLAGOPT(pinopts, PINOPT_HWFLOW)
      case 'b':  // baudrate
        val=parse_number(optarg, 12000000L);
        if(val<0)ERRCMDLINE("Invalid baudrate: %s\n", optarg)
        c.baudrate=val;
        do_configure=1;
        break;
      case 'd':  // dump-config
        printf("pindir      = 0x%.2X\n", c.pindir);
        printf("pinalts     = 0x%.2X  [", c.pinalts);
        for(int i=0; i<sizeof(altnames)/sizeof(altnames[0]); ++i)
          if(c.pinalts & altnames[i].pin)
            printf(" %d(%s)", i, altnames[i].name);
        printf(" ]\n");
        putchar('\n');
        printf("pindefaults = 0x%.2X\n", c.pindefaults);
        printf("pinopts     = 0x%.2X\n", c.pinopts);
        printf("baudrate    = %ld\n", c.baudrate);
        break;
      case 'h':
      default:
        printf("usage usage ***\n");
        break;
      }
    }

  if(err)return err;

  if(argv[optind]){
    fprintf(stderr, "Garbage on command line: %s\n", argv[optind]);
    return 1;
    }

  if(do_configure)
    mcp2200_configure(&m, &c);

  mcp2200_close(&m);

  libusb_exit(0);
  return 0;
  }
