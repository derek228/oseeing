.SUFFIXES : .x .o .c .s

CC := arm-linux-gcc
#STRIP := arm-linux-strip
TARGET = ir8062

#SRCS := ini-parser.c main.c
#OBJS= main.o cJSON.o curl_fun.o sensor.o cJSON.o
#OBJS= main.o cJSON.o sensor.o ini-parse.o cloud-service.o http-igreent.o rs485.o led_control.o jpegenc.o alarm.o
OBJS= main.o ini-parse.o rs485.o led_control.o jpegenc.o alarm.o ethernet.o mi48.o cloud-service.o cJSON.o curl_thread.o
LIBS = -lcurl -lm -ljpeg #-lssl -lcrypto #-ljson-c -lnsl 

$(TARGET) : $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS) $(CFLAGS) $(LIBS) 
clean:
	rm -r *.o

