#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mqueue.h>
#include <curl/curl.h>
#include <fcntl.h>

#include "cJSON.h"
#include "cloud-service.h"
#include "ini-parse.h"
#include "rs485.h"
#include "../leds/pwm_message.h"
#include "led_control.h"
#include "jpegenc.h"
#include "curl_thread.h"

#define MAX_MSG_SIZE 8192
#define QUEUE_CURL "/curl_queue"

// 定义消息结构体
typedef struct {
    char data[MAX_MSG_SIZE];
} Message;
pthread_t curl_tid;
mqd_t mq_curl;
char *out;
static char data_posting_lock=0;
//Message msg;

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
    printf("Free memory: %llu bytes\n", free_memory);
    return 0;
}

// Curl线程函数
void *curl_thread(void *arg) {
    CURL *curl;
    CURLcode res;
    mqd_t mq;
    Message msg;
    struct mq_attr attr;
	unsigned int cnt=0;
    struct curl_slist *headers = NULL;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    mq = mq_open(QUEUE_CURL, O_RDONLY | O_CREAT, 0644, NULL);
    if (mq == (mqd_t)-1) {
        perror("mq_open");
        return;
    }

#if 1
    while (1) {
        // 接收消息
        if (mq_receive(mq, (char *)&msg, sizeof(Message), NULL) == -1) {
            perror("mq_receive");
            continue;
        }
        //memory_print();
        if (curl == NULL) {
            curl = curl_easy_init();
        }
    	cnt++;
		//printf("GET mq data(%d) =%s, post data length=%d\n",cnt, msg.data, strlen(out));
        //if (cnt>=97) {
        //printf("JSON(%d):\n%s\n",strlen(out),out);
        //}
        // 初始化 libcurl
        //curl = curl_easy_init();
        if (curl) {
            //printf("curl start url = %s\n",msg.data);
            data_posting_lock=1;
            curl_easy_setopt(curl, CURLOPT_URL, msg.data);
            //curl_easy_setopt(curl, CURLOPT_URL,"http://abcd");
            headers = curl_slist_append(headers, "Expect:");
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, out);
            /* if we do not provide POSTFIELDSIZE, libcurl will strlen() by itself */
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1L);
            // 设置超时时间
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

            // 设置连接超时时间
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

            // 执行非阻塞操作
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL); // 设置一个用于处理响应数据的回调函数，这里暂时设为NULL

            /* Perform the request, res will get the return code */
            while (is_connected(msg.data)<0) {
                printf("ERROR : ethernet disconnected\n");
                sleep(1);
            }
            res = curl_easy_perform(curl);
            /* Check for errors */
            if(res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed(%d): %s\n",res, curl_easy_strerror(res));
                //curl_easy_reset(curl);
                if (curl)
                    curl_easy_cleanup(curl);
                curl = NULL;
            }
            free(out);
            if (headers)
                curl_slist_free_all(headers);
            //printf("curl stop\n");
            headers = NULL;
            data_posting_lock=0;
        }

    }
#endif
    printf("exit curl thread \n");
    // 清理资源
    curl_easy_cleanup(curl);
    curl_global_cleanup();  
    // 关闭消息队列
    mq_close(mq);

    return NULL;
}

int curl_send_msg(char *url, char *json) {
    Message msg;
    int len;
    if (data_posting_lock) {
        //printf("CURL post not finished, skip data\n");
        return -1;
    }
    memset(msg.data,0,sizeof(Message));
    //strcpy(msg.data,"sensor message\0");
    strncpy(msg.data,url, strlen(url));
    msg.data[strlen(url)]='\0';
    //printf("send url %s\n", msg.data);
    //printf("json string size = %d\n", strlen(json));
    len = strlen(json);
    out = malloc(len+1);
    memset(out,0,len+1);
    //strcpy(out,json);
    memcpy(out,json,len);
    out[len]='\0';
    //printf("Curl post data: \n %s",out);
    if (mq_send(mq_curl, (char *)&msg, strlen(msg.data), 0) == -1) {
        perror("mq_send");
        return -1;
    }
    else {
        return 0;
    }
}
void curl_thread_destory() {
    mq_close(mq_curl);
    mq_unlink(QUEUE_CURL);
    pthread_cancel(curl_tid);
    pthread_join(curl_tid, NULL);  

}
int curl_thread_create() {
    struct mq_attr attr;
    data_posting_lock=0;
    // create curl thread
    if (pthread_create(&curl_tid, NULL, curl_thread, NULL) != 0) {
        perror("curl pthread_create");
        return -1;
    }
    // create curl queue
    mq_curl = mq_open(QUEUE_CURL, O_WRONLY | O_CREAT, 0644, NULL);
    if (mq_curl == (mqd_t)-1) {
        perror("CURL mq_open");
        return -1;
    }
    else {
        printf("CURL send open\n");
    }
    return 0;
}
