#ifndef __UTILS_H__
#define __UTILS_H__


/*
Celsius2K:
K = 10*Celsius+2735
Fahrenheit2K
K = (50*Fahrenheit + 22985)/9

KToCelsius:
Celsius = (K-2735) / 10

*/
#if 0
#define C2K(c) {10*c+2735} // Celsius2K : K = 10*Celsius+2735
#define F2K(f) {(50*f+22985)/9} // Fahrenheit2K : K = (50*Fahrenheit + 22985)/9
#define K2C(k) {(k-2735)/10} //KToCelsius:Celsius = (K-2735) / 10
#define K2F(k) {((k*9/5)-4597)/10 }//K2Fahrenheit = ((K*9/5)-4597)/10
#else
#define C2K(c) (c+2735) // Celsius2K : K = 10*Celsius+2735
#define F2K(f) ((5*f+22983)/9) // Fahrenheit2K : K = (50*Fahrenheit + 22985)/9
#define K2C(k) (k-2735) //KToCelsius:Celsius = (K-2735) / 10
#define K2F(k) ((k*9/5)-4597) //K2Fahrenheit = ((K*9/5)-4597)/10
#endif

int oseeing_pidkill(char *program_name);
uint16_t calculate_modbus_crc(unsigned char *data, unsigned short int length);
int check_modbus_crc(uint8_t *received_data, uint16_t length);
int file_exist(char *fname);
int write_char_to_file(char *fname, char *data, int len);
int write_shortint_to_file(char *fname, short int *data, int len);
int read_char_from_file(char *fname, char *data);
int read_shortint_from_file(char *fname, short int *data);
void set_mi48_reg(uint8_t reg, uint8_t val);

#endif //  __UTILS_H__