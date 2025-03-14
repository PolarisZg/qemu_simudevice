#ifndef WIRELESS_SIMU_TX
#define WIRELESS_SIMU_TX

#ifdef DEBUG
// gcc wireless_txrx.c -DDEBUG -o wireless_txrx.out $(pkg-config --cflags --libs glib-2.0)
typedef int bool;
#define false 0
#define true 1
static char *WIRELESS_SIMU_DEVICE_NAME;
#else /* DEBUG */
#include "wireless_simu.h"
#endif /* DEBUG */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <pthread.h>

// 发送数据报文
int wireless_tx_data(void *data, size_t data_size);

// 初始化函数, 仅接受 rx 数据后的处理函数指针
int wireless_txrx_init(void (*rx_data_handler)(void* data, size_t len));

#endif /*WIRELESS_SIMU_TX*/