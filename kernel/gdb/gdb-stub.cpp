/****************************************************************************

                THIS SOFTWARE IS NOT COPYRIGHTED

   HP offers the following for use in the public domain.  HP makes no
   warranty with regard to the software or it's performance and the
   user accepts the software "AS IS" with all faults.

   HP DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD
   TO THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

****************************************************************************/

/****************************************************************************
 *  Header: remcom.c,v 1.34 91/03/09 12:29:49 glenne Exp $
 *
 *  Module name: remcom.c $
 *  Revision: 1.34 $
 *  Date: 91/03/09 12:29:49 $
 *  Contributor:     Lake Stevens Instrument Division$
 *
 *  Description:     low level support for gdb debugger. $
 *
 *  Considerations:  only works on target hardware $
 *
 *  Written by:      Glenn Engel $
 *  ModuleState:     Experimental $
 *
 *  NOTES:           See Below $
 *
 *  Modified for 386 by Jim Kingdon, Cygnus Support.
 *  Modified for Ananas/amd64 by Rink Springer
 *
 *************
 *
 *    The following gdb commands are supported:
 *
 * command          function                               Return value
 *
 *    g             return the value of the CPU registers  hex data or ENN
 *    G             set the value of the CPU registers     OK or ENN
 *
 *    mAA..AA,LLLL  Read LLLL bytes at address AA..AA      hex data or ENN
 *    MAA..AA,LLLL: Write LLLL bytes at address AA.AA      OK or ENN
 *
 *    c             Resume at current address              SNN   ( signal NN)
 *    cAA..AA       Continue at address AA..AA             SNN
 *
 *    s             Step one instruction                   SNN
 *    sAA..AA       Step one instruction from AA..AA       SNN
 *
 *    k             kill
 *
 *    ?             What was the last sigval ?             SNN   (signal NN)
 *
 * All commands and responses are sent with a packet which includes a
 * checksum.  A packet consists of
 *
 * $<packet info>#<checksum>.
 *
 * where
 * <packet info> :: <characters representing the command or response>
 * <checksum>    :: < two hex digits computed as modulo 256 sum of <packetinfo>>
 *
 * When a packet is received, it is first acknowledged with either '+' or '-'.
 * '+' indicates a successful transfer.  '-' indicates a failed transfer.
 *
 * Example:
 *
 * Host:                  Reply:
 * $m0,10#2a               +$00010203040506070809101112131415#42
 *
 ****************************************************************************/

#include <ananas/types.h>
#include "kernel/debug-console.h"
#include "kernel/gdb.h"
#include "kernel/lib.h"
#include "kernel-md/frame.h"
#include "kernel-md/gdb-support.h"

namespace
{
    inline int getDebugChar()
    {
        while (true) {
            int ch = debugcon_getch();
            if (ch != 0)
                return ch;
        }
    }

    inline void putDebugChar(int ch) { debugcon_putch(ch); }

/************************************************************************/
/* BUFMAX defines the maximum number of characters in inbound/outbound buffers*/
/* at least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX 400

    static const char hexchars[] = "0123456789abcdef";

    int parse_hexDigit(char ch)
    {
        if (ch >= '0' && ch <= '9')
            return ch - '0';
        if (ch >= 'a' && ch <= 'z')
            ch -= 0x20; // make uppercase
        if (ch >= 'A' && ch <= 'F')
            return ch - 'A' + 10;
        return -1;
    }

    /* scan for the sequence $<data>#<checksum>     */

    char* gdb_read_packet()
    {
        static uint8_t buffer[BUFMAX];
        while (true) {
            // wait around for the start character, ignore all other characters
            while (getDebugChar() != '$')
                /* nothing */;

        retry:
            // Now, read until a # or end of buffer is found
            uint8_t checksum = 0;
            int count = 0;
            while (count < BUFMAX - 1) {
                uint8_t ch = getDebugChar();
                if (ch == '$')
                    goto retry;
                if (ch == '#')
                    break;
                // XXX we should do escaping here
                checksum += ch;
                buffer[count] = ch;
                count++;
            }
            buffer[count] = 0;

            // Fetch the checksum
            uint8_t ch = getDebugChar();
            uint8_t xmitcsum = parse_hexDigit(ch) << 4;
            ch = getDebugChar();
            xmitcsum += parse_hexDigit(ch);
            if (checksum != xmitcsum) {
                putDebugChar('-'); /* failed checksum */
            } else {
                putDebugChar('+'); /* successful transfer */
                return (char*)&buffer[0];
            }
        }

        // NOTREACHED
    }

    // send the packet in buffer - waits until positive acknowledgement
    void gdb_send_packet(unsigned char* buffer)
    {
        /*  $<packet info>#<checksum>.  */
        do {
            putDebugChar('$');
            uint8_t checksum = 0;
            int count = 0;
            while (uint8_t ch = buffer[count]) {
                putDebugChar(ch);
                checksum += ch;
                count++;
            }

            putDebugChar('#');
            putDebugChar(hexchars[checksum >> 4]);
            putDebugChar(hexchars[checksum % 16]);
        } while (getDebugChar() != '+');
    }

    char* mem2hex(char* mem, char* buf, size_t count, bool& faulted)
    {
        for (size_t i = 0; i < count; i++) {
            uint8_t ch = *(uint8_t*)mem++;
            *buf++ = hexchars[ch >> 4];
            *buf++ = hexchars[ch % 16];
        }
        *buf = 0;

        faulted = false; // TODO: implement this sensibly
        return buf;
    }

    char* reg2hex(struct STACKFRAME* sf, int regnum, char* buf)
    {
        void* reg = gdb_md_get_register(sf, regnum);
        size_t size = gdb_md_get_register_size(regnum);

        if (reg == NULL) {
            while (size-- > 0) {
                *buf++ = 'x';
                *buf++ = 'x';
            }
            return buf;
        }

        bool dummy;
        return mem2hex((char*)reg, buf, size, dummy);
    }

    void hex2mem(char* buf, char* mem, size_t count)
    {
        for (size_t i = 0; i < count; i++) {
            uint8_t ch = parse_hexDigit(*buf++) << 4;
            ch |= parse_hexDigit(*buf++);
            *mem++ = ch; // TODO deal with faults
        }
    }

    int parse_hex(char*& ptr, register_t& result)
    {
        int n = 0;

        result = 0;
        while (*ptr != '\0') {
            int nibble = parse_hexDigit(*ptr);
            if (nibble < 0)
                break;
            result = (result << 4) | nibble;
            n++, ptr++;
        }

        return n;
    }

} // unnamed namespace

void gdb_handle_exception(struct STACKFRAME* sf)
{
    static char reply[BUFMAX];

    // Reply to host that an exception has occurred
    int sigval = gdb_md_map_signal(sf);
    {
        char* ptr = reply;

        *ptr++ = 'T'; /* notify gdb with signo, PC, FP and SP */
        *ptr++ = hexchars[sigval >> 4];
        *ptr++ = hexchars[sigval & 0xf];

        *ptr++ = hexchars[GDB_REG_PC >> 4];
        *ptr++ = hexchars[GDB_REG_PC & 0xf];
        *ptr++ = ':';
        ptr = reg2hex(sf, GDB_REG_PC, ptr);
        *ptr++ = ';';
        *ptr = '\0';

        gdb_send_packet((unsigned char*)reply);
    }

    while (true) {
        reply[0] = 0;
        char* ptr = gdb_read_packet();

        switch (*ptr++) {
            case '?':
                reply[0] = 'S';
                reply[1] = hexchars[sigval >> 4];
                reply[2] = hexchars[sigval % 16];
                reply[3] = 0;
                break;
            case 'g': { /* return the value of the CPU registers */
                ptr = reply;
                for (int n = 0; n < GDB_NUMREGS; n++) {
                    ptr = reg2hex(sf, n, ptr);
                }
                break;
            }
            case 'G': // set the value of the CPU registers
                strcpy(reply, "E01");
                break;
            case 'P': // set the value of a single CPU register
                strcpy(reply, "E01");
                break;
            case 'm': { // mAAAA,LLLL  Read LLLL bytes at address AAAA
                register_t addr, length;
                if (parse_hex(ptr, addr) && *ptr++ == ',' && parse_hex(ptr, length)) {
                    bool faulted;
                    mem2hex((char*)addr, reply, length, faulted);
                    if (faulted)
                        strcpy(reply, "E03");
                } else {
                    strcpy(reply, "E01");
                }
                break;
            }
            case 'M': { /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
                /* TRY TO READ '%x,%x:'.  IF SUCCEED, SET PTR = 0 */
                register_t addr, length;
                if (parse_hex(ptr, addr) && *(ptr++) == ',' && parse_hex(ptr, length) &&
                    *(ptr++) == ':') {
                    hex2mem(ptr, (char*)addr, length); // XXX could fault
                    // strcpy (reply, "E03"); // on fault
                    strcpy(reply, "OK");
                } else {
                    strcpy(reply, "E02");
                }
                break;
            }
        }

        gdb_send_packet((unsigned char*)reply);
    }
}
