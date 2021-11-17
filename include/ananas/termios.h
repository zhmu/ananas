/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>
#include <sys/cdefs.h>

typedef __uint32_t cc_t;
typedef __uint32_t speed_t;
typedef __uint32_t tcflag_t;

/* Subscript names for c_cc array */
#define VEOF 0     /* EOF charachter */
#define VEOL 1     /* EOL charachter */
#define VERASE 2   /* ERASE charachter */
#define VINTR 3    /* INTR charachter */
#define VKILL 4    /* KILL charachter */
#define VMIN VEOF  /* MIN value */
#define VQUIT 5    /* QUIT charachter */
#define VSTART 6   /* START charachter */
#define VSTOP 7    /* STOP charachter */
#define VSUSP 8    /* SUSP charachter */
#define VTIME VEOL /* TIME value */
#define NCCS 10
#define _POSIX_VDISABLE 0

/* Input mode flags */
#define BRKINT 0x0000001 /* Signal interrupt on break */
#define ICRNL 0x0000002  /* Map CR to NL-CR on input */
#define IGNBRK 0x0000004 /* Ignore break condition */
#define IGNCR 0x0000008  /* Ignore CR */
#define IGNPAR 0x0000010 /* Ignore charachters with parity errors */
#define INLCR 0x0000020  /* Map NL to CR on input */
#define INPCK 0x0000040  /* Enable input parity check */
#define ISTRIP 0x0000080 /* Strip charachter */
#define IXANY 0x0000100  /* Enable any charachter to restart output */
#define IXOFF 0x0000200  /* Enable start/stop input control */
#define IXON 0x0000400   /* Enable start/stop output control */
#define PARMRK 0x0000800 /* Mark parity errors */

/* Output mode flags */
#define OPOST 0x0000001  /* Post-process output */
#define ONLCR 0x0000002  /* Map NL to CR-NL on output */
#define ONOCR 0x0000004  /* No CR output at column 0 */
#define ONLRET 0x0000008 /* NL performs CR function */
#define OFILL 0x0000010  /* Use fill charachters for delay */

/* Baud rate selection */
#define B0 0
#define B50 50
#define B75 75
#define B110 110
#define B134 134
#define B150 150
#define B200 200
#define B300 300
#define B600 600
#define B1200 1200
#define B1800 1800
#define B2400 2400
#define B4800 4800
#define B9600 9600
#define B19200 19200
#define B38400 38400

/* Control modes (c_cflag) */
#define CSIZE  0x00000003  /* Size mask */
#define CS5    0x00000000  /* 5 bits */
#define CS6    0x00000001  /* 6 bits */
#define CS7    0x00000002  /* 7 bits */
#define CS8    0x00000003  /* 8 bits */
#define CSTOPB 0x00000004  /* Send two stop bits, else one */
#define CREAD  0x00000008  /* Enable receiver */
#define PARENB 0x00000010  /* Parity enable */
#define PARODD 0x00000020  /* Odd parity, else even */
#define HUPCL  0x00000040  /* Hang up on last close */
#define CLOCAL 0x00000080  /* Ignore modem status lines */

/* Local modes (c_lflag) */
#define ECHO   0x00000001  /* Enable echo */
#define ECHOE  0x00000002  /* Echo erase charachter as error-correcting backspace */
#define ECHOK  0x00000004  /* Echo KILL */
#define ECHONL 0x00000008  /* Echo NL */
#define ICANON 0x00000010  /* Canonical input (erase and kill processing) */
#define IEXTEN 0x00000020  /* Enable extended input charachter processing */
#define ISIG   0x00000040  /* Enable signals */
#define NOFLSH 0x00000080  /* Disable flush after interrupt or quit */
#define TOSTOP 0x00000100  /* Send SIGTTOU for background output*/

struct termios {
    tcflag_t c_iflag; /* Input modes */
    tcflag_t c_oflag; /* Output modes */
    tcflag_t c_cflag; /* Control modes */
    tcflag_t c_lflag; /* Local modes */
    cc_t c_cc[NCCS];  /* Control characters */
    speed_t  c_ispeed; /* Input speed */
    speed_t  c_ospeed; /* Output speed */
};

__BEGIN_DECLS

#define TCSANOW 1
#define TCSADRAIN 2
#define TCSAFLUSH 3

int tcgetattr(int fildes, struct termios* termios_p);
int tcsetattr(int fildes, int optional_actions, const struct termios* termios_p);

speed_t cfgetispeed(const struct termios* termios_p);
speed_t cfgetospeed(const struct termios* termios_p);
int cfsetispeed(struct termios* termios_p, speed_t speed);
int cfsetospeed(struct termios* termios_p, speed_t speed);

#define TCOOFF 1
#define TCOON 2
#define TCIOFF 3
#define TCION 4

int tcflow(int fildes, int action);

#define TCIFLUSH 1
#define TCOFLUSH 2
#define TCIOFLUSH 3

int tcflush(int fildes, int queue_selector);

int tcsendbreak(int fildes, int duration);

__END_DECLS
