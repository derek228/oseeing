#ifndef __MI48_H__
#define __MI48_H__

typedef struct {
	unsigned int frame_cnt;
	unsigned int max;
	unsigned int min;
}mi48_header_t;

//int mi48_scan(char *data);
int mi48_scan();
uint8_t *mi48_get_data();
int mi48_init();
int mi48_close();
unsigned int mi48_get_max_temperature();
unsigned int mi48_get_min_temperature();
#endif
