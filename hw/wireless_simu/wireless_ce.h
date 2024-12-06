#ifndef WIRELESS_SIMU_CE
#define WIRELESS_SIMU_CE

#include "wireless_simu.h"

#define WIRELESS_SIMU_CE_COUNT 1
#define SRNG_TEST_PIPE_COUNT_MAX 1 // ring（pipes）数量

struct srng_test_ring
{
	/* ring 中 entries 数量 */
	unsigned int nentries;
	unsigned int nentries_mask;

	/*
	 * */
	unsigned int sw_index;

	unsigned int write_index;

	/* 为 entries alloc 的 memory 空间*/
	/* 逻辑内存地址 */
	void *base_addr_owner_space_unaligned;
	/* 物理内存地址 */
	dma_addr_t base_addr_test_space_unaligned;
    
    /* dma malloc size */
    size_t dma_size;

	/* 经由内存对齐之后的，为 entries alloc 的 memory 地址
	 * 这就产生了问题，当使用ALIGN进行对齐的时候，一方面会导致void *被修改，使整个内存空间变少，另一个dmaaddr和void *未必有关联，那万一一个改了另一个没改不就发生冲突了吗？*/
	/* 逻辑内存地址 */
	void *base_addr_owner_space;
	/* 物理内存地址 */
	dma_addr_t base_addr_test_space;

	/* hal ring id */
	uint32_t hal_ring_id;

	/* 灵活数组必须放到结尾, 这里和驱动中的不同
	 * 驱动中这里存放的是一个某长度的 sk_buff 指针 数组，因为驱动之上会自动产生 sk_buff 这里只需要做一个记录即可
	 * 但是在设备中不会自动产生 sk_buff 只会自动产生data，因此这里设定上是一个 sk_buff 的数组，这样就能存放固定大小的，
	 * 已经有paddr的skb，等待其他部分来访问，使用 */
	struct sk_buff *skb;
};

struct srng_test_pipe
{
	struct wireless_simu_device_state *wd;
	uint16_t pipe_num;
	unsigned int attr_flags;
	unsigned int buf_sz;
	unsigned int rx_buf_needed;

	void (*recv_cb)(struct wireless_simu_device_state *wd, struct sk_buff *skb);
	void (*send_cb)(struct wireless_simu_device_state *wd, struct sk_buff *skb);

	// struct tasklet_struct intr_tq;

    /* 向hw传输数据 */
	// struct srng_test_ring *src_ring;
	
    /* 填充sw中申请的dma地址 */
    struct srng_test_ring *dst_ring;
	
    /* 填充有rx的数据 */
    struct srng_test_ring *status_ring;

	uint64_t timestamp;
};

struct srng_test_attr
{
	unsigned int flags;

	/* src entry 数量 */
	unsigned int src_nentries;

	/* 每一个 entry 的最大大小, 虽然名为src, 但dst的也可以用这个参数来设定entry大小限制 */
	unsigned int src_sz_max;

	/* dst entry 数量 */
	unsigned int dest_nentries;

	void (*recv_cb)(struct wireless_simu_device_state *wd, struct sk_buff *skb);
	void (*send_cb)(struct wireless_simu_device_state *wd, struct sk_buff *skb);
};

/* 虽然名叫copy_engine，但是实际上仅负责数据的hw 2 sw的功能
 * 
 * 因为tx路径上下传的数据只需要发出去即可，没有什么处理方案
 * */
struct copy_engine
{
	struct wireless_simu_device_state *wd;

	int ce_num;
    
    /* 下方pipes 的数量 */
    int pipes_count;
    
	struct srng_test_pipe pipes[SRNG_TEST_PIPE_COUNT_MAX];
	const struct srng_test_attr *host_config;
	pthread_mutex_t srng_test_lock;

    /* 超时处理
     * pipe 的 dst 中空位不足时，使用该数据来保证过一段时间后重试 */
    // struct timer_list rx_replenish_retry;
};

/* 驱动下发至设备的数据 */
struct hal_test_dst{
    uint32_t buffer_addr_low;
    uint32_t buffer_addr_info;
    uint32_t flag; 
}__attribute__((__packed__));

struct hal_test_dst_status{
    uint32_t buffer_length;
    uint32_t flag;
}__attribute__((__packed__));

/* 对ce进行初始化
 *
 * 和driver中的初始化不同，hw中对ce的初始化是不需要分配大量的空间的，
 * hw本身也不应该占用大量的内存空间*/
int wireless_simu_ce_init(struct wireless_simu_device_state *wd);

#endif /*WIRELESS_SIMU_CE*/