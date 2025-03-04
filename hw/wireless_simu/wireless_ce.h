#ifndef WIRELESS_SIMU_CE
#define WIRELESS_SIMU_CE

#include "wireless_simu.h"

#define WIRELESS_SIMU_CE_COUNT 1
#define SRNG_TEST_PIPE_COUNT_MAX 1 // ring（pipes）数量

struct wireless_simu_ce_ring
{
	/* ring 中 entries 数量 */
	unsigned int nentries;
	unsigned int nentries_mask;

	/*
	 * 已填充的dma_addr地址头指针*/
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

struct wireless_simu_ce_pipe
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
	// struct wireless_simu_ce_ring *src_ring;
	
    /* 填充sw中申请的dma地址 */
    struct wireless_simu_ce_ring *dst_ring;
	
    /* 填充有rx的数据 */
    struct wireless_simu_ce_ring *status_ring;

	uint64_t timestamp;

	/* 访问锁 */
	pthread_mutex_t pipe_lock;
};

struct ce_attr
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
    
	struct wireless_simu_ce_pipe pipes[SRNG_TEST_PIPE_COUNT_MAX];
	const struct ce_attr *host_config;
	pthread_mutex_t ce_lock;

    /* 超时处理
     * pipe 的 dst 中空位不足时，使用该数据来保证过一段时间后重试 */
    // struct timer_list rx_replenish_retry;
};

/* ce src desc 内容*/
struct hal_ce_srng_src_desc
{
    uint32_t buffer_addr_low;
    uint32_t buffer_addr_info; /* %HAL_CE_SRC_DESC_ADDR_INFO_ */
    uint32_t meta_info;        /* %HAL_CE_SRC_DESC_META_INFO_ */
    uint32_t flags;            /* %HAL_CE_SRC_DESC_FLAGS_ */
} __attribute__((__packed__));

/* ce dst desc 内容 */
struct hal_ce_srng_dest_desc
{
    uint32_t buffer_addr_low;
    uint32_t buffer_addr_info; /* %HAL_CE_DEST_DESC_ADDR_INFO_ */
} __attribute__((__packed__));

/* ce dst status 内容*/
struct hal_ce_srng_dst_status_desc
{
    uint32_t flags; /* %HAL_CE_DST_STATUS_DESC_FLAGS_ */
    uint32_t toeplitz_hash0;
    uint32_t toeplitz_hash1;
    uint32_t meta_info; /* HAL_CE_DST_STATUS_DESC_META_INFO_ */
} __attribute__((__packed__));

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

/* 对 dst ring 的处理 
 * dst_ring 本身是 src_ring 
 * 存储 paddr */
void ce_dst_ring_handler(void *user_data);

/* 对ce进行初始化
 *
 * 和driver中的初始化不同，hw中对ce的初始化是不需要分配大量的空间的，
 * hw本身也不应该占用大量的内存空间*/
int wireless_simu_ce_init(struct wireless_simu_device_state *wd);

/* 向驱动发送数据 
 * 该发送不用考虑是否成功, 不成功就是驱动方面出了问题, 不能耽误之后的发送操作
 */
void wireless_simu_ce_post_data(struct wireless_simu_device_state *wd, void *data, size_t data_size);

#endif /*WIRELESS_SIMU_CE*/