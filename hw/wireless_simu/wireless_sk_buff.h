#ifndef WIRELESS_SIMU_SK_BUFF
#define WIRELESS_SIMU_SK_BUFF
#include "wireless_simu.h"

/* 存放从内核中load出来的sk_buff结构体，便于统一和驱动间的编程 */

#define __aligned(x) __attribute__((__aligned__(x)))

#define struct_group(NAME, MEMBERS...) \
    __struct_group(/* no tag */, NAME, /* no attrs */, MEMBERS)

#define IEEE80211_TX_INFO_DRIVER_DATA_SIZE 40

typedef unsigned char *sk_buff_data_t;

/* 和引用计数器相关，在不引入垃圾回收机制的情况下应该无需这些结构体 */
typedef struct
{
    int counter;
} atomic_t;

typedef struct refcount_struct
{
    atomic_t refs;
} refcount_t;
/* end 引用计数器相关 */

struct sk_buff
{
    /* 前面一大套和网络、设备相关的东西，不用管 */

    char cb[48] __aligned(8);

    unsigned int len, data_len;
    __u16 mac_len, hdr_len;

    /* 总之就是网络帧的头部字段 */
    struct_group(headers,
                 __u16 protocol;
                 __u16 transport_header;
                 __u16 mac_header;); /* end header group */

    /* 这几个必须放到结尾 */
    sk_buff_data_t tail; // 没用
    sk_buff_data_t end;  // 没用
    unsigned char *head; // 没用
    unsigned char *data; // 指向连续的数据区域
    unsigned int truesize;

    // 引用计数器，但我这里不需要垃圾回收机制
    refcount_t users;
};

struct wireless_simu_skb_cb
{
    dma_addr_t paddr;
    uint8_t flags;
    uint32_t cipher;
    struct ieee80211_vif *vif;
} __attribute__((packed));

static inline struct wireless_simu_skb_cb *WIRELESS_SIMU_SKB_CB(struct sk_buff *skb)
{
	QEMU_BUILD_BUG_ON(sizeof(struct wireless_simu_skb_cb) > 
		IEEE80211_TX_INFO_DRIVER_DATA_SIZE);
	return (struct wireless_simu_skb_cb *)skb->cb;
}

struct sk_buff *alloc_skb(unsigned int size);

void free_skb(struct sk_buff *skb);

#endif /*WIRELESS_SIMU_SK_BUFF*/