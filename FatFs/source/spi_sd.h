#ifndef SPI_SD_DEFINED
#define SPI_SD_DEFINED

void spi_gpio_init(void);
void spi_send_byte(char);
char sd_cmd(char, int, char);
int sd_rcv_r3(void);
char sd_rcv_byte(void);

#endif