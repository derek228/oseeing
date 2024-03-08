#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <getopt.h>
#include <linux/types.h>
#include <time.h>
//#include <sys/socket.h>
//#include <net/if.h>
//#include <curl/curl.h>
#include "spidev.h"
#include "cloud-service.h"
#include "ini-parse.h"
//#include "../leds/pwm_message.h"
//#include "led_control.h"
#include "jpegenc.h"
#include "alarm.h"
#include "mi48.h"

#define NEW_THERMAL_SCAN 1
// Thermal sensor hardware signal setting
#define CAP_SIG_ID          0x0a 
#define CMD_SIG_TASK_REG    _IOW(CAP_SIG_ID, 0, int32_t*)
#define CAP_MAX_LINE                32
#define CAP_INFO_LEN_UNIT_BYTE      12
#define CAP_MAX_LEN                 (CAP_MAX_LINE * CAP_INFO_LEN_UNIT_BYTE)
#define GPIO_INFO_SIZE_INT          3
#define CAP_MAX_LEN_INT             (CAP_MAX_LEN/4)
//gpio
int state_change = 0;
// SPI interface config
static const char *device = "/dev/spidev1.0";
//static const char *device = "/dev/spidev0.1";
//static const char *device = "/dev/spidev1.1";
static uint32_t mode=0;
static uint8_t bits = 8;
static uint32_t speed = 4000000; //500000    4000000
static uint16_t delay=0;
static int fd_spi;
static int fd_capture;
static uint8_t *tx;
static uint8_t *rx;
static int size;
static uint8_t data[9920]={0};

//static char mi48_header_raw[160]={0};
static mi48_header_t mi48_header;
static int mi48_debug=0;
static void mi48_log_print() {
	const char *filename = "/mnt/mtdblock1/mi48";
	if (access(filename, F_OK) != -1) {
		mi48_debug=1;
	} else {
		mi48_debug=0;
    }
}

void sig_event_handler(int sig_id, siginfo_t *sig_info, void *unused)
{
	if ( sig_id == CAP_SIG_ID ) {
		state_change = 1;
	}
}


#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
static void pabort(const char *s)
{
	perror(s);
	abort();
}

static void transfer(int fd, uint8_t const *tx, uint8_t const *rx, size_t len)
{
	int i;
	int ret;
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = len,
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};
	if (mode & SPI_TX_QUAD)
		tr.tx_nbits = 4;
	else if (mode & SPI_TX_DUAL)
		tr.tx_nbits = 2;
	if (mode & SPI_RX_QUAD)
		tr.rx_nbits = 4;
	else if (mode & SPI_RX_DUAL)
		tr.rx_nbits = 2;
	if (!(mode & SPI_LOOP)) {
		if (mode & (SPI_TX_QUAD | SPI_TX_DUAL))
			tr.rx_buf = 0;
		else if (mode & (SPI_RX_QUAD | SPI_RX_DUAL))
			tr.tx_buf = 0;
	}

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
}

static int memory_print() {
    unsigned long long free_memory;
    FILE *fp = fopen("/proc/meminfo", "r");
    if(fp == NULL) {
        printf("Error: Failed to open /proc/meminfo\n");
        return 1;
    }
    char line[128];
    while(fgets(line, sizeof(line), fp)) {
        if(sscanf(line, "MemFree: %llu kB", &free_memory) == 1) {
            free_memory *= 1024;
            break;
        }
    }
    fclose(fp);
    logd(mi48_debug,"Free memory: %llu bytes\n", free_memory);
    return 0;
}

static int ir8062_hwinit()
{
	//SPI
	int ret = 0;
	// GPIO
	struct sigaction act;
	int i;

	sigemptyset(&act.sa_mask);
	act.sa_flags = (SA_SIGINFO | SA_RESTART);
	act.sa_sigaction = sig_event_handler;
	sigaction(CAP_SIG_ID, &act, NULL);

	printf("signal handler= %d, spi device=%s\n", CAP_SIG_ID,device);

	fd_capture = open("/dev/gpio_cap", O_RDWR);
	if(fd_capture < 0) {
		printf("Open device fail\n");
		return -1;
	}

	fd_spi = open(device, O_RDWR);
	if (fd_spi < 0)
		pabort("can't open device");
	/*
	 * spi mode
	 */
	ret = ioctl(fd_spi, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		pabort("can't set spi mode");
	ret = ioctl(fd_spi, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		pabort("can't get spi mode");
	/*
	 * bits per word
	 */
	ret = ioctl(fd_spi, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't set bits per word");
	ret = ioctl(fd_spi, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't get bits per word");
	/*
	 * max speed hz
	 */
	ret = ioctl(fd_spi, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't set max speed hz");
	ret = ioctl(fd_spi, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't get max speed hz");
	
	printf("spi mode: 0x%x\n", mode);
	printf("bits per word: %d\n", bits);
	printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);

	size = 160;
	tx = malloc(size);
	rx = malloc(size);
	for (i=0; i<size; i++)
	{
		tx[i] = 0;//i;
		//printf("%x ", tx[i] );
	}
	printf("Reset MI48 \n");
	system("./i2cset -f -y 1 0x40 0 1");
	printf("MI48 reset done\n");
	system("./i2cset -f -y 1 0x40 0xB4 0x03");
	system("./i2cset -f -y 1 0x40 0xD0 0x02");
	system("./i2cset -f -y 1 0x40 0xD0 0x03");

	system("./i2cset -f -y 1 0x40 0xB1 0x03");
	system("./i2cget -f -y 1 0x40 0xB2");
	system("./i2cget -f -y 1 0x40 0xB3");
	system("./i2cget -f -y 1 0x40 0xE0");
	system("./i2cget -f -y 1 0x40 0xE1");
	system("./i2cget -f -y 1 0x40 0xE2");
	system("./i2cget -f -y 1 0x40 0xE3");
	system("./i2cget -f -y 1 0x40 0xE4");
	system("./i2cget -f -y 1 0x40 0xE5");
	return ret;
}

static void mi48_header_parse(uint8_t *mi48_header_raw) 
{
	mi48_header.frame_cnt = (mi48_header_raw[0]<<8) | mi48_header_raw[1];
	mi48_header.max = (mi48_header_raw[10]<<8) | mi48_header_raw[11];
	mi48_header.min = (mi48_header_raw[12]<<8) | mi48_header_raw[13];
	logd(mi48_debug,"frame count = %d, max=%d, min=%d\n",mi48_header.frame_cnt,mi48_header.max,mi48_header.min);
/*
	switch (index) {
		case 0: // Frame count
		case 1:
			logd(sensor_debug,"Frame counter : %02X",buf[index]);
			logd(sensor_debug,"%02X\n",buf[index]);
			break;
		case 2:
		case 3:
			logd(sensor_debug,"SenXor VDD : %02X",buf[index]);
			logd(sensor_debug,"%02X\n",buf[index]);
			break;
		case 4:
		case 5:
			logd(sensor_debug,"Die Temperature : %02X",buf[index]);
			logd(sensor_debug,"%02X\n",buf[index]);
			break;
		default :
			break;
	}
*/
}

//int mi48_scan(char *data) {
int mi48_scan() {
	int spi_count = 0;
#ifndef NEW_THERMAL_SCAN
	int i;
	int count_break = 0;
	unsigned int index = 0;
	static uint16_t remove_front_count = 0;
#endif
	mi48_log_print();
	size = 160;
	if (state_change == 1) 
	{
		//led_send_msg(MSG_HEART_BIT);
		state_change = 0;
	#ifdef NEW_THERMAL_SCAN
		transfer(fd_spi,tx,rx,size);
		mi48_header_parse(rx);
		for (spi_count = 0; spi_count<62;spi_count++) {
			transfer(fd_spi,tx,rx,size);
			memcpy(&data[spi_count*160],rx,size);
		}
	#else
		for(spi_count = 0; spi_count<100; spi_count++)			//101 -> 80*62*2 / 99
		{
			transfer(fd_spi, tx, rx, size);
			// ----------------------Build RAW array----------------------			
			for (i = 0; i < size; i++)
			{
				if(remove_front_count >= 160){
					data[index] = rx[i]; 
					index = index +1 ;
				}
				else {
					mi48_header_raw[i]=rx[i];
					printf("index %d=%d\n",i,rx[i]);
				}
				count_break++;		// count for image
				remove_front_count++; 	// count for remove image header

			} // end spi_count 
			if ( count_break == 9920 + 160 )  // [80x62x2 = 9920(1pixel 2byte)]  [9920x2 (1pixel 2byte [SPI 16bits])]  [+160 because header ]
			{
				count_break = 0; 
				remove_front_count = 0;
				index = 0;
				mi48_header_parse();
				//temperature_alarm(thermeal_image_1B_raw,mi48_header.max,mi48_header.min);
				//ir8062_send_data(thermeal_image_1B_raw);
				//jpegenc(thermeal_image_1B_raw,max,min);
				break;
			}
		}
		#endif
		return 1; // success
	}
	else 
		return 0; // fail, spi not ready
}
unsigned int mi48_get_max_temperature() {
	return mi48_header.max;
}

unsigned int mi48_get_min_temperature() {
	return mi48_header.min;
}

uint8_t *mi48_get_data() {
	return data;
}
int mi48_close()
{
	free(rx);
	free(tx);
	close(fd_spi);
	close(fd_capture);
	return 0;	
}
int mi48_init()
{
	int ret=0;
	ret=ir8062_hwinit();
	if (ret < 0) 
		printf("ERROR : MI48 initial failure\n");
	return ret;
}

