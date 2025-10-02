//#include <riscv.h>
#include "pal.h"
#include "spi_sd.h"

void spi_gpio_init(void) {
	volatile unsigned int* GPIO_0_DDR = (unsigned int*) 0x80000004;
	volatile unsigned int* GPIO_0_PER = (unsigned int*) 0x80000012;
	*GPIO_0_DDR &= ~(Pin1);
	*GPIO_0_DDR |= (Pin0) | (Pin2) | (Pin3);
}

/*
 * spi_send_byte
 * parameter: char msg, the packet to send, 8 bits long
 * this function emulates a spi on gpio0 using pins 0-3
 * pin 0 is mosi, pin 1 is miso, pin2 is sclk, pin3 is SS/CS
 * rtv: 
 *
 * */
void spi_send_byte(char msg){
	char new_val = 0;
	volatile unsigned int* GPIO_0_DATA = (unsigned int*) 0x80000000;
	volatile char not_used;

	// *GPIO_0_DATA ^= Pin3;
	for(int i = 17; i > 1; i--){
		if((i & 1)) { // negedge
			new_val = (new_val & ~(1)) | ((msg>>7) &1); // load bit from msg into new_val
			msg <<= 1;
			new_val &= ~Pin2;
		}
		else { // posedge
			not_used = (new_val & ~(1)) | ((msg>>7) &1);
			not_used >>= 1;
			new_val |= Pin2;
		}
		*GPIO_0_DATA = new_val;
	}
	*GPIO_0_DATA &= ~Pin2;

	return;
}

char sd_rcv_byte(void) {
	volatile unsigned int* GPIO_0_DATA = (unsigned int*) 0x80000000;

	char read_data = 0;
	char delay_var = 0;
	// Read in 8 bits
	for(int i = 0; i < 8; i++) {
		*GPIO_0_DATA = Pin0 | Pin2; // posedge

		read_data <<= 1;
		read_data |= (*GPIO_0_DATA & Pin1) >> 1;
		
		*GPIO_0_DATA = Pin0; // negedge

		delay_var <<= 1;
		delay_var |= (*GPIO_0_DATA & Pin1) >> 1;
	}

	return read_data;
}

char sd_cmd(char index, int msg, char crc){
	// first byte: 01 index
	// 2-5 bytes: msg
	// 6 byte: crc
	
	char new_msg = 0;
	
	index = 0b01000000 | index;
	char msg_1 = (msg & 0xFF000000) >> 24;
	char msg_2 = (msg & 0xFF0000) >> 16;
	char msg_3 = (msg & 0xFF00) >> 8;
	char msg_4 = (msg & 0xFF);
	crc = (crc <<1) | 1;
	volatile unsigned int* GPIO_0_DATA = (unsigned int*) 0x80000000;
	
	// *GPIO_0_DATA &= ~Pin3;
	spi_send_byte(index);
	spi_send_byte(msg_1);	
	spi_send_byte(msg_2);
	spi_send_byte(msg_3);
	spi_send_byte(msg_4);
	spi_send_byte(crc);

	int i = 0;
	char rtv = 0xFF;
	while(i < 9 && rtv == 0xFF) {
		rtv = sd_rcv_byte();
		i++;
	}
	// *GPIO_0_DATA |= Pin3;
	return rtv;	
}

int sd_rcv_r3(void){
	int rtv = 0;
	rtv = sd_rcv_byte();
	rtv = (rtv << 8) | sd_rcv_byte();
	rtv = (rtv << 8) | sd_rcv_byte();
	rtv = (rtv << 8) | sd_rcv_byte();
	return rtv;
}
