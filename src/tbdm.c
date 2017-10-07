/*
 * tbdm.c
 *
 * This file is part of tbdm.
 *
 * tbdm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * tbdm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tbdm.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2016        M. Froeschle
 *
 * Parts of this code are based on Kevin Cuzner's blog post "Teensy 3.1 bare metal: Writing a USB driver"
 * from http://kevincuzner.com. Many thanks for the groundwork!
 *
 */
#include <stdlib.h>

#include "usb.h"
#include "arm_cm4.h"

#include "xprintf.h"
#include "xstring.h"

#define DBG_TUSB
#ifdef DBG_TUSB
#define dbg(format, arg...) do { xprintf("DEBUG (%s()): " format, __FUNCTION__, ##arg);} while(0)
#else
#define dbg(format, arg...) do {;} while (0)
#endif /* DBG_TUSB */
#define err(format, arg...) do { xprintf("ERROR (%s()): " format, __FUNCTION__, ##arg); } while(0)

enum
{
    PID_OUT = 0x01,
    PID_DATA0 = 0x03,
    PID_IN = 0x09,
    PID_NAK = 0x0a,
    PID_DATA1 = 0x0b,
    PID_SETUP = 0x0d,
    PID_STALL = 0x0e,
    PID_DERR = 0x0f
} pid_t;

/*
 * request types
 */
enum
{
    R_GET_STATUS = 0x00,
    R_CLEAR_FEATURE = 0x01,
    R_SET_FEATURE = 0x03,
    R_SET_ADDRESS = 0x05,
    R_GET_DESCRIPTOR = 0x06,
    R_SET_DESCRIPTOR = 0x07,
    R_GET_CONFIGURATION = 0x08,
    R_SET_CONFIGURATION = 0x09,
    R_GET_INTERFACE = 0x0a,
    R_SET_INTERFACE = 0x11,
    R_SYNC_FRAME = 0x12
} request_type;

/*
 * interface request types
 */
#define ENDP0_SIZE 64
#define ENDP2_SIZE 64

struct setup
{
    union
    {
        struct
        {
            union
            {
                uint8_t bmRequestType;
                struct
                {
                    uint8_t transfer_direction      : 1;    /* 0=host to device, 1=device to host */
                    uint8_t type                    : 2;    /* see enum r_type below */
                    uint8_t recipient               : 5;    /* see r_recip below */
                };
            };
            uint8_t bRequest;
        };
        uint16_t wRequestAndType;
    };
    union
    {
        struct
        {
            uint8_t lo;
            uint8_t hi;
        };
        uint16_t wValue;
    };
    uint16_t wIndex;
    uint16_t wLength;
};

enum r_type
{
    STANDARD = 0,
    CLASS = 1,
    VENDOR_SPECIFIC = 2
};

enum r_recip
{
    DEVICE = 0,
    INTERFACE = 1,
    ENDPOINT = 2,
    OTHER = 3           // ???
};

struct str_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wString[];
};

struct descriptor_entry
{
    uint16_t wValue;
    uint16_t wIndex;
    const void* addr;
    uint8_t length;
};

#define BDT_BC_SHIFT   16
#define BDT_OWN_MASK   0x80
#define BDT_DATA1_MASK 0x40
#define BDT_KEEP_MASK  0x20
#define BDT_NINC_MASK  0x10
#define BDT_DTS_MASK   0x08
#define BDT_STALL_MASK 0x04

#define BDT_DESC(count, data) ((count << BDT_BC_SHIFT) | BDT_OWN_MASK | (data ? BDT_DATA1_MASK : 0x00) | BDT_DTS_MASK)
#define BDT_PID(desc) ((desc >> 2) & 0xF)

/*
 * Buffer Descriptor Table entry
 * There are two entries per direction per endpoint:
 *   In  Even/Odd
 *   Out Even/Odd
 * A bidirectional endpoint would then need 4 entries
 */
struct bdt
{
    uint32_t desc;
    void *addr;
};

/*
 * we enforce a max of 15 endpoints (15 + 1 control = 16)
 */
#define USB_N_ENDPOINTS     15

/*
 * determines an appropriate BDT index for the given conditions (see fig. 41-3)
 */
#define RX 0
#define TX 1
#define EVEN 0
#define ODD  1
#define BDT_INDEX(endpoint, tx, odd) ((endpoint << 2) | (tx << 1) | odd)

/*
 * Buffer descriptor table, aligned to a 512-byte boundary (see linker file)
 */
__attribute__ ((aligned(512), used)) static struct bdt table[(USB_N_ENDPOINTS + 1) * 4]; /* max endpoints is 15 + 1 control */

/*
 * Endpoint 0 receive buffers (2 x 64 bytes)
 */
static uint8_t endp0_rx[2][ENDP0_SIZE];

static const uint8_t *endp0_tx_dataptr = NULL;  // pointer to current transmit chunk
static uint16_t endp0_tx_datalen = 0;           // length of data remaining to send

/*
 * endpoint 2 receive buffers (2 x 64 bytes)
 */
static uint8_t endp2_rx[2][ENDP2_SIZE];
static const uint8_t *endp2_rx_dataptr = NULL;
static uint16_t endp2_rx_datalen = 0;

/*
 * Device descriptor
 * NOTE: This cannot be const because without additional attributes, it will
 * not be placed in a part of memory that the usb subsystem can access. I
 * have a suspicion that this location is somewhere in flash, but not copied
 * to RAM.
 */
static uint8_t dev_descriptor[] =
{
    18,         // bLength
    1,          // bDescriptorType
    0x00, 0x02, // bcdUSB
    0xff,       // bDeviceClass
    0xff,       // bDeviceSubClass
    0xff,       // bDeviceProtocol
    ENDP0_SIZE, // bMaxPacketSize0
    0x25, 0x04, // idVendor
    0x01, 0x10, // idProduct
    0x01, 0x00, // bcdDevice
    1,          // iManufacturer
    2,          // iProduct
    2,          // iSerialNumber,
    1,          // bNumConfigurations
};

/*
 * Configuration descriptor
 * NOTE: Same thing about const applies here
 */
static uint8_t cfg_descriptor[] =
{
    9,                  // bLength
    2,                  // bDescriptorType
    9 + 9 + 7 + 7, 0x00, // wTotalLength
    1,                  // bNumInterfaces
    1,                  // bConfigurationValue,
    0,                  // iConfiguration
    0x80,               // bmAttributes
    250,                // bMaxPower
    /* INTERFACE 0 BEGIN */
    9,                  // bLength
    4,                  // bDescriptorType
    0,                  // bInterfaceNumber
    0,                  // bAlternateSetting
    2,                  // bNumEndpoints
    0xff,               // bInterfaceClass
    0xff,               // bInterfaceSubClass,
    0xff,               // bInterfaceProtocol
    2,                  // iInterface
    /* INTERFACE 0, ENDPOINT 1 BEGIN */
    7,                  // bLength
    5,                  // bDescriptorType,
    0x81,               // bEndpointAddress,
    0x02,               // bmAttributes, bulk endpoint
    ENDP0_SIZE, 0x00,   // wMaxPacketSize,
    1,                  // bInterval
    /* INTERFACE 0, ENDPOINT 1 END */
    /* INTERFACE 0, ENDPOINT 2 BEGIN */
    7,                  // bLength
    5,                  // bDescriptorType
    0x02,               // bInterfaceNumber
    0x02,               // bmAttributes
    ENDP0_SIZE, 0x00,   // wxMaxPacketSize
    1,                  // bInterval
    /* INTERFACE 0 END */
};

static uint8_t device_qualifier[] =
{
    9,                  // bLength
    0x6,                // bDescriptorType
    0x00, 0x20,         // bcdUSB - USB version number
    0xff,               // bDeviceClass
    0xff,               // bDeviceSubClass
    0xff,               // bDeviceProtocol
    64,                 // bMaxPacketSize0
    1,                  // bNumConfigurations
    0,                  // bReserved
};

static struct str_descriptor lang_descriptor =
{
    .bLength = 4,
    .bDescriptorType = 3,
    .wString = { 0x0409 }   //english (US)
};

static struct str_descriptor manuf_descriptor =
{
    .bLength = 2 + 7 * 2,
    .bDescriptorType = 3,
    .wString = {'m','u','b','f', '.', 'd', 'e'}
};

static struct str_descriptor product_descriptor =
{
    .bLength = 2 + 15 * 2,
    .bDescriptorType = 3,
    .wString = {'T', 'e', 'e', 'n', 's', 'y', ' ', 'B', 'D', 'M', ' ', 'P', 'O', 'D', ' ' }
};

static const struct descriptor_entry descriptors[] =
{
    { 0x0100, 0x0000, dev_descriptor, sizeof(dev_descriptor) },
    { 0x0200, 0x0000, &cfg_descriptor, sizeof(cfg_descriptor) },
    { 0x0300, 0x0000, &lang_descriptor, 4 },
    { 0x0301, 0x0409, &manuf_descriptor, 2 + 15 * 2 },
    { 0x0302, 0x0409, &product_descriptor, 2 + 15 * 2 },
    { 0x0600, 0x0000, &device_qualifier, sizeof(device_qualifier) },
    { 0x0000, 0x0000, NULL, 0 }
};

static uint8_t endp0_odd = 0;
static uint8_t endp0_data = 0;

static uint8_t endp2_odd = 0;
static uint8_t endp2_data = 0;


static void usb_endp0_transmit(const void *data, uint8_t length)
{
    table[BDT_INDEX(0, TX, endp0_odd)].addr = (void *) data;
    table[BDT_INDEX(0, TX, endp0_odd)].desc = BDT_DESC(length, endp0_data);

    /*
     * toggle the odd and data bits
     */
    endp0_odd ^= 1;
    endp0_data ^= 1;
}

static unsigned char cmd_buffer[ENDP0_SIZE];

/*
 * Endpoint 0 setup handler
 */
static void usb_endp0_handle_setup(struct setup *packet)
{
    const struct descriptor_entry *entry;
    const uint8_t *data = NULL;
    uint8_t data_length = 0;
    uint32_t size = 0;

    dbg("\r\n"
        "bmRequestType: direction: %s\r\n"
        "         type: %s\r\n"
        "    recipient: %s\r\n",
        packet->wRequestAndType & (1 << 7) ? "device to host" : "host to device",
        (packet->wRequestAndType >> 5 & 3) == 0 ? "standard" :
        (packet->wRequestAndType >> 5 & 3) == 1 ? "class" :
        (packet->wRequestAndType >> 5 & 3) == 2 ? "vendor" : "reserved",
        (packet->wRequestAndType & 7) == 0 ? "device" :
        (packet->wRequestAndType & 7) == 1 ? "interface" :
        (packet->wRequestAndType & 7) == 2 ? "endpoint" :
        (packet->wRequestAndType & 7) == 3 ? "other" : "reserved"
        );

    switch (packet->bRequest)
    {
        case R_SET_ADDRESS:    // set address (wait for IN packet)
            dbg("set address. Waiting for IN packet\r\n");
            break;

        case R_SET_CONFIGURATION:    // set configuration
            dbg("set configuration %d\r\n", packet->wValue);
            dbg("enable endpoint 2\r\n");
            if (packet->lo)
            {
                /* non-zero configuration number */
                /* initialize EP2 */

            }
            else
            {
                /* configuration zero means back to addressed state */
            }

            /* next send back the confirmation */
            break;

        case R_GET_DESCRIPTOR:    // get descriptor
            dbg("get descriptor\r\n");
            for (entry = descriptors; 1; entry++)
            {
                if (entry->addr == NULL)
                    break;
                dbg("packet->wValue=%4x, entry->wValue=%4x\r\n", packet->wValue, entry->wValue);
                dbg("packet->wIndex=%4x, entry->wIndex=%4x\r\n", packet->wIndex, entry->wIndex);
                if (packet->wValue == entry->wValue && packet->wIndex == entry->wIndex)
                {
                    // this is the descriptor to send
                    data = entry->addr;
                    data_length = entry->length;
                    goto send;
                }
            }
            goto end;
            break;

        case R_GET_STATUS:
            dbg("get status\r\n");
            break;

        default:
            dbg("stalled\r\n"
                "(packet->bmRequest=%02x,\r\n"
                " packet->bmRequestType=%02x)\r\n\r\n", packet->bRequest, packet->bmRequestType);
            goto stall;
    }

    /*
     * if we are sent here, we need to send some data
     */

send:

    LED_OFF();
    /*
     * truncate the data length to whatever the setup packet is expecting
     */

    if (data_length > packet->wLength)
        data_length = packet->wLength;

    /*
     * transmit 1st chunk
     */
    size = data_length;
    if (size > ENDP0_SIZE)
        size = ENDP0_SIZE;

    usb_endp0_transmit(data, size);
    data += size;           // move the pointer down
    data_length -= size;    // move the size down
    if (data_length == 0 && size < ENDP0_SIZE)
    {
        LED_ON();
        return;             // all done!
    }

    /*
     * transmit 2nd chunk
     */
    size = data_length;
    if (size > ENDP0_SIZE)
        size = ENDP0_SIZE;

    usb_endp0_transmit(data, size);
    data += size;           // move the pointer down
    data_length -= size;    // move the size down
    if (data_length == 0 && size < ENDP0_SIZE)
        return;             // all done!

    /*
     * if any data remains to be transmitted, we need to store it
     */
    endp0_tx_dataptr = data;
    endp0_tx_datalen = data_length;

    LED_ON();

    return;

    /*
     * if we make it here, we are not able to send data and have stalled
     */
stall:
    dbg("stalled.\r\n");
end:
    USB0_ENDPT0 = USB_ENDPT_EPSTALL_MASK | USB_ENDPT_EPRXEN_MASK | USB_ENDPT_EPTXEN_MASK | USB_ENDPT_EPHSHK_MASK;
}

void usb_endp0_handle_vendor(struct setup *setup)
{
    const struct descriptor_entry *entry;
    const uint8_t *data = NULL;
    uint8_t data_length = 0;
    uint32_t size = 0;

    dbg("\r\n");
}

/**
 * Endpoint 0 handler
 */
void usb_endp0_handler(uint8_t stat)
{
    static struct setup last_setup;

    const uint8_t *data = NULL;
    uint32_t size = 0;

    /*
     * determine which bdt we are looking at here
     */
    struct bdt *bdt = &table[BDT_INDEX(0, (stat & USB_STAT_TX_MASK) >> USB_STAT_TX_SHIFT, (stat & USB_STAT_ODD_MASK) >> USB_STAT_ODD_SHIFT)];

    switch (BDT_PID(bdt->desc))
    {
        case PID_SETUP:
            dbg("PID_SETUP\r\n");

            /*
             * extract the setup token
             */
            last_setup = *((struct setup *) (bdt->addr));

            /*
             * we are now done with the buffer
             */
            bdt->desc = BDT_DESC(ENDP0_SIZE, 1);

            /*
             * clear any pending IN stuff
             */
            table[BDT_INDEX(0, TX, EVEN)].desc = 0;
            table[BDT_INDEX(0, TX, ODD)].desc = 0;
            endp0_data = 1;

            /*
             * cast the data into our setup type and run the setup
             * check for vendor specific command (needed for TBLCF)
             */
            if (last_setup.type == VENDOR_SPECIFIC)
                usb_endp0_handle_vendor(&last_setup);
            else
                usb_endp0_handle_setup(&last_setup);

            /*
             * unfreeze this endpoint
             */
            USB0_CTL = USB_CTL_USBENSOFEN_MASK;
            break;

        case PID_IN:
            dbg("PID_IN (host requesting more data)\r\n");

            /*
             * continue sending any pending transmit data
             */
            data = endp0_tx_dataptr;
            if (data)
            {
                size = endp0_tx_datalen;
                if (size > ENDP0_SIZE)
                    size = ENDP0_SIZE;
                usb_endp0_transmit(data, size);
                data += size;
                endp0_tx_datalen -= size;
                endp0_tx_dataptr = (endp0_tx_datalen > 0 || size == ENDP0_SIZE) ? data : NULL;
            }

            if (last_setup.bRequest == R_SET_ADDRESS)
            {
                dbg("set address to %d\r\n", last_setup.wValue);
                USB0_ADDR = last_setup.wValue;
            }
            break;

        case PID_OUT:
            dbg("PID_OUT\r\n");
            /*
             * nothing to do here..just give the buffer back
             */
            bdt->desc = BDT_DESC(ENDP0_SIZE, 1);
            break;

        default:
            dbg("type %0x02d packet received. Not handled\r\n", BDT_PID(bdt->desc));
            break;
    }

    USB0_CTL = USB_CTL_USBENSOFEN_MASK;
}

/*
 * Endpoint 2 handler
 */
void usb_endp2_handler(uint8_t stat)
{
    /*
     * determine which bdt we are looking at here
     */
    struct bdt *bdt = &table[BDT_INDEX(2, (stat & USB_STAT_TX_MASK) >> USB_STAT_TX_SHIFT, (stat & USB_STAT_ODD_MASK) >> USB_STAT_ODD_SHIFT)];

    dbg("\r\n");
    switch (BDT_PID(bdt->desc))
    {
        case PID_OUT:
            dbg("PID_OUT\r\n");
            /*
             * nothing to do here..just give the buffer back
             */
            bdt->desc = BDT_DESC(ENDP2_SIZE, bdt);
            break;

        default:
            dbg("type %0x02d packet received. Not handled\r\n", BDT_PID(bdt->desc));
            break;
    }

    USB0_CTL = USB_CTL_USBENSOFEN_MASK;
}

static void (*handlers[16])(uint8_t) =
{
    usb_endp0_handler,
    usb_endp1_handler,
    usb_endp2_handler,
    usb_endp3_handler,
    usb_endp4_handler,
    usb_endp5_handler,
    usb_endp6_handler,
    usb_endp7_handler,
    usb_endp8_handler,
    usb_endp9_handler,
    usb_endp10_handler,
    usb_endp11_handler,
    usb_endp12_handler,
    usb_endp13_handler,
    usb_endp14_handler,
    usb_endp15_handler,
};

/**
 * Default handler for USB endpoints that does nothing
 */
static void usb_endp_default_handler(uint8_t stat)
{
    dbg("unexpected (stat=%d).\r\n", stat);
}

// weak aliases as "defaults" for the usb endpoint handlers

void usb_endp1_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp3_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp4_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp5_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp6_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp7_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp8_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp9_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp10_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp11_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp12_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp13_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp14_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp15_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));

void usb_init(void)
{
    uint32_t i;

    /*
     * reset the buffer descriptors
     */
    for (i = 0; i < (USB_N_ENDPOINTS + 1) * 4; i++)
    {
        table[i].desc = 0;
        table[i].addr = 0;
    }

    /*
     * 1: Select clock source
     */
    SIM_SOPT2 |= SIM_SOPT2_USBSRC_MASK | SIM_SOPT2_PLLFLLSEL_MASK; //we use MCGPLLCLK divided by USB fractional divider
    SIM_CLKDIV2 = SIM_CLKDIV2_USBDIV(1);// SIM_CLKDIV2_USBDIV(2) | SIM_CLKDIV2_USBFRAC_MASK; //(USBFRAC + 1)/(USBDIV + 1) = (1 + 1)/(2 + 1) = 2/3 for 72Mhz clock

    /*
     * 2: Gate USB clock
     */
    SIM_SCGC4 |= SIM_SCGC4_USBOTG_MASK;

    /*
     * 3: Software USB module reset
     */
    USB0_USBTRC0 |= USB_USBTRC0_USBRESET_MASK;
    while (USB0_USBTRC0 & USB_USBTRC0_USBRESET_MASK);

    /*
     * 4: Set BDT base registers
     */
    USB0_BDTPAGE1 = ((uint32_t)table) >> 8;  // bits 15-9
    USB0_BDTPAGE2 = ((uint32_t)table) >> 16; // bits 23-16
    USB0_BDTPAGE3 = ((uint32_t)table) >> 24; // bits 31-24

    /*
     * 5: Clear all ISR flags and enable weak pull downs
     */
    USB0_ISTAT = 0xFF;
    USB0_ERRSTAT = 0xFF;
    USB0_OTGISTAT = 0xFF;
    USB0_USBTRC0 |= 0x40; //a hint was given that this is an undocumented interrupt bit

    /*
     * 6: Enable USB reset interrupt
     */
    USB0_CTL = USB_CTL_USBENSOFEN_MASK;
    USB0_USBCTRL = 0;

    USB0_INTEN |= USB_INTEN_USBRSTEN_MASK;
    //NVIC_SET_PRIORITY(IRQ(INT_USB0), 112);
    enable_irq(IRQ(INT_USB0));

    /*
     * 7: Enable pull-up resistor on D+ (Full speed, 12Mbit/s)
     */
    USB0_CONTROL = USB_CONTROL_DPPULLUPNONOTG_MASK;
}

void USBOTG_IRQHandler(void)
{
    uint8_t status;
    uint8_t stat, endpoint;

    status = USB0_ISTAT;

    if (status & USB_ISTAT_USBRST_MASK)
    {
        /*
         * handle USB reset
         */
        dbg("USB reset requested\r\n");

        /*
         * initialize endpoint 0 ping-pong buffers
         */
        USB0_CTL |= USB_CTL_ODDRST_MASK;
        endp0_odd = 0;
        table[BDT_INDEX(0, RX, EVEN)].desc = BDT_DESC(ENDP0_SIZE, 0);
        table[BDT_INDEX(0, RX, EVEN)].addr = endp0_rx[0];
        table[BDT_INDEX(0, RX, ODD)].desc = BDT_DESC(ENDP0_SIZE, 0);
        table[BDT_INDEX(0, RX, ODD)].addr = endp0_rx[1];
        table[BDT_INDEX(0, TX, EVEN)].desc = 0;
        table[BDT_INDEX(0, TX, ODD)].desc = 0;


        endp2_odd = 0;
        table[BDT_INDEX(2, RX, EVEN)].desc = BDT_DESC(ENDP2_SIZE, 0);
        table[BDT_INDEX(2, RX, EVEN)].addr = endp2_rx[0];
        table[BDT_INDEX(2, RX, ODD)].desc = BDT_DESC(ENDP2_SIZE, 0);
        table[BDT_INDEX(2, RX, ODD)].addr = endp2_rx[1];

        USB0_ENDPT1 = USB_ENDPT_EPTXEN_MASK | USB_ENDPT_EPHSHK_MASK;
        USB0_ENDPT2 = USB_ENDPT_EPRXEN_MASK | USB_ENDPT_EPHSHK_MASK;

        /*
         * initialize endpoint0 to 0x0d (41.5.23)
         */

        /*
         * transmit, recieve, and handshake
         */
        USB0_ENDPT0 = USB_ENDPT_EPRXEN_MASK | USB_ENDPT_EPTXEN_MASK | USB_ENDPT_EPHSHK_MASK;

        /*
         * clear all interrupts...this is a reset
         */
        USB0_ERRSTAT = 0xff;
        USB0_ISTAT = 0xff;

        /*
         * after reset, we are address 0, per USB spec
         */
        USB0_ADDR = 0;

        /*
         * all necessary interrupts are now active
         */
        USB0_ERREN = 0xFF;
        USB0_INTEN = USB_INTEN_USBRSTEN_MASK | USB_INTEN_ERROREN_MASK |
                USB_INTEN_SOFTOKEN_MASK | USB_INTEN_TOKDNEEN_MASK |
                USB_INTEN_SLEEPEN_MASK | USB_INTEN_STALLEN_MASK;

        return;
    }

    if (status & USB_ISTAT_ERROR_MASK)
    {
        /*
         * handle error
         */
        dbg("error\r\n");

        USB0_ERRSTAT = USB0_ERRSTAT;
        USB0_ISTAT = USB_ISTAT_ERROR_MASK;
    }

    if (status & USB_ISTAT_SOFTOK_MASK)
    {
        /*
         * handle start of frame token
         */
        // dbg("start of frame\r\n");

        USB0_ISTAT = USB_ISTAT_SOFTOK_MASK;
    }

    if (status & USB_ISTAT_TOKDNE_MASK)
    {
        /*
         * handle completion of current token being processed
         */
        // dbg("token done interrupt\r\n");

        stat = USB0_STAT;
        endpoint = stat >> 4;
        handlers[endpoint & 0xf](stat);

        USB0_ISTAT = USB_ISTAT_TOKDNE_MASK;
    }

    if (status & USB_ISTAT_SLEEP_MASK)
    {
        /*
         * handle USB sleep
         */
        dbg("sleep\r\n");

        USB0_ISTAT = USB_ISTAT_SLEEP_MASK;
    }

    if (status & USB_ISTAT_STALL_MASK)
    {
        dbg("stall\r\n");
        /*
         * handle usb stall
         */
        USB0_ISTAT = USB_ISTAT_STALL_MASK;
    }
}
