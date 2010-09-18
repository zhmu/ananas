#ifndef __ANANAS_ERROR_H__
#define __ANANAS_ERROR_H__

#define ANANAS_ERROR_UNSPECIFIED	0		/* ? */
#define ANANAS_ERROR_BAD_HANDLE		1		/* Supplied handle is invalid or bad type */
#define ANANAS_ERROR_BAD_ADDRESS	2		/* Supplied pointer is invalid */
#define ANANAS_ERROR_OUT_OF_HANDLES	3		/* Cannot allocate handle */
#define ANANAS_ERROR_NO_FILE		4		/* File not found */
#define ANANAS_ERROR_NOT_A_DIRECTORY	5		/* Not a directory */
#define ANANAS_ERROR_BAD_EXEC		6		/* Couldn't execute handle */

#endif /* __ANANAS_ERROR_H__ */
