#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>

#include "utils.h"

// Modbus RTU CRC 计算函数
uint16_t calculate_modbus_crc(uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    uint16_t pos,i;
    for (pos = 0; pos < length; pos++) {
        crc ^= data[pos];
        for (i = 0; i < 8; i++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// 检查 CRC 是否正确
int check_modbus_crc(uint8_t *received_data, uint16_t length) {
    // 获取数据的 CRC（最后两个字节）
    uint16_t received_crc = (received_data[length - 1] << 8) | received_data[length - 2];
    
    // 计算数据的 CRC
    uint16_t calculated_crc = calculate_modbus_crc(received_data, length - 2);
    printf("recv CRC = 0x%x, cal CRC = 0x%x\n",received_crc, calculated_crc);
    // 比较接收到的 CRC 和计算的 CRC 是否一致
    if (calculated_crc == received_crc) {
        return 1; // CRC 正确
    } else {
        return 0; // CRC 错误
    }
}

int oseeing_pidkill(char *program_name) {
	//char *program_name = "ir8062";
	int pid = -1;
	DIR *dir;
	struct dirent *ent;
	char buf[512];

	if ((dir = opendir("/proc")) != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			if (isdigit(*ent->d_name)) {
				sprintf(buf, "/proc/%s/cmdline", ent->d_name);
				FILE *fp = fopen(buf, "r");
				if (fp != NULL) {
					fgets(buf, sizeof(buf), fp);
					fclose(fp);
					if (strstr(buf, program_name) != NULL) {
						pid = atoi(ent->d_name); 
						break;
					}
				}
			}
        	}
        	closedir(dir);
	}

	if (pid != -1) {
		printf("PID of process '%s': %d\n", program_name, pid);
	} else {
		printf("Process '%s' not found\n", program_name);
		return -1;
	}
	int result = kill(pid, SIGKILL);
	if (result == 0) {
		printf("Process with PID %d killed successfully.\n", pid);
	} else {
		perror("Error killing process");
		return -1;
	}

	return 0;
}

int file_exist(char *fname) {
    if (access(fname, F_OK) < 0) {
        //printf("ERROR(%s) : File %s is not exist\n",__FUNCTION__,fname);
        return -1;
    }
    else   
        return 1;
}


int write_char_to_file(char *fname, char *data, int len) {
    int ret = 0;
    size_t written;
    FILE *file = fopen(fname, "wb");
    if (file == NULL) {
        //printf("ERROR(%s) : Can't open file %s\n",__FUNCTION__,fname);
        return -1;
    }
	written = fwrite(data, 1, len, file);
	if (written == len) {
        //printf("%s : Write %d bytes data to %s file\n",__FUNCTION__, len, fname);
        ret = 1;
    }
    else
    {
		//printf("ERROR(%s) : Faile to write %d bytes data to %s file\n",__FUNCTION__, len, fname);
		ret = -1;
	}
    fclose(file);
    return ret;
}

int write_shortint_to_file(char *fname, short int *data, int len) {
    int ret = 0,i;
    size_t written;
    FILE *file = fopen(fname, "wb");
    if (file == NULL) {
        //printf("ERROR(%s) : Can't open file %s\n",__FUNCTION__,fname);
        return -1;
    }
	//printf("%s : Write %d bytes to %s file\n",__FUNCTION__, len, fname);
	written = fwrite(data, sizeof(short int), len, file);
	if (written == len) {
        //printf("%s : Write 0x%x(%d bytes) to %s file\n",__FUNCTION__, *data, len, fname);
        //for (i=0; i<written; i++) 
            //printf("Write[%d]=0x%x\n",i,data[i]);
        ret = 1;
    }
    else
    {
		printf("ERROR(%s) : Faile to write 0x%x data to %s file\n",__FUNCTION__, *data, fname);
		ret = -1;
	}
    fclose(file);
    return ret;
}

int read_char_from_file(char *fname, char *data) {
    FILE *file = NULL;
    size_t readlen=0;
    int ret = 0;
	long filesize;
    if (file_exist(fname) < 0) 
        return -1;
    file = fopen(fname, "rb");
    if (file == NULL) {
        //printf("ERROR(%s) : Can't open file %s\n",__FUNCTION__,fname);
        return -1;
    }
	fseek(file,0,SEEK_END);
	filesize=ftell(file);
	rewind(file);
	readlen = fread(data, 1, filesize, file);
	if (readlen == 1) {
        //printf("%s : Read %s(%ld bytes) file data = 0x%x\n",__FUNCTION__,fname,filesize, *data);
        ret = readlen;
	}
    else if (readlen > 1) {
        //printf("%s : Read %s(%ld bytes) file data = %s\n",__FUNCTION__,fname, filesize, data);
        ret = readlen;
    }
    else {
        printf("ERROR(%s) : Can't Read %s(%ld bytes) file data\n",__FUNCTION__,fname, filesize);
        ret = -1;
    }
	fclose(file);
	return ret;
}

int read_shortint_from_file(char *fname, short int *data) {
    FILE *file = NULL;
    size_t readlen=0;
	long filesize;
    int ret = 0;
    if (file_exist(fname) < 0) 
        return -1;
    file = fopen(fname, "rb");
    if (file == NULL) {
        //printf("ERROR(%s) : Can't open file %s\n",__FUNCTION__,fname);
        return -1;
    }
	fseek(file,0,SEEK_END);
	filesize=ftell(file);
	rewind(file);
	readlen = fread(data, sizeof(short int), filesize/sizeof(short int), file);
	if (readlen == (filesize/sizeof(short int)) ) {
        //printf("%s : Read %s(%ld bytes) file data = 0x%x\n",__FUNCTION__,fname,filesize, *data);
        ret = 1;
	}
    else {
        //printf("ERROR(%s) : Can't Read %s file data\n",__FUNCTION__,fname);
        ret = -1;
    }
	fclose(file);
	return ret;
}

void set_mi48_reg(uint8_t reg, uint8_t val) 
{
    char *cmd;
    sprintf(cmd,"/i2cset -f -y 1 0x40 0x%x 0x%x", reg, val);
    printf("system call : %s\n",cmd);
}
