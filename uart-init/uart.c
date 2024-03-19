#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <linux/reboot.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>

#define PORT 8003 // 8080
#define IMG_BUFFER_SIZE  1024 // 102400 // 1048576 // 1MB
#define UPDATE_IMG "/image"
#define IMG_UPDATE_CMD "IR8062UPDATE"
#define IMG_DOWNLOAD_FINISH "IR8062IMGDOWNLOADED"
#define UPDATING_ACK "IR8062SYSTEMUPDATING\n"
#define REBOOT_ACK "IR8062SYSTEMREBOOT\n"
#define FILENAME "/mnt/mtdblock1/ir8062.ini"
#define BUFFER_SIZE 1024
#define END_MARKER "END_OF_FILE" // 自定義的結束標記
#define START_MARKER "IR8062"
#define SERIALDEV "/dev/ttyS4"
#define ETHERNET_DEVICE_NAME "eth0"

char device_macno[18]={0};
char ipaddr[16]={0};
#define CMD_START "IR8062_CONNECTED\n"
enum {
	CONNECTION_INIT = 0,
	BROADCASTING,
	SYSTEM_CONFIG_START,
	SYSTEM_CONFIG_FINISH,
	SYSTEM_UPDATE_START,
	SYSTEM_UPDATE_FINISH,
	RESTART,
};
int serial_port;


int get_ip() {
	int sockfd;
	struct ifreq ifr;
	struct sockaddr_in *sin;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Socket creation error");
		return -1;
	}
	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, ETHERNET_DEVICE_NAME, IFNAMSIZ - 1);
	if (ioctl(sockfd, SIOCGIFADDR, &ifr) < 0) {
		perror("IOCTL error");
		close(sockfd);
		return -1;
	}

	sin = (struct sockaddr_in *)&ifr.ifr_addr;
	sprintf(ipaddr,"%s\n",inet_ntoa(sin->sin_addr));
	//printf("IP Address: %s\n", ipaddr);// inet_ntoa(sin->sin_addr));
	close(sockfd);
	return 0;
}

void get_macno() {
	int fd;
	struct ifreq ifr;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	strncpy(ifr.ifr_name, ETHERNET_DEVICE_NAME, IFNAMSIZ);
	if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
		perror("ioctl");
		exit(1);
	}

	sprintf(device_macno,"%02x:%02x:%02x:%02x:%02x:%02x",
            (unsigned char)ifr.ifr_hwaddr.sa_data[0],
            (unsigned char)ifr.ifr_hwaddr.sa_data[1],
            (unsigned char)ifr.ifr_hwaddr.sa_data[2],
            (unsigned char)ifr.ifr_hwaddr.sa_data[3],
            (unsigned char)ifr.ifr_hwaddr.sa_data[4],
            (unsigned char)ifr.ifr_hwaddr.sa_data[5]);

	close(fd);
	//printf("Onboard MAC Address : %s\n", device_macno);
}

static int serial_init(int blocking)
{
	struct termios tty;
	if (blocking) {
		serial_port = open(SERIALDEV, O_RDWR | O_NOCTTY | O_NONBLOCK); // 以读写方式打开串口设备
	}
	else 
		serial_port = open(SERIALDEV, O_RDWR);
	if (serial_port < 0) {
		perror("Error opening serial port");
		return 1;
	}

	memset(&tty, 0, sizeof(tty));
	if (tcgetattr(serial_port, &tty) != 0) {
		perror("Error getting serial port attributes");
		close(serial_port);
		return 1;
	}

	tty.c_cflag &= ~CSTOPB; // 1 stop bit
	tty.c_cflag |= CS8; // 8 bit
	tty.c_cflag |= CREAD | CLOCAL; 
	cfsetospeed(&tty, B115200); // baudrate 115200
	tty.c_cc[VMIN] = 0; // non-blocking
	tty.c_cc[VTIME] = 0; // non-blocking
	tcsetattr(serial_port, TCSANOW, &tty);
	return 0;
}

static void sendcmd(char *cmd, int newline){
	if (newline)
		sprintf(cmd,"%s\n",cmd);
	write(serial_port,cmd,strlen(cmd));
}

static int readdev(char *rx) {
	FILE *log;
	ssize_t rx_size=0;
	rx_size = read(serial_port, rx, BUFFER_SIZE);
	if (rx_size>0) {
		log = fopen("/log", "ab"); 
		fputs(rx,log);
		fclose(log);
		printf("ACK rx=%s\n",rx);
		return 1;
	}
//	else 
//		usleep(200000);
	return 0;
}
static int readcmd(char *rx) {
	ssize_t rx_size=0;
	rx_size = read(serial_port, rx,BUFFER_SIZE);
	return rx_size;
}
#define UPDATE_FAIL_ACK "IR8062UPDATEFAILED\n"
static int upgrade_fail() {
	printf("Firmware upgrade failed...\n");
	sleep(1);
	sendcmd(UPDATE_FAIL_ACK,0);
	sleep(1);
	sendcmd(UPDATE_FAIL_ACK,0);
	sleep(1);
	sendcmd(UPDATE_FAIL_ACK,0);
	system("reboot -f");
}
static int upgrading() {
	printf("Start Firmware upgrade...\n");
	sleep(1);
	sendcmd(UPDATING_ACK,0);
	sleep(1);
	sendcmd(UPDATING_ACK,0);
	sleep(1);
	sendcmd(UPDATING_ACK,0);
}
static int upgrade_finish() {
	printf("Firmware upgrade finish...\n");
	sleep(1);
	sendcmd(REBOOT_ACK,0);
	sleep(1);
	sendcmd(REBOOT_ACK,0);
	sleep(1);
	sendcmd(REBOOT_ACK,0);
	system("reboot -f");
}
#define RECV_TIMEOUT_SEC 30
static int download() {
	int server_fd, new_socket, valread;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	char buffer[IMG_BUFFER_SIZE] = {0};
	char rx[BUFFER_SIZE]={0};
	int cnt=0;


	struct timeval timeout;
	timeout.tv_sec = RECV_TIMEOUT_SEC;
	timeout.tv_usec = 0;
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		perror("socket failed");
		return -1;
	}
#if 1 // add timeout 
	if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
		perror("setsockopt");
		return 1;
	}
#else
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
		perror("setsockopt");
		return -1;
	}
#endif
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);
	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind failed");
		return -1;
	}
	if (listen(server_fd, 3) < 0) {
		perror("listen");
		return -1;
	}
	if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
		perror("accept");
		return -1;
	}
	FILE *received_file = fopen(UPDATE_IMG, "wb");
	if (received_file == NULL) {
		perror("Unable to open file.");
		return -1;
	}

	while (1) {
		valread = recv(new_socket, buffer, IMG_BUFFER_SIZE, 0);
        	if (valread == 0) {
			printf("Host disconnect socket ...\n");
			break;
		} else if (valread == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				printf("recv : timeout, try again\n");
				continue;			
			}
			else
				perror("recv");
			break;
		} else {
			fwrite(buffer, 1, valread, received_file);
			//printf("write file\n");
		}
	}
	printf("CLOSE socket\n");
	close(new_socket);
	close(server_fd);
//    while ((valread = read(new_socket, buffer, IMG_BUFFER_SIZE)) > 0) {
//        fwrite(buffer, 1, valread, received_file);
	//printf("write file\n");
//    }
	
	if (valread <0) {
		printf("IMAGE download fail\n");
		fclose(received_file);
		return -1;
	}
	// clear uart rx buffer
	if (tcflush(serial_port, TCIFLUSH) == -1) {
		perror("tcflush");
		return -1;
	}
	memset(rx,0,BUFFER_SIZE);
	while(1) {
		if (readcmd(rx) <= 0) {
			printf("Wait for download finish command...\n");
			usleep(100000);
			continue;
		}
		else 
			printf("rx cmd = %s\n",rx);
		if (strstr(rx,IMG_DOWNLOAD_FINISH) != NULL)
			break;
	}
	fclose(received_file);
	printf("File received successfully.\n");

	return 0;
}

static int ir8062_pidkill() {
	char *program_name = "ir8062";
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


int main() {
	FILE *file;
	char buffer[BUFFER_SIZE];
	char rx[BUFFER_SIZE];
	static int cnt=0,retry=0,err=0;
	ssize_t rx_size=0;
	static int state=CONNECTION_INIT;

	get_macno();
	get_ip();

	while (1) { // send mac
		get_ip();
		switch (state) {
		case CONNECTION_INIT:
			err = serial_init(1);
			state=BROADCASTING;
			break;
		case BROADCASTING:
			sprintf(buffer,"IR8062_BROADCASTINGMAC%sIP%s\n",device_macno,ipaddr);
			sendcmd(buffer,0);
			if ((cnt++ %10)==0)
				printf("Broadcasting : %s",buffer);
			usleep(100000);
			if (readdev(rx)) {
				if (strstr(rx,device_macno)!=NULL) {
					printf("SYSTEM CONFIGURATION \n");
					state=SYSTEM_CONFIG_START;
					cnt = 0;
				}
				else if (strstr(rx,"IR8062UPDATE")!=NULL) {
					printf("SYSTEM IMAGE UPDATE\n");
					state = SYSTEM_UPDATE_START;
					cnt = 0;
				}
				else {
					printf("ACK ERROR : %s\n",rx);
				}
			}
			sleep(1);
			break;
		case SYSTEM_CONFIG_START:
			file = fopen(FILENAME, "wb+"); 
			usleep(1);
			sendcmd(CMD_START,0);
			printf("Start system config\n");
			state=SYSTEM_CONFIG_FINISH;
			break;			
		case SYSTEM_CONFIG_FINISH:
			while (1) {
				memset(rx,0,BUFFER_SIZE);
				if (readdev(rx)) {
					if (strstr(rx,END_MARKER) == NULL) {
						fputs(rx,file);
						printf("Write INI: %s",rx);
					}
					else
						break;
				}
			}
			fclose(file);
			printf("Systam ini file updated\n");
			state = RESTART;
			sleep(1);
			break;

		case SYSTEM_UPDATE_START:
			usleep(100000);
			sendcmd(CMD_START,0);
			ir8062_pidkill();
			sleep(1);
			printf("Download image\n");
/*
			while (download()<0) {
				sleep(1);
				printf("IMAGE download failed\n");
			}
*/
			if (download()<0) {
				//while (1) {
					//printf("Firmware download fail, restart system and try again\n");
					//sleep(1);
				//}
				state = RESTART;
				upgrade_fail(); // send update fail ack then reboot system
			}
			else
				state=SYSTEM_UPDATE_FINISH;
			break;

		case SYSTEM_UPDATE_FINISH:
			upgrading();
			printf("Start upgrade process\n");
			system("/fwupdate -p /image -w kernel");
			printf("Update system finished, rebooting....\n");
			state = RESTART;
			upgrade_finish();
			break;

		case RESTART:
			printf("Uart tool restart\n");
			close(serial_port);
			sleep(1);
			state=CONNECTION_INIT;
			break;
		default:
			printf("Unknow state %d\n",state);
			break;
		}
		if (err)
			break;
	}
    close(serial_port);
	printf("EXIT uart\n");

    return 0;
}


