#ifndef __CURL_THREAD_H__
#define __CURL_THREAD_H__

int curl_thread_create();
void curl_thread_destory();
int curl_send_msg(char *url, char *json);

#endif
