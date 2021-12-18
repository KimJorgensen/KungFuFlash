/*
 * Copyright (c) 2019-2021 Kim Jørgensen
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************
 * Based on https://github.com/dmitrystu/libusb_stm32
 * Apache License 2.0
 */

#include "stm32.h"
#include "usb.h"
#include "usb_cdc.h"
#include "usb_hid.h"
#include "hid_usage_desktop.h"
#include "hid_usage_button.h"

#define USBD_SOF_DISABLED
#include "usbd_core.c"
#include "usbd_stm32f429_otgfs.c"

#define CDC_EP0_SIZE    0x08
#define CDC_RXD_EP      0x01
#define CDC_TXD_EP      0x81
#define CDC_DATA_SZ     0x40
#define CDC_NTF_EP      0x82
#define CDC_NTF_SZ      0x08
#define HID_RIN_EP      0x83
#define HID_RIN_SZ      0x10


/* Declaration of the report descriptor */
struct cdc_config {
    struct usb_config_descriptor        config;
    struct usb_iad_descriptor           comm_iad;
    struct usb_interface_descriptor     comm;
    struct usb_cdc_header_desc          cdc_hdr;
    struct usb_cdc_call_mgmt_desc       cdc_mgmt;
    struct usb_cdc_acm_desc             cdc_acm;
    struct usb_cdc_union_desc           cdc_union;
    struct usb_endpoint_descriptor      comm_ep;
    struct usb_interface_descriptor     data;
    struct usb_endpoint_descriptor      data_eprx;
    struct usb_endpoint_descriptor      data_eptx;
} __attribute__((packed));

/* HID mouse report desscriptor. 2 axis 5 buttons */
static const u8 hid_report_desc[] = {
    HID_USAGE_PAGE(HID_PAGE_DESKTOP),
    HID_USAGE(HID_DESKTOP_MOUSE),
    HID_COLLECTION(HID_APPLICATION_COLLECTION),
        HID_USAGE(HID_DESKTOP_POINTER),
        HID_COLLECTION(HID_PHYSICAL_COLLECTION),
            HID_USAGE(HID_DESKTOP_X),
            HID_USAGE(HID_DESKTOP_Y),
            HID_LOGICAL_MINIMUM(-127),
            HID_LOGICAL_MAXIMUM(127),
            HID_REPORT_SIZE(8),
            HID_REPORT_COUNT(2),
            HID_INPUT(HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_RELATIVE ),
            HID_USAGE_PAGE(HID_PAGE_BUTTON),
            HID_USAGE_MINIMUM(1),
            HID_USAGE_MAXIMUM(5),
            HID_LOGICAL_MINIMUM(0),
            HID_LOGICAL_MAXIMUM(1),
            HID_REPORT_SIZE(1),
            HID_REPORT_COUNT(5),
            HID_INPUT(HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE ),
            HID_REPORT_SIZE(1),
            HID_REPORT_COUNT(3),
            HID_INPUT(HID_IOF_CONSTANT),
        HID_END_COLLECTION,
    HID_END_COLLECTION,
};

/* Device descriptor */
static const struct usb_device_descriptor device_desc = {
    .bLength            = sizeof(struct usb_device_descriptor),
    .bDescriptorType    = USB_DTYPE_DEVICE,
    .bcdUSB             = VERSION_BCD(2,0,0),
    .bDeviceClass       = USB_CLASS_IAD,
    .bDeviceSubClass    = USB_SUBCLASS_IAD,
    .bDeviceProtocol    = USB_PROTO_IAD,
    .bMaxPacketSize0    = CDC_EP0_SIZE,
    .idVendor           = 0x0483,
    .idProduct          = 0x5740,
    .bcdDevice          = VERSION_BCD(1,0,0),
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = INTSERIALNO_DESCRIPTOR,
    .bNumConfigurations = 1,
};

/* Device configuration descriptor */
static const struct cdc_config config_desc = {
    .config = {
        .bLength                = sizeof(struct usb_config_descriptor),
        .bDescriptorType        = USB_DTYPE_CONFIGURATION,
        .wTotalLength           = sizeof(struct cdc_config),
        .bNumInterfaces         = 2,
        .bConfigurationValue    = 1,
        .iConfiguration         = NO_DESCRIPTOR,
        .bmAttributes           = USB_CFG_ATTR_RESERVED | USB_CFG_ATTR_SELFPOWERED,
        .bMaxPower              = USB_CFG_POWER_MA(100),
    },
    .comm_iad = {
        .bLength = sizeof(struct usb_iad_descriptor),
        .bDescriptorType        = USB_DTYPE_INTERFASEASSOC,
        .bFirstInterface        = 0,
        .bInterfaceCount        = 2,
        .bFunctionClass         = USB_CLASS_CDC,
        .bFunctionSubClass      = USB_CDC_SUBCLASS_ACM,
        .bFunctionProtocol      = USB_PROTO_NONE,
        .iFunction              = NO_DESCRIPTOR,
    },
    .comm = {
        .bLength                = sizeof(struct usb_interface_descriptor),
        .bDescriptorType        = USB_DTYPE_INTERFACE,
        .bInterfaceNumber       = 0,
        .bAlternateSetting      = 0,
        .bNumEndpoints          = 1,
        .bInterfaceClass        = USB_CLASS_CDC,
        .bInterfaceSubClass     = USB_CDC_SUBCLASS_ACM,
        .bInterfaceProtocol     = USB_PROTO_NONE,
        .iInterface             = NO_DESCRIPTOR,
    },
    .cdc_hdr = {
        .bFunctionLength        = sizeof(struct usb_cdc_header_desc),
        .bDescriptorType        = USB_DTYPE_CS_INTERFACE,
        .bDescriptorSubType     = USB_DTYPE_CDC_HEADER,
        .bcdCDC                 = VERSION_BCD(1,1,0),
    },
    .cdc_mgmt = {
        .bFunctionLength        = sizeof(struct usb_cdc_call_mgmt_desc),
        .bDescriptorType        = USB_DTYPE_CS_INTERFACE,
        .bDescriptorSubType     = USB_DTYPE_CDC_CALL_MANAGEMENT,
        .bmCapabilities         = 0,
        .bDataInterface         = 1,

    },
    .cdc_acm = {
        .bFunctionLength        = sizeof(struct usb_cdc_acm_desc),
        .bDescriptorType        = USB_DTYPE_CS_INTERFACE,
        .bDescriptorSubType     = USB_DTYPE_CDC_ACM,
        .bmCapabilities         = 0,
    },
    .cdc_union = {
        .bFunctionLength        = sizeof(struct usb_cdc_union_desc),
        .bDescriptorType        = USB_DTYPE_CS_INTERFACE,
        .bDescriptorSubType     = USB_DTYPE_CDC_UNION,
        .bMasterInterface0      = 0,
        .bSlaveInterface0       = 1,
    },
    .comm_ep = {
        .bLength                = sizeof(struct usb_endpoint_descriptor),
        .bDescriptorType        = USB_DTYPE_ENDPOINT,
        .bEndpointAddress       = CDC_NTF_EP,
        .bmAttributes           = USB_EPTYPE_INTERRUPT,
        .wMaxPacketSize         = CDC_NTF_SZ,
        .bInterval              = 0xFF,
    },
    .data = {
        .bLength                = sizeof(struct usb_interface_descriptor),
        .bDescriptorType        = USB_DTYPE_INTERFACE,
        .bInterfaceNumber       = 1,
        .bAlternateSetting      = 0,
        .bNumEndpoints          = 2,
        .bInterfaceClass        = USB_CLASS_CDC_DATA,
        .bInterfaceSubClass     = USB_SUBCLASS_NONE,
        .bInterfaceProtocol     = USB_PROTO_NONE,
        .iInterface             = NO_DESCRIPTOR,
    },
    .data_eprx = {
        .bLength                = sizeof(struct usb_endpoint_descriptor),
        .bDescriptorType        = USB_DTYPE_ENDPOINT,
        .bEndpointAddress       = CDC_RXD_EP,
        .bmAttributes           = USB_EPTYPE_BULK,
        .wMaxPacketSize         = CDC_DATA_SZ,
        .bInterval              = 0x01,
    },
    .data_eptx = {
        .bLength                = sizeof(struct usb_endpoint_descriptor),
        .bDescriptorType        = USB_DTYPE_ENDPOINT,
        .bEndpointAddress       = CDC_TXD_EP,
        .bmAttributes           = USB_EPTYPE_BULK,
        .wMaxPacketSize         = CDC_DATA_SZ,
        .bInterval              = 0x01,
    }
};

static const struct usb_string_descriptor lang_desc     = USB_ARRAY_DESC(USB_LANGID_ENG_US);
static const struct usb_string_descriptor manuf_desc_en = USB_STRING_DESC("KTH");
static const struct usb_string_descriptor prod_desc_en  = USB_STRING_DESC("Kung Fu Flash");
static const struct usb_string_descriptor *const dtable[] = {
    &lang_desc,
    &manuf_desc_en,
    &prod_desc_en,
};

static usbd_device udev;
static u32 ubuf[0x20];

static u8 utx_fifo[0x200], urx_fifo[0x200];
static volatile u32 utx_pos = 0, urx_pos = 0;
static volatile bool utx_wait = false, urx_wait = false;

static struct usb_cdc_line_coding cdc_line = {
    .dwDTERate          = 38400,
    .bCharFormat        = USB_CDC_1_STOP_BITS,
    .bParityType        = USB_CDC_NO_PARITY,
    .bDataBits          = 8,
};

static usbd_respond cdc_getdesc (usbd_ctlreq *req, void **address, u16 *length) {
    const u8 dtype = req->wValue >> 8;
    const u8 dnumber = req->wValue & 0xFF;
    const void* desc;
    u16 len = 0;
    switch (dtype) {
    case USB_DTYPE_DEVICE:
        desc = &device_desc;
        break;
    case USB_DTYPE_CONFIGURATION:
        desc = &config_desc;
        len = sizeof(config_desc);
        break;
    case USB_DTYPE_STRING:
        if (dnumber < 3) {
            desc = dtable[dnumber];
        } else {
            return usbd_fail;
        }
        break;
    default:
        return usbd_fail;
    }
    if (len == 0) {
        len = ((struct usb_header_descriptor*)desc)->bLength;
    }
    *address = (void*)desc;
    *length = len;
    return usbd_ack;
}

static usbd_respond cdc_control(usbd_device *dev, usbd_ctlreq *req, usbd_rqc_callback *callback) {
    if (((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) == (USB_REQ_INTERFACE | USB_REQ_CLASS)
        && req->wIndex == 0 ) {
        switch (req->bRequest) {
        case USB_CDC_SET_CONTROL_LINE_STATE:
            return usbd_ack;
        case USB_CDC_SET_LINE_CODING:
            memcpy(&cdc_line, req->data, sizeof(cdc_line));
            return usbd_ack;
        case USB_CDC_GET_LINE_CODING:
            dev->status.data_ptr = &cdc_line;
            dev->status.data_count = sizeof(cdc_line);
            return usbd_ack;
        default:
            return usbd_fail;
        }
    }

    return usbd_fail;
}

static inline bool usb_can_putc(void)
{
    return utx_pos < (sizeof(utx_fifo) - 1);
}

static void usb_putc(char ch)
{
    // Wait for room in the fifo
    while (utx_pos >= (sizeof(utx_fifo) - 1));

    utx_wait = true;
    __DSB();

    utx_fifo[utx_pos++] = ch;
    COMPILER_BARRIER();

    utx_wait = false;
}

static inline bool usb_gotc(void)
{
    return urx_pos > 0;
}

static char usb_getc(void)
{
    // wait for data
    while (!usb_gotc());

    urx_wait = true;
    __DSB();

    char ch = urx_fifo[0];
    memmove(&urx_fifo[0], &urx_fifo[1], --urx_pos);
    COMPILER_BARRIER();

    urx_wait = false;
    __DSB();

    // Enable interrupt if room in buffer
    if (urx_pos <= (sizeof(urx_fifo) - CDC_DATA_SZ))
    {
        _BST(OTG->GINTMSK, USB_OTG_GINTMSK_RXFLVLM);
    }

    return ch;
}

/* CDC loop callback. Both for the Data IN and Data OUT endpoint */
static void cdc_rx_tx(usbd_device *dev, u8 event, u8 ep) {
    if (event == usbd_evt_eptx) {
        u32 pos = utx_pos;
        u16 len = 0;
        if (!utx_wait) {
            len = (pos < CDC_DATA_SZ) ? pos : CDC_DATA_SZ;
        }

        s32 _t = usbd_ep_write(dev, ep, &utx_fifo[0], len);
        if (_t > 0) {
            pos -= _t;
            memmove(&utx_fifo[0], &utx_fifo[_t], pos);
            utx_pos = pos;
        }
    } else {
        u32 pos = urx_pos;
        if (!urx_wait && pos <= (sizeof(urx_fifo) - CDC_DATA_SZ)) {
            s32 _t = usbd_ep_read(dev, ep, &urx_fifo[pos], CDC_DATA_SZ);
            if (_t > 0) {
                urx_pos = pos + _t;
            }
        } else {
            // Disable interrupt otherwise we will be called again immediately,
            // preventing anybody from reading the buffer
            _BCL(OTG->GINTMSK, USB_OTG_GINTMSK_RXFLVLM);
        }
    }
}

static usbd_respond cdc_setconf(usbd_device *dev, u8 cfg) {
    switch (cfg) {
    case 0:
        /* deconfiguring device */
        usbd_ep_deconfig(dev, CDC_NTF_EP);
        usbd_ep_deconfig(dev, CDC_TXD_EP);
        usbd_ep_deconfig(dev, CDC_RXD_EP);
        usbd_reg_endpoint(dev, CDC_RXD_EP, 0);
        usbd_reg_endpoint(dev, CDC_TXD_EP, 0);
        return usbd_ack;
    case 1:
        /* configuring device */
        usbd_ep_config(dev, CDC_RXD_EP, USB_EPTYPE_BULK /*| USB_EPTYPE_DBLBUF*/, CDC_DATA_SZ);
        usbd_ep_config(dev, CDC_TXD_EP, USB_EPTYPE_BULK /*| USB_EPTYPE_DBLBUF*/, CDC_DATA_SZ);
        usbd_ep_config(dev, CDC_NTF_EP, USB_EPTYPE_INTERRUPT, CDC_NTF_SZ);
        // Note: usbd_reg_endpoint only allows one callback for CDC_RXD_EP and CDC_TXD_EP
        usbd_reg_endpoint(dev, CDC_RXD_EP, cdc_rx_tx);
        usbd_reg_endpoint(dev, CDC_TXD_EP, cdc_rx_tx);
        usbd_ep_write(dev, CDC_TXD_EP, 0, 0);
        return usbd_ack;
    default:
        return usbd_fail;
    }
}

static void cdc_init_usbd(void) {
    usbd_init(&udev, &usbd_hw, CDC_EP0_SIZE, ubuf, sizeof(ubuf));
    usbd_reg_config(&udev, cdc_setconf);
    usbd_reg_control(&udev, cdc_control);
    usbd_reg_descr(&udev, cdc_getdesc);
}

void OTG_FS_IRQHandler(void)
{
    usbd_poll(&udev);
}

static void usb_disable(void)
{
    usbd_connect(&udev, false);
    usbd_enable(&udev, false);
    NVIC_DisableIRQ(OTG_FS_IRQn);
}

/*************************************************
* Configure USB FS (PA11/PA12)
*************************************************/
static void usb_config(void)
{
	// Set PA11 and PA12 output speed to very high speed
    GPIOA->OSPEEDR |= (GPIO_OSPEEDR_OSPEED11|GPIO_OSPEEDR_OSPEED12);

    // Set PA11 and PA12 to AF10 (USB_FS)
    MODIFY_REG(GPIOA->AFR[1], GPIO_AFRH_AFSEL11|GPIO_AFRH_AFSEL12,
               GPIO_AFRH_AFSEL11_3|GPIO_AFRH_AFSEL11_1|
               GPIO_AFRH_AFSEL12_3|GPIO_AFRH_AFSEL12_1);

    MODIFY_REG(GPIOA->MODER, GPIO_MODER_MODER11|GPIO_MODER_MODER12,
               GPIO_MODER_MODER11_1|GPIO_MODER_MODER12_1);

    cdc_init_usbd();
    NVIC_SetPriority(OTG_FS_IRQn, 0x07);
    NVIC_EnableIRQ(OTG_FS_IRQn);
    usbd_enable(&udev, true);
    usbd_connect(&udev, true);
}
