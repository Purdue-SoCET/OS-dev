/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include "sdio.h"

/* Definitions of physical drive number for each drive */
#define DEV_SD		0	/* Example: Map SD card to physical drive 0 */


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat;
	int result;

	switch (pdrv) {
	case DEV_SD :
		result = SD_disk_status();
		
		// translate the result code here
		if(result == 0) stat = 0;
		else stat = STA_NOINIT;
		return stat;

	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat;
	int result;

	switch (pdrv) {
	case DEV_SD :
		result = SD_disk_initialize();

		// translate the result code here
		if(result == 0xFF) stat = STA_NOINIT;
		else stat = 0;

		return stat;
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	DRESULT res;
	int result;

	switch (pdrv) {
	case DEV_SD :
		// translate the arguments here

		result = SD_disk_read(buff, sector, count);
		if(result == 0) res = RES_OK;
		else res = RES_ERROR;
		// translate the result code here
		return res;

	}

	return RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	DRESULT res;
	int result;

	switch (pdrv) {
	case DEV_SD :
		// translate the arguments here
		DSTATUS sd_state = disk_status(0);
		if(sd_state) return RES_NOTRDY;
		result = SD_disk_write((unsigned char*) buff, sector, count);
		if(!result) res = RES_OK;
		else res = RES_ERROR;
		// translate the result code here
		return res;

	}

	return RES_PARERR;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res;
	int result;

	switch (pdrv) {
	case DEV_SD :

		// Process of the command for the RAM drive
		if (cmd == CTRL_SYNC) {
			return RES_OK;
		}
		else if(cmd == GET_SECTOR_COUNT) {
			LBA_t* new_buff = (LBA_t*) buff;
			*new_buff = 0x3A0A7FF;
			return RES_OK;
		}
		else if(cmd == GET_SECTOR_SIZE) {
			WORD* new_buff = (WORD*) buff;
			*new_buff = 512;
			return RES_OK;
		}
		else if(cmd == GET_BLOCK_SIZE) {
			DWORD* new_buff = (DWORD*) buff;
			*new_buff = 1;
			return RES_OK;
		}
		return res;
	}

	return RES_PARERR;
}

