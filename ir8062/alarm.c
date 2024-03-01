
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "ini-parse.h"
#include "sensor.h"
#include "rs485.h"
#include "../leds/pwm_message.h"
#include "led_control.h"
#include "jpegenc.h"
#include "alarm.h"

char RS485_DO1_OFF[8]={0x01,0x05,0x00,0x00,0x00,0x00,0xCD,0xCA};
char RS485_DO1_ON[8]={0x01,0x05,0x00,0x00,0xFF,0x00,0x8C,0x3A};
char RS485_DO2_OFF[8]={0x01,0x05,0x00,0x01,0x00,0x00,0x9C,0x0A};
char RS485_DO2_ON[8]={0x01,0x05,0x00,0x01,0xFF,0x00,0xDD,0xFA};

static int alarm_debug=0;

static void alarm_log_print() {
    const char *filename = "/mnt/mtdblock1/alarm";

    // 检查文件是否存在
    if (access(filename, F_OK) != -1) {
        alarm_debug=1;
    } else {
        alarm_debug=0;
    }
}


static void set_alarm_led(int level) {
  static int lv1=0;
  static int lv2=0;
  
  if ((level == 2) && (lv2==0) ){
    logd(alarm_debug,"RED Blanking : level=%d, lv1=%d, lv2=%d\n",level, lv1,lv2);
    led_send_msg(MSG_LEDW_DISABLE);
    led_send_msg(MSG_LEDY_DISABLE);
    led_send_msg(MSG_LEDR_BLANKING);
    lv2=1;
    lv1=0;
  }
  else if ((level == 1) && (lv1==0) ){
    logd(alarm_debug,"YELLOW Blanking : level=%d, lv1=%d, lv2=%d\n",level, lv1,lv2);
    led_send_msg(MSG_LEDR_DISABLE);
    led_send_msg(MSG_LEDY_BLANKING);
    led_send_msg(MSG_LEDW_DISABLE);
    lv2=0;
    lv1=1;
  }
  else if (level == 0) { // no alarm
    if (lv1) {
      logd(alarm_debug,"YELLOW OFF : level=%d, lv1=%d, lv2=%d\n",level, lv1,lv2);
      led_send_msg(MSG_LEDY_DISABLE);
      lv1=0;
      if (lv2==0)
        led_send_msg(MSG_LEDW_BREATHING);
    }
    if (lv2) {
      logd(alarm_debug,"RED OFF : level=%d, lv1=%d, lv2=%d\n",level, lv1,lv2);
      led_send_msg(MSG_LEDR_DISABLE);
      lv2=0;
      if (lv1==0)
        led_send_msg(MSG_LEDW_BREATHING);
    }
  }
}

static int rs485_ack_pass(char *cmd) {
  int len=0;
  int retry=10;
  char rx[8]={0};
  while(retry) {
    len = rs485_read(rx,8);
    if ( (memcmp(rx,cmd,8)!=0) ) {
      retry--;
      usleep(10000);
    }
    else break;
  }
  if (retry == 0) {
	printf("ERROR, RS485 ACK failure\n");
    return 1; //fail
  }
  else {
    printf("Get RS485 Command ACK, retry =%d\n",retry);
    return 0; //pass
  }
}

static void set_alarm_dio(int level) {
  static int lv1=0;
  static int lv2=0;
  logd(alarm_debug,"level=%d, lv1=%d, lv2=%d\n",level, lv1,lv2);
  if ((level == 2) && ((lv2==0) || (lv1==0)) ){
    logd(alarm_debug, "DO1/2 On : level=%d, lv1=%d, lv2=%d\n",level, lv1,lv2);
    // send do1
    if (lv1 == 0) {
      rs485_send(RS485_DO1_ON,8);
      if (rs485_ack_pass(RS485_DO1_ON)) 
      {
        printf("ERROR DO 1 off command fail\n");
      }
      else 
        lv1 = 1 ; 
    }
    if (lv2==0) {
      rs485_send(RS485_DO2_ON,8);
      if (rs485_ack_pass(RS485_DO2_ON)) 
      {
        printf("ERROR DO 2 on command fail\n");
      }
      else 
        lv2 = 1;
    }
  }
  else if ((level == 1) && (lv1==0) ){
    logd(alarm_debug,"DO1 On/DO2 Off : level=%d, lv1=%d, lv2=%d\n",level, lv1,lv2);
    if (lv2==1) {
      rs485_send(RS485_DO2_OFF,8);
      if (rs485_ack_pass(RS485_DO2_OFF)) 
      {
        printf("ERROR DO 2 off command fail\n");
      }
      else {
        lv2=0;
        printf("DO2 Off : level=%d, lv1=%d, lv2=%d\n",level, lv1,lv2);
      }
    }
    rs485_send(RS485_DO1_ON,8);
    if (rs485_ack_pass(RS485_DO1_ON)) 
    {
      printf("ERROR DO 1 on command fail\n");
    }
    else {
      lv1=1;
      logd(alarm_debug,"DO1 On : level=%d, lv1=%d, lv2=%d\n",level, lv1,lv2);
    }
  }
  else if (level == 0) { // no alarm
    logd(alarm_debug,"DO1/2 Off : level=%d, lv1=%d, lv2=%d\n",level, lv1,lv2);
    if (lv1) {
      rs485_send(RS485_DO1_OFF,8);
      if (rs485_ack_pass(RS485_DO1_OFF)) 
      {
        printf("ERROR DO 1 off command fail\n");
      }
      else {
        lv1=0;
        logd(alarm_debug,"DO1 Off : level=%d, lv1=%d, lv2=%d\n",level, lv1,lv2);
      }
    }
    if (lv2) {
      rs485_send(RS485_DO2_OFF,8);
      if (rs485_ack_pass(RS485_DO2_OFF)) 
      {
        printf("ERROR DO 2 off command fail\n");
      }
      else {
        lv2=0;
        logd(alarm_debug,"DO2 Off : level=%d, lv1=%d, lv2=%d\n",level, lv1,lv2);
      }
    }
  }
  logd(alarm_debug,"Exit : level=%d, lv1=%d, lv2=%d\n",level, lv1,lv2);
}

int temperature_alarm(char *data, unsigned int max, unsigned int min) {
  char rs485_test_cmd[32]={0};//"Normal Temperature";
	alarm_log_print();
  logd(alarm_debug, "max/min= %d/%d ,level 1/2= %d/%d \n",(max-2735)/10, (min-2735)/10, get_eth_over_temperature(),get_eth_alert_temerature());
  if ( max > get_eth_alert_temerature() ) { // alarm level2
    set_alarm_dio(2);
    set_alarm_led(2);
  jpegenc(data, max, min); // save alarm image 
  }
  else if (max > get_eth_over_temperature() ) { // alarm level 1
    set_alarm_dio(1);
    set_alarm_led(1);
  jpegenc(data, max, min); // save alarm image 
  }
  else {  // normal temperature
    set_alarm_dio(0);
    set_alarm_led(0);
    return 0;
  }
  return 0;
}

