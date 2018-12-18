/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __UHCI_REG_H__
#define __UHCI_REG_H__

#define UHCI_REG_USBCMD 0x0000         /* USB command register */
#define UHCI_USBCMD_MAXP (1 << 7)      /* Max Packet Size (1=64, 0=32 bytes) */
#define UHCI_USBCMD_CF (1 << 6)        /* Configure Flag */
#define UHCI_USBCMD_SWDBG (1 << 5)     /* Software Debug */
#define UHCI_USBCMD_FGR (1 << 4)       /* Force Global Resume */
#define UHCI_USBCMD_EGSM (1 << 3)      /* Enter Global Suspend Mode */
#define UHCI_USBCMD_GRESET (1 << 2)    /* Global Reset */
#define UHCI_USBCMD_HCRESET (1 << 1)   /* Host Controller Reset */
#define UHCI_USBCMD_RS (1 << 0)        /* Run/Stop */
#define UHCI_REG_USBSTS 0x0002         /* USB status register */
#define UHCI_USBSTS_HCHALTED (1 << 5)  /* Host Controller Halted */
#define UHCI_USBSTS_HCPE (1 << 4)      /* Host Controller Process Error */
#define UHCI_USBSTS_HSE (1 << 3)       /* Host System Error */
#define UHCI_USBSTS_RD (1 << 2)        /* Resume Detect */
#define UHCI_USBSTS_ERR (1 << 1)       /* Error Interrupt */
#define UHCI_USBSTS_USBINT (1 << 0)    /* USB Interrupt */
#define UHCI_REG_USBINTR 0x0004        /* USB interrupt enable register */
#define UHCI_USBINTR_SPI (1 << 3)      /* Short Packet Interrupt Enable */
#define UHCI_USBINTR_IOC (1 << 2)      /* Interrupt On Complete Enable */
#define UHCI_USBINTR_RI (1 << 1)       /* Resume Interrupt Enable */
#define UHCI_USBINTR_TOCRC (1 << 0)    /* Timeout/CRC Interrupt Enable */
#define UHCI_REG_FRNUM 0x0006          /* Frame Number Register */
#define UHCI_REG_FLBASEADD 0x0008      /* Frame List Base Address Register */
#define UHCI_REG_SOF 0x000c            /* Start Of Frame Modify Register */
#define UHCI_REG_PORTSC1 0x0010        /* Port 1 Status And Control Register */
#define UHCI_PORTSC_SUSP (1 << 12)     /* Suspend */
#define UHCI_PORTSC_RESET (1 << 9)     /* Port Reset */
#define UHCI_PORTSC_LOWSPEED (1 << 8)  /* Low Speed Device (R/O) */
#define UHCI_PORTSC_ALWAYS1 (1 << 7)   /* Reserved (yet always 1, some reservation) */
#define UHCI_PORTSC_RD (1 << 6)        /* Resume Detect */
#define UHCI_PORTSC_LS_DMINUS (1 << 5) /* Line Status: D- */
#define UHCI_PORTSC_LS_DPLUS (1 << 4)  /* Line Status: D+ */
#define UHCI_PORTSC_PECHANGE (1 << 3)  /* Port Enable/Disable Change */
#define UHCI_PORTSC_PORTEN (1 << 2)    /* Port Enable/Disable */
#define UHCI_PORTSC_CSCHANGE (1 << 1)  /* Connect Status Change */
#define UHCI_PORTSC_CONNSTAT (1 << 0)  /* Connect Status (R/O) */
#define UHCI_REG_PORTSC2 0x0012        /* Port 1 Status And Control Register */
#define UHCI_PCI_LEGSUPP 0x00c0        /* Legacy Support Register */
#define UHCI_LEGSUPP_A20PTS (1 << 15)  /* End of A20Gate Pass */
#define UHCI_LEGSUPP_PIRQEN (1 << 13)  /* USB interrupt routed to PIRQD */
#define UHCI_LEGSUPP_IRQS (1 << 12)    /* USB IRQ status */
#define UHCI_LEGSUPP_TBY64W (1 << 11)  /* Trap by port 0x64 write */
#define UHCI_LEGSUPP_TBY64R (1 << 10)  /* Trap by port 0x64 read */
#define UHCI_LEGSUPP_TBY60W (1 << 9)   /* Trap by port 0x60 write */
#define UHCI_LEGSUPP_TBY60R (1 << 8)   /* Trap by port 0x60 read */
#define UHCI_LEGSUPP_SMIEPTE (1 << 7)  /* SMI At End Of Pass Through enable */
#define UHCI_LEGSUPP_PSS (1 << 6)      /* Pass Through status */
#define UHCI_LEGSUPP_A20PTEN (1 << 5)  /* A20 Gate Pass Through enable */
#define UHCI_LEGSUPP_SMIEN (1 << 4)    /* Trap/SMI on IRQ enable */
#define UHCI_LEGSUPP_64WEN (1 << 3)    /* Trap on port 0x64 write */
#define UHCI_LEGSUPP_64REN (1 << 2)    /* Trap on port 0x64 read */
#define UHCI_LEGSUPP_60WEN (1 << 1)    /* Trap on port 0x60 write */
#define UHCI_LEGSUPP_60REN (1 << 0)    /* Trap on port 0x60 read */

#define UHCI_PORTSC_MASK(x) \
    ((x) & (UHCI_PORTSC_SUSP | UHCI_PORTSC_RESET | UHCI_PORTSC_RD | UHCI_PORTSC_PORTEN))

struct UHCI_TD {
    uint32_t td_linkptr;                         /* Link pointer */
#define TD_LINKPTR_VF (1 << 2)                   /* Depth(=1)/Breadth(=0) Select */
#define TD_LINKPTR_QH (1 << 1)                   /* QH(=1)/TD(=0) Select */
#define TD_LINKPTR_T (1 << 0)                    /* Terminate */
    uint32_t td_status;                          /* Status */
#define TD_STATUS_SPD (1 << 29)                  /* Short Packet Detect */
#define TD_STATUS_INTONERR(x) ((x) << 27)        /* Interrupt after x errors */
#define TD_STATUS_LS (1 << 26)                   /* Low Speed Device */
#define TD_STATUS_IOS (1 << 25)                  /* Isochronous Select */
#define TD_STATUS_IOC (1 << 24)                  /* Interrupt On Complete */
#define TD_STATUS_ACTIVE (1 << 23)               /* Execution active */
#define TD_STATUS_STALLED (1 << 22)              /* Serious error */
#define TD_STATUS_DATABUFERR (1 << 21)           /* Data Buffer error */
#define TD_STATUS_BABBLE (1 << 20)               /* Babble Detected */
#define TD_STATUS_NAK (1 << 19)                  /* NAK Received */
#define TD_STATUS_CRCTOERR (1 << 18)             /* CRC/Timeout Error */
#define TD_STATUS_BITSTUFF (1 << 17)             /* Bitstuff Error */
#define TD_STATUS_ACTUALLEN(x) (((x)&0x7ff) + 1) /* Actual amount transferred */
#define TD_ACTUALLEN_NONE 0x800
    uint32_t td_token;                     /* Token */
#define TD_TOKEN_MAXLEN(x) (((x)-1) << 21) /* Maximum transfer length */
#define TD_TOKEN_DATA (1 << 19)            /* Data Toggle bit */
#define TD_TOKEN_ENDPOINT(x) ((x) << 15)   /* Endpoint number */
#define TD_TOKEN_ADDRESS(x) ((x) << 8)     /* Device address */
#define TD_PID_IN 0x69
#define TD_PID_OUT 0xe1
#define TD_PID_SETUP 0x2d
#define TD_TOKEN_PID(x) (x)  /* Packet Identification */
    uint32_t td_buffer;      /* Transaction buffer */
    uint32_t td_software[4]; /* Usable by us */
} __attribute__((packed));

struct UHCI_QH {
    uint32_t qh_headptr;    /* Head link pointer */
#define QH_PTR_QH (1 << 1)  /* QH(=1)/TD(=0) Select */
#define QH_PTR_T (1 << 0)   /* Terminate */
    uint32_t qh_elementptr; /* Element pointer */
    uint32_t qh_software[2];
} __attribute__((packed));

#endif /* __UHCI_REG_H__ */
