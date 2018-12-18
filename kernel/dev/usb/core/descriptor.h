/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __UHCI_DESCRIPTOR_H__
#define __UHCI_DESCRIPTOR_H__

#define HUB_MAX_PORTS 127

struct USB_CONTROL_REQUEST {
    uint8_t req_type;                      /* Request type */
#define USB_CONTROL_REQ_DEV2HOST (1 << 7)  /* Device to host */
#define USB_CONTROL_REQ_TYPE(x) ((x) << 5) /* Request type */
#define USB_CONTROL_TYPE_STANDARD 0
#define USB_CONTROL_TYPE_CLASS 1
#define USB_CONTROL_TYPE_VENDOR 2
#define USB_CONTROL_REQ_RECIPIENT(x) (x) /* Request recipient */
#define USB_CONTROL_RECIPIENT_DEVICE 0
#define USB_CONTROL_RECIPIENT_INTERFACE 1
#define USB_CONTROL_RECIPIENT_ENDPOINT 2
#define USB_CONTROL_RECIPIENT_OTHER 3
    uint8_t req_request; /* Request */
#define USB_CONTROL_REQUEST_GET_STATUS 0x00
#define USB_CONTROL_REQUEST_CLEAR_FEATURE 0x01
#define USB_CONTROL_REQUEST_GET_STATE 0x02 /* Hub only */
#define USB_CONTROL_REQUEST_SET_FEATURE 0x03
#define USB_CONTROL_REQUEST_SET_ADDRESS 0x05
#define USB_CONTROL_REQUEST_GET_DESC 0x06
#define USB_CONTROL_REQUEST_SET_DESC 0x07
#define USB_CONTROL_REQUEST_GET_CONFIGURATION 0x08
#define USB_CONTROL_REQUEST_SET_CONFIGURATION 0x09
#define USB_CONTROL_REQUEST_GET_INTERFACE 0x0a
#define USB_CONTROL_REQUEST_SET_INTERFACE 0x0b
#define USB_CONTROL_REQUEST_SYNC_FRAME 0x0c
#define USB_CONTROL_REQUEST_SET_SEL 0x30
#define USB_CONTROL_REQUEST_SET_ISO_DELAY 0x31
#define USB_CONTROL_REQUEST_GET_MAX_LUN 0xfe
    uint16_t req_value;  /* Value */
    uint16_t req_index;  /* Index value */
    uint16_t req_length; /* Request Length */
} __attribute__((packed));

#define USB_REQUEST_MAKE(type, req) (((type) << 8) | (req))

/* Standard requests */
#define USB_REQUEST_STANDARD_GET_DESCRIPTOR                                          \
    USB_REQUEST_MAKE(                                                                \
        USB_CONTROL_REQ_DEV2HOST | USB_CONTROL_REQ_TYPE(USB_CONTROL_TYPE_STANDARD) | \
            USB_CONTROL_REQ_RECIPIENT(USB_CONTROL_RECIPIENT_DEVICE),                 \
        USB_CONTROL_REQUEST_GET_DESC)

#define USB_REQUEST_STANDARD_SET_ADDRESS                             \
    USB_REQUEST_MAKE(                                                \
        USB_CONTROL_REQ_TYPE(USB_CONTROL_TYPE_STANDARD) |            \
            USB_CONTROL_REQ_RECIPIENT(USB_CONTROL_RECIPIENT_DEVICE), \
        USB_CONTROL_REQUEST_SET_ADDRESS)

#define USB_REQUEST_STANDARD_SET_CONFIGURATION                       \
    USB_REQUEST_MAKE(                                                \
        USB_CONTROL_REQ_TYPE(USB_CONTROL_TYPE_STANDARD) |            \
            USB_CONTROL_REQ_RECIPIENT(USB_CONTROL_RECIPIENT_DEVICE), \
        USB_CONTROL_REQUEST_SET_CONFIGURATION)

/* Hub requests */
#define USB_REQUEST_CLEAR_HUB_FEATURE                                \
    USB_REQUEST_MAKE(                                                \
        USB_CONTROL_REQ_TYPE(USB_CONTROL_TYPE_CLASS) |               \
            USB_CONTROL_REQ_RECIPIENT(USB_CONTROL_RECIPIENT_DEVICE), \
        USB_CONTROL_REQUEST_CLEAR_FEATURE)
#define USB_REQUEST_CLEAR_PORT_FEATURE                              \
    USB_REQUEST_MAKE(                                               \
        USB_CONTROL_REQ_TYPE(USB_CONTROL_TYPE_CLASS) |              \
            USB_CONTROL_REQ_RECIPIENT(USB_CONTROL_RECIPIENT_OTHER), \
        USB_CONTROL_REQUEST_CLEAR_FEATURE)
#define USB_REQUEST_GET_BUS_STATE                                                 \
    USB_REQUEST_MAKE(                                                             \
        USB_CONTROL_REQ_DEV2HOST | USB_CONTROL_REQ_TYPE(USB_CONTROL_TYPE_CLASS) | \
            USB_CONTROL_REQ_RECIPIENT(USB_CONTROL_RECIPIENT_OTHER),               \
        USB_CONTROL_REQUEST_GET_STATE)
#define USB_REQUEST_GET_HUB_DESCRIPTOR                                            \
    USB_REQUEST_MAKE(                                                             \
        USB_CONTROL_REQ_DEV2HOST | USB_CONTROL_REQ_TYPE(USB_CONTROL_TYPE_CLASS) | \
            USB_CONTROL_REQ_RECIPIENT(USB_CONTROL_RECIPIENT_DEVICE),              \
        USB_CONTROL_REQUEST_GET_DESC)
#define USB_REQUEST_GET_HUB_STATUS                                                \
    USB_REQUEST_MAKE(                                                             \
        USB_CONTROL_REQ_DEV2HOST | USB_CONTROL_REQ_TYPE(USB_CONTROL_TYPE_CLASS) | \
            USB_CONTROL_REQ_RECIPIENT(USB_CONTROL_RECIPIENT_DEVICE),              \
        USB_CONTROL_REQUEST_GET_STATUS)
#define USB_REQUEST_GET_PORT_STATUS                                               \
    USB_REQUEST_MAKE(                                                             \
        USB_CONTROL_REQ_DEV2HOST | USB_CONTROL_REQ_TYPE(USB_CONTROL_TYPE_CLASS) | \
            USB_CONTROL_REQ_RECIPIENT(USB_CONTROL_RECIPIENT_OTHER),               \
        USB_CONTROL_REQUEST_GET_STATUS)
#define USB_REQUEST_SET_HUB_DESCRIPTOR                               \
    USB_REQUEST_MAKE(                                                \
        USB_CONTROL_REQ_TYPE(USB_CONTROL_TYPE_CLASS) |               \
            USB_CONTROL_REQ_RECIPIENT(USB_CONTROL_RECIPIENT_DEVICE), \
        USB_CONTROL_REQUEST_SET_DESC)
#define USB_REQUEST_SET_HUB_FEATURE                                  \
    USB_REQUEST_MAKE(                                                \
        USB_CONTROL_REQ_TYPE(USB_CONTROL_TYPE_CLASS) |               \
            USB_CONTROL_REQ_RECIPIENT(USB_CONTROL_RECIPIENT_DEVICE), \
        USB_CONTROL_REQUEST_SET_FEATURE)
#define USB_REQUEST_SET_PORT_FEATURE                                \
    USB_REQUEST_MAKE(                                               \
        USB_CONTROL_REQ_TYPE(USB_CONTROL_TYPE_CLASS) |              \
            USB_CONTROL_REQ_RECIPIENT(USB_CONTROL_RECIPIENT_OTHER), \
        USB_CONTROL_REQUEST_SET_FEATURE)

/* Hub feature selectors */
#define HUB_FEATURE_C_HUB_LOCAL_POWER 0
#define HUB_FEATURE_C_HUB_OVER_CURRENT 1
#define HUB_FEATURE_PORT_CONNECTION 0
#define HUB_FEATURE_PORT_ENABLE 1
#define HUB_FEATURE_PORT_SUSPEND 2
#define HUB_FEATURE_PORT_OVER_CURRENT 3
#define HUB_FEATURE_PORT_RESET 4
#define HUB_FEATURE_PORT_POWER 8
#define HUB_FEATURE_PORT_LOW_SPEED 9
#define HUB_FEATURE_C_PORT_CONNECTION 16
#define HUB_FEATURE_C_PORT_ENABLE 17
#define HUB_FEATURE_C_PORT_SUSPEND 18
#define HUB_FEATURE_C_PORT_OVER_CURRENT 19
#define HUB_FEATURE_C_PORT_RESET 20

struct USB_HUB_PORTSTATUS {
    uint16_t ps_portstatus;
#define USB_HUB_PS_PORT_CONNECTION (1 << 0)
#define USB_HUB_PS_PORT_ENABLE (1 << 1)
#define USB_HUB_PS_PORT_SUSPEND (1 << 2)
#define USB_HUB_PS_PORT_OVER_CURRENT (1 << 3)
#define USB_HUB_PS_PORT_RESET (1 << 4)
#define USB_HUB_PS_PORT_POWER (1 << 8)
#define USB_HUB_PS_PORT_LOW_SPEED (1 << 9)
    uint16_t ps_portchange;
#define USB_HUB_PC_C_PORT_CONNECTION (1 << 0)
#define USB_HUB_PC_C_PORT_ENABLE (1 << 1)
#define USB_HUB_PC_C_PORT_SUSPEND (1 << 2)
#define USB_HUB_PC_C_PORT_OVER_CURRENT (1 << 3)
#define USB_HUB_PC_C_PORT_RESET (1 << 4)
};

/* Descriptor types */
#define USB_DESCR_TYPE_DEVICE 1
#define USB_DESCR_TYPE_CONFIG 2
#define USB_DESCR_TYPE_STRING 3
#define USB_DESCR_TYPE_INTERFACE 4
#define USB_DESCR_TYPE_ENDPOINT 5
#define USB_DESCR_TYPE_HUB 0x29

/* Generic descriptor*/
struct USB_DESCR_GENERIC {
    uint8_t gen_length; /* Length in bytes */
    uint8_t gen_type;   /* Descriptor type */
};

/* Device descriptor */
struct USB_DESCR_DEVICE {
    uint8_t dev_length;   /* Length in bytes */
    uint8_t dev_type;     /* Descriptor type */
    uint16_t dev_version; /* USB version (BCD) */
    uint8_t dev_class;    /* Device class */
#define USB_DESCR_CLASS_COMMUNICATION 2
#define USB_DESCR_CLASS_HUB 9
#define USB_DESCR_CLASS_
    uint8_t dev_subclass;        /* Device subclass */
    uint8_t dev_protocol;        /* Device protocol code */
    uint8_t dev_maxsize0;        /* Maximum size of endpoint 0 */
    uint16_t dev_vendor;         /* Vendor ID */
    uint16_t dev_product;        /* Product ID */
    uint16_t dev_release;        /* Product release (BCD) */
    uint8_t dev_manufactureridx; /* Manufacturer string index */
    uint8_t dev_productidx;      /* Product string index */
    uint8_t dev_serialidx;       /* Serial number string index */
    uint8_t dev_num_configs;     /* Number of configurations */
} __attribute__((packed));

/* Configuration descriptor */
struct USB_DESCR_CONFIG {
    uint8_t cfg_length;        /* Length in bytes */
    uint8_t cfg_type;          /* Descriptor type */
    uint16_t cfg_totallen;     /* Total length */
    uint8_t cfg_numinterfaces; /* Number of interfaces */
    uint8_t cfg_identifier;    /* Identifier for get/set requests */
    uint8_t cfg_stringidx;     /* String index */
    uint8_t cfg_attrs;         /* Attributes */
    uint8_t cfg_maxpower;      /* Maximum bus power use */
} __attribute__((packed));

/* String descriptor */
struct USB_DESCR_STRING {
    uint8_t str_length; /* Length in bytes */
    uint8_t str_type;   /* Descriptor type */
    union {
        uint16_t str_langid[1]; /* Language ID */
        uint16_t str_string[1]; /* Unicode values */
    } u;
} __attribute__((packed));

/* Interface descriptor */
struct USB_DESCR_INTERFACE {
    uint8_t if_length;       /* Length in bytes */
    uint8_t if_type;         /* Descriptor type */
    uint8_t if_number;       /* Interface number */
    uint8_t if_altsetting;   /* Alternate setting */
    uint8_t if_numendpoints; /* Number of endpoints */
    uint8_t if_class;        /* Class code */
#define USB_IF_CLASS_AUDIO 1
#define USB_IF_CLASS_COMM 2
#define USB_IF_CLASS_HID 3
#define USB_IF_CLASS_PHYSICAL 5
#define USB_IF_CLASS_IMAGE 6
#define USB_IF_CLASS_PRINTER 7
#define USB_IF_CLASS_STORAGE 8
#define USB_IF_PROTOCOL_BULKONLY 0x50
#define USB_IF_CLASS_HUB 9
#define USB_IF_CLASS_COMM_DATAINT 10
#define USB_IF_CLASS_SMARTCARD 11
#define USB_IF_CLASS_CONTENTSECURITY 13
#define USB_IF_CLASS_VIDEO 14
#define USB_IF_CLASS_HEALTHCARE 15
    uint8_t if_subclass;     /* Subclass code */
    uint8_t if_protocol;     /* Protocol code */
    uint8_t if_interfaceidx; /* Interface string index */
} __attribute__((packed));

/* Endpoint descriptor */
struct USB_DESCR_ENDPOINT {
    uint8_t ep_length; /* Length in bytes */
    uint8_t ep_type;   /* Descriptor type */
    uint8_t ep_addr;   /* Endpoint number and type */
#define USB_EP_ADDR_IN (1 << 7)
#define USB_EP_ADDR(x) ((x)&0xf)
    uint8_t ep_attr; /* Attributes */
#define USB_PE_ATTR_TYPE(x) ((x)&3)
#define USB_PE_ATTR_TYPE_CONTROL 0
#define USB_PE_ATTR_TYPE_ISOCHRONOUS 1
#define USB_PE_ATTR_TYPE_BULK 2
#define USB_PE_ATTR_TYPE_INTERRUPT 3
    uint16_t ep_maxpacketsz; /* Maximum packet size */
    uint8_t ep_interval;     /* Service interval */
} __attribute__((packed));

/* Interface association descriptor */
struct USB_DESCR_IASSOC {
    uint8_t ia_length;      /* Length in bytes */
    uint8_t ia_type;        /* Descriptor type */
    uint8_t ia_firstif;     /* First interface */
    uint8_t ia_ifcount;     /* Interface count */
    uint8_t ia_class;       /* Class */
    uint8_t ia_subclass;    /* Subclass */
    uint8_t ia_protocol;    /* Protocol */
    uint8_t ia_functionidx; /* Function string descriptor */
} __attribute__((packed));

/* Hub descriptor */
struct USB_DESCR_HUB {
    uint8_t hd_length;   /* Length in bytes */
    uint8_t hd_type;     /* Descriptor type */
    uint8_t hd_numports; /* Number of downstream ports */
    uint8_t hd_flags;    /* Characteristics */
#define USB_HD_FLAG_PS_GANGED (0 << 0)
#define USB_HD_FLAG_PS_INDIVIDUAL (1 << 0)
#define USB_HD_FLAG_COMPOUND (1 << 2)
#define USB_HD_FLAG_OC_GLOBAL (0 << 3)
#define USB_HD_FLAG_OC_INDIVIDUAL (1 << 3)
#define USB_HD_FLAG_OC_NONE (2 << 3)
    uint8_t hd_poweron2good; /* Time from power-on to power-good */
    uint8_t hd_max_current;  /* Maximum current requirements */
    /* Removable mask, depending on number of ports */
    uint8_t hd_removable[((HUB_MAX_PORTS * 2) + 7) / 8];
} __attribute__((packed));

#endif /* __UHCI_DESCRIPTOR_H__ */
