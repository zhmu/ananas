#ifndef __ANANAS_WII_SD_H__
#define __ANANAS_WII_SD_H__

#define SD_HC_WRITE8	0x1
#define SD_HC_READ8	0x2
#define SD_RESET_CARD	0x4
#define SD_SET_CLOCK	0x6
#define SD_COMMAND	0x7
#define SD_GET_STATUS	0xb
# define SD_STATUS_CARD		1
# define SD_STATUS_NOCARD	2
#define SD_GET_OCR	0xc

/* SD card commands */
#define SD_CMD_GO_IDLE_STATE		0
#define SD_CMD_ALL_SEND_CID		2
#define SD_CMD_SEND_RELATIVE_ADDR	3
#define SD_CMD_SET_DSR			4
#define SD_CMD_SELECT			7
#define SD_CMD_SEND_IF_COND		8
#define SD_CMD_SEND_CSR			9
#define SD_CMD_SEND_CID			10
#define SD_CMD_VOLTAGE_SWITCH		11
#define SD_CMD_STOP_TRANSMISSION	12
#define SD_CMD_SEND_STATUS		13
#define SD_CMD_GO_INACTIVE_STATE	15
#define SD_CMD_SET_BLOCKLEN		16
#define SD_CMD_READ_SINGLE_BLOCK	17
#define SD_CMD_READ_MULTIPLE_BLOCK	18
#define SD_CMD_SEND_TUNING_BLOCK	19
#define SD_CMD_SPEED_CLASS_CONTROL	20
#define SD_CMD_SET_BLOCK_COUNT		23
#define SD_CMD_WRITE_BLOCK		24
#define SD_CMD_WRITE_MULTIPLE_BLOCK	25
#define SD_CMD_PROGRAM_CSD		27
#define SD_CMD_SET_WRITE_PROT		28
#define SD_CMD_CLR_WRITE_PROT		29
#define SD_CMD_SEND_WRITE_PROT		30
#define SD_CMD_ERASE_WR_BLK_START	32
#define SD_CMD_ERASE_WR_BLK_END		33
#define SD_CMD_ERASE			38
#define SD_CMD_LOCK_UNLOCK		42
#define SD_CMD_APP_CMD			55
#define SD_CMD_GEN_CMD			56

#define SD_CMD_APP_SETWIDTH		6

/* Command types */
#define SD_TYPE_BC			1
#define SD_TYPE_BCR			2
#define SD_TYPE_AC			3
#define SD_TYPE_ADTC			4

/* Response types */
#define SD_RESP_NONE			0
#define SD_RESP_R1			1
#define SD_RESP_R1B			2
#define SD_RESP_R2			3
#define SD_RESP_R3			4
#define SD_RESP_R4			5

/* HC registers */
#define SD_HD_HC			0x28	/* Host Control */
# define SD_HD_HC_4BIT			(1 << 1)

#endif /* __ANANAS_WII_SD_H__ */
