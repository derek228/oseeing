#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mqueue.h>
#include <curl/curl.h>


#include <fcntl.h>

#include "cJSON.h"
#include "cloud-service.h"
#include "http-igreent.h"
#include "ini-parse.h"
#include "sensor.h"
#include "rs485.h"
#include "../leds/pwm_message.h"
#include "led_control.h"
#include "jpegenc.h"

#define MAX_MSG_SIZE 9920
#define QUEUE_NAME "/curl_queue"

// 定义消息结构体
typedef struct {
    char data[MAX_MSG_SIZE];
} Message;

// Curl线程函数
void *curl_thread(void *arg) {
    mqd_t mq;
    Message msg;
    struct mq_attr attr;
    CURL *curl;
    CURLcode res;
	unsigned int cnt=0;
    // 打开消息队列
    mqd_t mq;
    Message msg;

    mq = mq_open(QUEUE_NAME, O_WRONLY | O_CREAT, 0644, NULL);
    if (mq == (mqd_t)-1) {
        perror("mq_open");
        return EXIT_FAILURE;
    }


    // 打开消息队列
    mq = mq_open(QUEUE_NAME, O_RDONLY | O_CREAT, 0644, NULL);
    if (mq == (mqd_t)-1) {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }
#if 1
        while (1) {
            // 接收消息
            if (mq_receive(mq, (char *)&msg, sizeof(Message), NULL) == -1) {
                perror("mq_receive");
                break;
            }

            // 发送数据
		cnt++;
		printf("GET mq data %d\n",cnt);
        }
#else
    // 初始化 libcurl
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://example.com");

        // 从消息队列中读取消息并传输
        while (1) {
            // 接收消息
            if (mq_receive(mq, (char *)&msg, sizeof(Message), NULL) == -1) {
                perror("mq_receive");
                break;
            }

            // 发送数据
            res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, msg.data);
            if (res != CURLE_OK) {
                fprintf(stderr, "curl_easy_setopt() failed: %s\n", curl_easy_strerror(res));
            } else {
                res = curl_easy_perform(curl);
                if(res != CURLE_OK) {
                    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                }
            }
        }

        // 清理资源
        curl_easy_cleanup(curl);
    }
#endif

    // 关闭消息队列
    mq_close(mq);

    return NULL;
}

int curl_thread_init() {
    pthread_t tid;

    // 创建Curl线程
    if (pthread_create(&tid, NULL, curl_thread, NULL) != 0) {
        perror("pthread_create");
        return EXIT_FAILURE;
    }

    // 模拟主机接收数据并发送给Curl线程
    while (1) {
        printf("Enter data to send: ");
        fgets(msg.data, sizeof(msg.data), stdin);

        // 发送消息到消息队列
        if (mq_send(mq, (char *)&msg, sizeof(Message), 0) == -1) {
            perror("mq_send");
            break;
        }
    }

    // 关闭消息队列
    mq_close(mq);
    mq_unlink(QUEUE_NAME);

    // 等待Curl线程结束
    pthread_join(tid, NULL);

    return 0;
}

