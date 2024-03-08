#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
//#include <sys/socket.h>
//#include <net/if.h>
#include <stdint.h>
//#include <curl/curl.h>
#include "ini-parse.h"
#include "spidev.h"
#include "cloud-service.h"
#include "ini-parse.h"
#include "../leds/pwm_message.h"
#include "led_control.h"
#include "jpegenc.h"
#include "alarm.h"
#include "ethernet.h"
#include "mi48.h"

#define FW_VERSION	"1.00.04"
static void parse_opts(int argc, char *argv[])
{
	char *macno;
	int i=0;
	while (1) {
		int c;
		c = getopt(argc, argv, "m:");
		if (c == -1) {
			break;
		}
		switch (c) {
			case 'm':
				eth_set_fake_mac(optarg);
				printf("Set fack MAC address = %s, len=%d\n",optarg,strlen(optarg));
				break;
			default:
				printf("Unknow args, use original MAC NO=%s\n",optarg);
				break;
		}
	}
}
static int factory_test() {
	// To Do : Check factory gpio, if not factory test mode, return 0
	return 0; 
	while (1) {
		printf("Start factory mode\n");
		sleep(1);
	}
}
static void monitor_temperature() {
	// INI file configuartion parse.
	while (parse_ini_file(INI_FILENAME)<0) {
		printf("ERROR : Can't open %s file\n",INI_FILENAME);
		sleep(1);
	}
	// Open rs485 device
	while (rs485_init()<0) {
		printf("ERROR : Can't open RS485 port\n");
		sleep(1);
	}
	// Check dhcp if support ethernet
	eth_mac_config();
	while (ethernet_init() < 0) {
		printf("ERROR : DHCP IP address not found\n");
		sleep(1);
	}
	if (get_ini_conn_type() == RJ45) {
		cloud_service_init();
	}
	// start system LEDs control
	led_msg_init();
	printf("Post MAC address = %s\n", eth_get_mac());
	// MI48 thermal sensor initial
	mi48_init();
	while (1) {
		if (mi48_scan()) {
			led_heartbit();
			jpegenc(mi48_get_data(),mi48_get_max_temperature(), mi48_get_min_temperature());
			temperature_alarm(mi48_get_data(),mi48_get_max_temperature(), mi48_get_min_temperature() );
			run_cloud_service();
		}
	}
}
int main(int argc, char *argv[]) {
	//pthread_t tid_sensor;
	// Show Oseeing firmware version
	printf("FW Version : %s\n", FW_VERSION);
	parse_opts(argc, argv);
	if (factory_test()) {
		return 0;
	}
	else {
		monitor_temperature();
	}
}

