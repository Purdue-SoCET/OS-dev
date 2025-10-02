#include "pal.h"
#include "sdio.h"
#include "spi_sd.h"
#include <stdio.h>

int SD_disk_status() {
	sd_cmd(58,0,1);
	uint32_t ocr = sd_rcv_r3();
	if(ocr & 0x8000) {
		return 0;
	}
	else {
		printf("disk_status failed\n");
		return -1;
	}

};

int SD_disk_initialize() {
	//this is disk_initialize, one of the 3 required driver functions. 
	//That has a DSTATUS return value though, so change name later
	spi_gpio_init();
	volatile unsigned int* GPIO_0_DATA = (unsigned int*) 0x80000000;
	volatile char new_val = 0;
	char msg = 0xFF;
	volatile char not_used;
	int r3_resp;

	*GPIO_0_DATA |= Pin3 | Pin0; // sets cs and MOSI high
	new_val |= Pin3 | Pin0;
	volatile int k;
	for(int i = 104; i > 0; i--) {
		new_val ^= Pin2;
		*GPIO_0_DATA = new_val; // posedge
		k++; k++; k++; k++; k++; k++; //k++; k++; k++; // 9
	
		new_val ^= Pin2;
		*GPIO_0_DATA = new_val; // negedge
		k++; k++; k++; k++; k++; k++; //k++; k++; k++; // 9
	}

	*GPIO_0_DATA &= ~Pin3; // set CS low
	char err_code = sd_cmd(0,0,0x4A);
	if(err_code !=0x1) {
		*GPIO_0_DATA |= Pin3; // set CS High
		return err_code;
	}
	err_code = sd_cmd(8,0x1AA,0x43);
	r3_resp = sd_rcv_r3();
	for(int i = 0; i< 1000; i ++){
		//send 55, then cmd 41, acmd 41 is 55+41
		//attempt 1000 times, if this loop did failed, the sd card did not init
		err_code = sd_cmd(55,0,0x0);
		if(err_code == 1){
			err_code = sd_cmd(41,0x40000000,0x0);
			if(err_code == 0) goto init_suc;		
		} 
	}
	printf("Failed ACMD41 Loop\n");
	*GPIO_0_DATA |= Pin3; // set CS High
	return -1;

	init_suc:
	err_code = sd_cmd(58,0,1);
	r3_resp = sd_rcv_r3();	
	err_code = sd_cmd(16,0x200,1);
	*GPIO_0_DATA |= Pin3; // set CS High
	return 1;
};

int SD_disk_read(unsigned char* buffer, uint32_t sector_no, unsigned int count) {
	int attempts;
	uint8_t response;
	// Completes a multi block read via CMD18

	// Loop through each sector
	for (int cur_sec = 0; cur_sec < count; cur_sec++) {
		if(sd_read_block(buffer+512*cur_sec, sector_no+cur_sec)) {
			printf("Failed on read of block %d/%d. Actually block %d\n", cur_sec+1,count, sector_no);
			return -1;
		}
	}

	return 0;
};

int sd_read_block(unsigned char* buffer, uint32_t sector_no) {
	volatile unsigned int* GPIO_0_DATA = (unsigned int*) 0x80000000;
	
	*GPIO_0_DATA &= ~Pin3; // set CS low
	int rtv = sd_cmd(17, sector_no, 0);
	if(rtv != 0x00) {
		*GPIO_0_DATA |= Pin3; // set CS High
		printf("Attempted reading of SD card at block %d, command 17 gave no response\n", sector_no);
		return -1;
	}
	
	// Reading Data Token
	int attempts = 0;
	const int MAX_ATTEMPTS = 512*8;
	uint8_t response = 0xFF;
	uint8_t new_val = Pin0;

	while ((response == 0xFF) && (attempts < MAX_ATTEMPTS)) {
		new_val ^= Pin2;
		*GPIO_0_DATA = new_val; // posedge

		if(!(*GPIO_0_DATA & Pin1)) {
			new_val ^= Pin2;
			*GPIO_0_DATA = new_val; // negedge
			break;
		}
		
		new_val ^= Pin2;
		*GPIO_0_DATA = new_val; // negedge
		
		attempts++;
	}
	if(attempts == MAX_ATTEMPTS) {
		printf("Timed out waiting for data token\n");
		return -1;
	};

	// Reading in data values
	for (int cur_byte = 0; cur_byte < 512; cur_byte++) {
		buffer[cur_byte] = sd_rcv_byte();
	}

	// Taking in CRC, not used
	sd_rcv_byte();
	sd_rcv_byte();

	*GPIO_0_DATA |= Pin3; // set CS High
	spi_send_byte(0xFF);
	return 0;
}

int SD_disk_write(unsigned char* buffer, uint32_t sector_no, unsigned int count) {
	volatile unsigned int* GPIO_0_DATA = (unsigned int*) 0x80000000;
	*GPIO_0_DATA &= ~Pin3; // set CS low
    for(int cur_sec = 0; cur_sec < count; cur_sec++){
	//single block writes
		int rtv = sd_cmd(24,sector_no + cur_sec, 0);
		if(rtv != 0x00){
			*GPIO_0_DATA |= Pin3; // set CS High
			printf("SD_disk_write: Command 24 responded with 0x%2x instead of 0x00\n", rtv);
			return -1;
		}

		// Send one buffer byte
		spi_send_byte(0xFF);
		// TEST: sending 2 additional
		spi_send_byte(0xFF);
		spi_send_byte(0xFF);

		//send every byte in the 512. Start with the token, which is 0xFE
		spi_send_byte(0xFE);
		for(int cur_byte = 0; cur_byte < 512; cur_byte++){
			spi_send_byte(buffer[cur_sec*512 + cur_byte]);
		}
		//Pad the end of the sector write with CRC.
		spi_send_byte(0);
		spi_send_byte(0);
		//Poll the data response.
		uint8_t data_response = sd_rcv_byte();
		if ((data_response & 0x1F) != 0x5) {
			*GPIO_0_DATA |= Pin3; // set CS high
			printf("Attempted to write SD with CMD 24, got back invalid Data Response %x\n", data_response);
			return -1;
		}
		int attempts = 0;
		while((sd_rcv_byte() == 0x00) && (attempts < 512*8)) {
			attempts++;
		}
		if(attempts == 512*8) {
			*GPIO_0_DATA |= Pin3; // set CS high
			printf("Attempting write, stuck in BUSY\n");
			return -1;
		}
	}
	// printf("SD_disk_write: completed, exiting with success code of 1.\n");
	// printf("Blocks %d to %d were written to!\n", sector_no, sector_no + count);
	*GPIO_0_DATA |= Pin3; // set CS high
	spi_send_byte(0xFF);
	return 0;
};
