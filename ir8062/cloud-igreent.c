#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/types.h>
#include <net/if.h>
#include <stdint.h>
#include <curl/curl.h>
#include "cloud-service.h"
#include "ini-parse.h"
#include "rs485.h"
#include "cJSON.h"
#include "ethernet.h"
#include "curl_thread.h"
#include "mi48.h"

enum {
	IGREENT_REGISTERING = 0,
	IGREENT_REGISTER_SUCCESS,
	IGREENT_REGISTER_FAIL
};

#define SENSOR_ID_LEN	20
#define THERMAL_STRING_LEN  29760  // 80x62x2x3 ( 3 char per 1 byte)
#define POST_SENSOR_DATA_URL "http://demo.igreent.com/php/insert_ir_arrary_post.php"
#define POST_SENSOR_0101_DATA_URL "http://demo.igreent.com/php/insert_ir_arrary_post_0101.php"
#define THERMAL_STRING_LEN  29760  // 80x62x2x3 ( 3 char per 1 byte)

char thermal_data_string[THERMAL_STRING_LEN]={0};
int igreent_debug=0;
static char SensorID[SENSOR_ID_LEN]={0};
static char igreent_ir_gateway_url_prefix[128]="http://demo.igreent.com/php/exec_q_ir_gateway.php?macno=";

struct memory {
  char *response;
  size_t size;
};
struct memory chunk = {0};
uint8_t register_status=IGREENT_REGISTERING;
uint8_t curl_running = 0;
pthread_t igreent_register_tid;

static int cJSONParserID(const char * const response) {
	cJSON *res = NULL;
	cJSON *sensor_id = NULL;
	char *out;    
	long unsigned int len=0;
	int status = 0;
	out=strchr(response,0x7b); // search json string start address of char "{" 
	printf("JSON res = %s\n",out);
	res = cJSON_Parse(out);
	if (res == NULL)
	{
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL)
		{
			fprintf(stderr, "Error before: %s\n", error_ptr);
		}
		status = 0;
		return -1;
	}
	sensor_id = cJSON_GetObjectItem(res,"sensor_id");
	if ((sensor_id == NULL))
	{
		printf("ERROR : can't find JSON key of sensor_id\n");
		return -1;
	}
	else if (cJSON_IsString(sensor_id)) {
		len = strlen(sensor_id->valuestring);
		printf("Get JSON key of sensor_id, value=%s, len=%ld\n",sensor_id->valuestring, len);
		memset(SensorID,0, sizeof(SensorID)) ;
		memcpy(SensorID, sensor_id->valuestring,  len);
		printf("SensorID is string=%s, len=%ld\n",SensorID,strlen(SensorID));
	}
	else {
		printf("ERROR : return value not string format of JSON key sensor_id\n");
		return -1;
	}
	//cJSON_Delete(sensor_id);
	cJSON_Delete(res);
	return 0;
}

static size_t exec_q_ir_gateway_cb(void *data, size_t size, size_t nmemb, void *clientp)
{
	size_t realsize = size * nmemb;
	struct memory *mem = (struct memory *)clientp;
 	char *ptr = realloc(mem->response, mem->size + realsize + 1);
	if(ptr == NULL)
		return 0;  /* out of memory! */
	mem->response = ptr;
	memcpy(&(mem->response[mem->size]), data, realsize);
	mem->size += realsize;
	mem->response[mem->size] = 0;
	printf("response:\n%s\nEnd\n",mem->response);
	if (cJSONParserID(mem->response)<0) {
		printf("ERROR : Can't register iGreent Cloud service\n");
		register_status=IGREENT_REGISTER_FAIL;
	}
	else
		register_status=IGREENT_REGISTER_SUCCESS;
	return realsize;
}



// Post MAC address to cloud service, if device register successful, return sensor ID.
static int http_exec_q_ir_gateway() { 
	CURL *curl;
	CURLcode res;
/*	if (is_connected("http://demo.igreent.com") < 0) {
		register_status=IGREENT_REGISTER_FAIL;
		printf("No internet service, can't connect to iGreent cloud\n");
		return -1;
	}*/
	curl = curl_easy_init();
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, igreent_ir_gateway_url_prefix);
		/* send all data to this function  */
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, exec_q_ir_gateway_cb);
		curl_easy_setopt(curl,CURLOPT_TIMEOUT, 10L);
		/* we pass our 'chunk' struct to the callback function */
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
 
		/* Perform the request, res will get the return code */
    	res = curl_easy_perform(curl);
    	/* Check for errors */
    	if(res != CURLE_OK) {
			register_status=IGREENT_REGISTER_FAIL;
      		printf("curl_easy_perform() iGreent register failed: %s\n",curl_easy_strerror(res));
      		return -1;
    	}
		else
			register_status=IGREENT_REGISTER_SUCCESS;
    	/* remember to free the buffer */
		free(chunk.response);
		/* always cleanup */
		curl_easy_cleanup(curl);
		return 0;
	}
}

static void convert_raw_to_string()
{
  int i;
  uint8_t *sensor_data=mi48_get_data();
  memset(thermal_data_string,'\0',strlen(sensor_data));
  sprintf(thermal_data_string , "%02X", sensor_data);
  for (i=1; i < 9920; i++) {
    if ( (i*3)>= THERMAL_STRING_LEN ) {
      //printf("ERROR : over buffer length (%d)\n",i*3);
      break; // over buffer length
    }
    //printf("%d : ,%02X\n",i, sensor_data[i]);
    sprintf(thermal_data_string + (i * 3)-1, ",%02X", sensor_data[i]);
  }
}

static int full_data_post(){
	cJSON *cjson_ir_array = NULL;
	cJSON *cjson_sensor_id = NULL;
	cJSON *cjson_modbus_cmd = NULL;
	cJSON *cjson_ir_value = NULL;
	char *out;
	convert_raw_to_string();
	cjson_ir_array = cJSON_CreateObject();
	cJSON_AddStringToObject(cjson_ir_array, "sensor_id", SensorID);
	cJSON_AddStringToObject(cjson_ir_array, "modbus_cmd","IG8062");
	cJSON_AddStringToObject(cjson_ir_array, "ir_value",thermal_data_string);
	out = cJSON_Print(cjson_ir_array);
	logd(igreent_debug, "%s\n",out);
	//printf("Create ir_array_post data\n %s \n len=%ld   END\n",out,strlen(out));
	
	curl_send_msg(POST_SENSOR_DATA_URL, out);
	printf("free out string %d\n", strlen(out));
	cJSON_Delete(cjson_ir_array);
	free(out);
}
static int simple_data_post(char *data) {
	cJSON *cjson_ir_array = NULL;
	cJSON *cjson_sensor_id = NULL;
	cJSON *cjson_modbus_cmd = NULL;
	cJSON *cjson_ir_value = NULL;
	char *out;
	char max_temp_str[5]={0};
	//printf("simple data post\n");
	if (mi48_get_max_temperature() < get_ini_over_alarm1()) 
	  return 0;
	sprintf(max_temp_str, "%02X,%02X", (mi48_get_max_temperature()&0xff00)>>8,(mi48_get_max_temperature()&0x00ff));
	logd(igreent_debug,"MAX Temperature = %s\n", max_temp_str);
	cjson_ir_array = cJSON_CreateObject();
	cJSON_AddStringToObject(cjson_ir_array, "sensor_id", SensorID);
	cJSON_AddStringToObject(cjson_ir_array, "modbus_cmd","IG8062_01X01");
	cJSON_AddStringToObject(cjson_ir_array, "ir_value",max_temp_str);
	out = cJSON_Print(cjson_ir_array);
	logd(igreent_debug,"Create ir_array_post data\n %s \n len=%ld   END\n",out,strlen(out));
	curl_send_msg(POST_SENSOR_0101_DATA_URL, out);
	free(out);
	return 0;
}

static int igreent_register_thread() {
	size_t len;
    while (register_status != IGREENT_REGISTER_SUCCESS) {
		printf("Waitting igreent cloud registering...\n");
        if (is_connected() != 1) {
			printf("Connect failure...\n");
			sleep(3);
			continue;
		}
    	strcat(igreent_ir_gateway_url_prefix, eth_get_mac());
    	len=strlen(igreent_ir_gateway_url_prefix);
    	igreent_ir_gateway_url_prefix[len]='\0';
    	printf("URL = %s len=%d\n", igreent_ir_gateway_url_prefix,len);
    	if (http_exec_q_ir_gateway()<0) {
            printf("IGREENT cloud service register failure...\n");
            register_status = IGREENT_REGISTER_FAIL;
        }
		else {
            printf("IGREENT cloud service register success !!!\n");
            register_status = IGREENT_REGISTER_SUCCESS;
        }
        sleep(1);
    }
}

static int igreent_service_init() {
    if (pthread_create(&igreent_register_tid, NULL, igreent_register_thread, NULL) != 0) {
        perror("igreent service pthread_create\n");
        return -1;
    }
	/*
    if (pthread_join(igreent_register_thread, NULL) != 0) {
        perror("igreent service join failed...\n");
        return -1;
    }
	*/
    return 0;

}

static int is_cloud_service_ready() {
    if (register_status != IGREENT_REGISTER_SUCCESS)
        return -1;
    if (curl_running == 0) 
        return -1;
    return 0;
}

int cloud_service_init()
{
	//int ret;
	printf("Cloud service initial\n");
    if (igreent_service_init() < 0) {
        printf("ERROR : Create igreent_service_init failure...\n");
        return -1;
    }
    if (curl_thread_create() < 0) {
        printf("ERROR : curl therad create failed\n");
        return -1;
    }
    else 
        curl_running = 1;
	return 0;	
}

void run_cloud_service() {
	if (is_cloud_service_ready() < 0) {
		printf("ERROR : Cloud service not ready...\n");
		return;
	}
	switch (get_ini_post_type()) {
	case POST_FULL_DATA:
		full_data_post();
		break;
	case POST_SIMPLE_DATA:
		simple_data_post(NULL);
		break;
	default:
		printf("ERROR : Data post format not defined\n");
		break;
	}
	//printf("Igreent cloud service running\n");
}
