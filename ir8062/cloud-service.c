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
#define IGREENT_CLOUD 1


#ifdef IGREENT_CLOUD
	#include "cloud/igreent.c"
#else // run igreent cloud as default
	#include "cloud/igreent.c"
#endif