
#ifdef DEBUG
#include "wireless_txrx.h"
static int pid = 0;
#else
#include "wireless_simu.h"
#endif /* DEBUG */

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT_1 12700
#define SERVER_PORT_2 12701

static int sockfd_tx = 0;
static bool tx_stop = false;
static GMutex tx_lock;
static struct sockaddr_in server_addr_tx = {0};

static int sockfd_rx = 0;
static bool rx_stop = false;
static pthread_t rx_thread;
GThreadPool *rx_thread_pool = NULL;
#define RX_BUFFER_SIZE 2048
static char rx_buffer[RX_BUFFER_SIZE];
static void (*rx_handler)(void *data, size_t len, void* device) = NULL;

static int bind_rx_port(int port, int *sock_fd)
{
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(*sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        return -1;
    }

    return 0;
}

static int bind_tx_port(const char *addr, int port, struct sockaddr_in *server_addr)
{
    if (server_addr == NULL)
        return -2;
    memset(server_addr, 0, sizeof(struct sockaddr_in));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(port);
    inet_pton(AF_INET, addr, &server_addr->sin_addr);

    return 0;
}

static int init_txrx_fd(void)
{
    sockfd_tx = socket(AF_INET, SOCK_DGRAM, 0);
    sockfd_rx = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd_tx < 0 || sockfd_rx < 0)
    {
        printf("%s : init fd tx %d rx %d err \n", WIRELESS_SIMU_DEVICE_NAME, sockfd_tx, sockfd_rx);
        exit(1);
    }
    
    if (bind_rx_port(SERVER_PORT_1, &sockfd_rx) == 0)
    {
        if (bind_tx_port(SERVER_ADDR, SERVER_PORT_2, &server_addr_tx))
        {
            printf("%s : init fd bind err \n", WIRELESS_SIMU_DEVICE_NAME);
            exit(1);
        }
        printf("%s : tx rx port %d %d \n", WIRELESS_SIMU_DEVICE_NAME, SERVER_PORT_2, SERVER_PORT_1);
    }
    else
    {
        if (bind_rx_port(SERVER_PORT_2, &sockfd_rx) || bind_tx_port(SERVER_ADDR, SERVER_PORT_1, &server_addr_tx))
        {
            printf("%s : init fd bind this device num overflow 2 \n", WIRELESS_SIMU_DEVICE_NAME);
            exit(1);
        }
        printf("%s : tx rx port %d %d \n", WIRELESS_SIMU_DEVICE_NAME, SERVER_PORT_1, SERVER_PORT_2);
    }

    return 0;
}

int wireless_tx_data(void *data, size_t data_size)
{
    int ret = 0;

    if (tx_stop)
    {
        printf("%s : wireless tx stop \n", WIRELESS_SIMU_DEVICE_NAME);
        return -2;
    }

    if(data_size >= RX_BUFFER_SIZE){
        printf("%s : wireless tx cant send so big data %ld \n", WIRELESS_SIMU_DEVICE_NAME, data_size);
        
        #ifndef DEBUG
        return -3;
        #else
        data_size = RX_BUFFER_SIZE;
        #endif
    }

    g_mutex_lock(&tx_lock);
    if (server_addr_tx.sin_port == 0 || server_addr_tx.sin_family == 0)
    {
        printf("%s : wireless tx server addr struct nor init \n", WIRELESS_SIMU_DEVICE_NAME);
        goto END;
    }

    ret = sendto(sockfd_tx, data, data_size, 0, (struct sockaddr *)&server_addr_tx, sizeof(server_addr_tx));
    if (ret == -1)
    {
        printf("%s : wireless tx socket send err \n", WIRELESS_SIMU_DEVICE_NAME);
        goto END;
    }

    printf("%s : send success \n", WIRELESS_SIMU_DEVICE_NAME);

END:
    g_mutex_unlock(&tx_lock);
    return ret;
}

typedef struct rx_data_packet_define
{
    size_t len;
    void *data;
} rx_data_packet;

/* 单开一个线程去监听, 所以不需要去考虑阻塞的问题 */
static void *wireless_rx_data(void *p_data)
{
    while (!rx_stop)
    {
        // memset(rx_buffer, 0, RX_BUFFER_SIZE);
        struct sockaddr_in sender_addr = {0};
        socklen_t sender_len = sizeof(sender_addr);

        size_t received = recvfrom(sockfd_rx, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr *)&sender_addr, &sender_len);

        if (received <= 0)
        {
            printf("%s : wireless rx err \n", WIRELESS_SIMU_DEVICE_NAME);
            continue;
        }

        void *data = malloc(received);
        if (!data)
        {
            printf("%s : wireless rx memory fuull \n", WIRELESS_SIMU_DEVICE_NAME);
        }
        memcpy(data, rx_buffer, received);

        rx_data_packet *skb = (rx_data_packet *)malloc(sizeof(rx_data_packet));
        if (!skb)
        {
            printf("%s : wireless rx memory packet fuull \n", WIRELESS_SIMU_DEVICE_NAME);
        }

        skb->data = data;
        skb->len = received;

        g_thread_pool_push(rx_thread_pool, (void *)skb, NULL);
    }

    return NULL;
}

static void wireless_rx_data_handler_task(gpointer data, gpointer user_data)
{
    rx_data_packet *skb = (rx_data_packet *)data;

    // 对接收到的数据进行处理 这里选择打印一下
    if(rx_handler){
        rx_handler(skb->data, skb->len, user_data);
    }

    free(skb->data);
    skb->data = NULL;

    free(skb);
}

int wireless_txrx_init(void (*rx_data_handler)(void* data, size_t len, void* device), void* device)
{
    g_mutex_init(&tx_lock);

    if (init_txrx_fd())
        printf("%s : fd init err \n", WIRELESS_SIMU_DEVICE_NAME);

    rx_stop = false;
    rx_handler = rx_data_handler;

    // rx 单独一个线程去处理
    rx_thread_pool = g_thread_pool_new(wireless_rx_data_handler_task, device, 20, FALSE, NULL);

    rx_stop = false;
    pthread_create(&rx_thread, NULL, &wireless_rx_data, NULL);

    return 0;
}

void wireless_txrx_deinit(void)
{
    tx_stop = true;
    rx_stop = true;

    pthread_join(rx_thread, NULL);

    g_thread_pool_free(rx_thread_pool, FALSE, TRUE);

    rx_handler = NULL;
}

#ifdef DEBUG
void test_rx_handler(void *data, size_t len, void* device)
{
    fprintf(stdout, "%s : from %d : tail 3 char %c %c %c len %ld \n", WIRELESS_SIMU_DEVICE_NAME,
            *(int *)data,
            *(char *)(data + len - 3), *(char *)(data + len - 2), *(char *)(data + len - 1),
            len);
}

void handle_sigint(int sig) {
    wireless_txrx_deinit();
    exit(0);
}

int main()
{

    WIRELESS_SIMU_DEVICE_NAME = (char *)malloc(30);
    sprintf(WIRELESS_SIMU_DEVICE_NAME, "%s %d", "wireless_txrx_test", getpid());

    wireless_txrx_init(test_rx_handler, NULL);

    // for (int i = 0; i < 65535; i++)
    // {
    //     size_t len = i * 23 + 7;
    //     int *data = (int *)malloc(len);
    //     memset(data, 0x12345678, len);
    //     *data = getpid();
    //     wireless_tx_data((void *)data, len);
    //     // sleep(1);
    //     free(data);
    // }

    if(signal(SIGINT, handle_sigint) == SIG_ERR) {
        perror("signal");
        wireless_txrx_deinit();
        return 1;
    }

    while(1){}
    
    return 0;
}
#endif /* DEBUG */