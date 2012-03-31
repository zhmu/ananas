#include <sys/types.h>

#ifndef __TFTP_H__
#define __TFTP_H__

#define TFTP_OPCODE_RRQ		1	/* read request */
#define TFTP_OPCODE_WRQ		2	/* write request */
#define TFTP_OPCODE_DATA	3	/* data */
#define TFTP_OPCODE_ACK		4	/* ack */
#define TFTP_OPCODE_ERROR	5	/* error */

#define TFTP_ERROR_UNDEF		0	/* not defined */
#define TFTP_ERROR_FILENOTFOUND		1	/* file not found */
#define TFTP_ERROR_ACCESSVIOLATION	2	/* access violation */
#define TFTP_ERROR_DISKFULL		3	/* disk full or allocations exceeded */
#define TFTP_ERROR_ILLEGALOPERATION	4	/* illegal operation */
#define TFTP_ERROR_UNKNOWNID		5	/* unknown transfer id */
#define TFTP_ERROR_FILEALREADYEXISTS	6	/* file already exists */
#define TFTP_ERROR_NOSUCHUSER		7	/* no such user */

#endif /* __TFTP_H__ */
