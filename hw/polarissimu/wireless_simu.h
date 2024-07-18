#ifndef WIRELESS_SIMU_POI
#define WIRELESS_SIMU_POI

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "hw/pci/pci.h"
#include "hw/hw.h"
#include "hw/pci/msi.h"
#include "qemu/timer.h"
#include "qom/object.h"
#include "qemu/main-loop.h" /* iothread mutex */
#include "qemu/module.h"
#include "qapi/visitor.h"

// 使用host -- target 来区分操作系统和虚拟设备，此处的host实际是指在qemu中运行的ghost系统，而非运行qemu的host

#define WIRELESS_DEVICE_NAME "polariswfifi"
#define WIRELESS_DMA_SIZE_BIT 16 // 使用 2 ^ WIRELESS_DMA_SIZE_BIT 大小的数组来存放发送到设备的数据，便于之后的大小比较
#define WIRELESS_DMA_SIZE ((1 << WIRELESS_DMA_SIZE_BIT) - 1)

#define WIRELESS_CHECK_WORD_FLAG(flag, n) (!((flag >> n) & 1))
#define WIRELESS_FLAG_DMA_NODE_ISUSING_BIT 0

#define WIRELESS_TX_RING_SIZE 10
#define WIRELESS_RX_RING_SIZE 2

#define WIRELESS_REG_TEST 0x00
#define WIRELESS_REG_EVENT 0x10
#define WIRELESS_REG_DMA_IN_HOSTADDR 0x20
#define WIRELESS_REG_DMA_IN_LENGTH 0x30
#define WIRELESS_REG_DMA_IN_BUFF_ID 0x40
#define WIRELESS_REG_DMA_IN_FLAG 0xA0
#define WIRELESS_REG_IRQ_STATUS 0x50
#define WIRELESS_REG_DMA_OUT_HOSTADDR 0x60
#define WIRELESS_REG_DMA_OUT_LENGTH 0x70
#define WIRELESS_REG_DMA_OUT_BUFF_ID 0x80
#define WIRELESS_REG_DMA_OUT_FLAG 0x90
#define WIRELESS_REG_DMA_RX_RING_BUF_ID 0xB0
#define WIRELESS_REG_DMA_RX_RING_HOSTADDR 0xC0
#define WIRELESS_REG_DMA_RX_RING_LENGTH 0xD0
#define WIRELESS_REG_DMA_RX_RING_FLAG 0xE0
#define WIRELESS_REG_IRQ_ENABLE 0xF0

#define WIRELESS_BITCHECK(num, n) ((num >> n) & 1)
#define WIRELESS_BITSET(num, n) (num |= (1 << n))
#define WIRELESS_BITCLR(num, n) (num &= ~(1 << n))

#define WIRELESS_RX_RING_BUF_IS_USING 0
#define WIRELESS_RX_RING_BUF_INIT_END 1

/*
 * @brief 真实存放数据的Node
 *
 * 所有指向该数据结构的结构都要在本结构中有一个指针记录，用来防止在删除node时防止出现悬空指针
 */
struct Wireless_DMA_Node
{
    u_int32_t node_id;
    u_int32_t data_length;
    u_int32_t flag;
    void *data;
    struct Wireless_DMA_Node *next;
};

/*
 * @brief 对device dma的描述
 */
struct Wireless_DMA_Detail
{
    struct Wireless_DMA_Node *dma_node_list;
    struct Wireless_DMA_Node *dma_node_tail;
    u_int32_t dma_node_count;
    u_int32_t dma_node_count_size;
    u_int64_t dma_data_length;
    u_int64_t dma_max_mem_size;
};

/*
 * @brief DMA 方向
 */
enum Wireless_DMA_Direction
{
    DMA_NOUSE = 0,
    DMA_MEMORY_TO_DEVICE,
    DMA_DEVICE_TO_MEMORY,
};

/*
 * @brief 数据源
 */
struct Wireless_Data_Detail
{
    u_int64_t host_addr;
    enum Wireless_DMA_Direction DMA_derection;
    u_int32_t data_length;
    u_int32_t data_max_length;
    u_int32_t host_buffer_id;
    u_int32_t dma_node_id;
    u_int32_t flag;
};

enum Wireless_LongTimeEvent
{
    WIRELESS_EVENT_NOEVENT = 0,
    WIRELESS_EVENT_DMA,
    WIRELESS_EVENT_CLEAN_DMA,
    WIRELESS_EVENT_TEST,
};

enum Wireless_DMA_IRQ_STATUS
{
    WIRELESS_IRQ_TEST = 0,
    WIRELESS_IRQ_DNA_MEM_TO_DEVICE_END,
    WIRELESS_IRQ_DMA_DELALL_END,
    WIRELESS_IRQ_RX_START,
    WIRELESS_IRQ_DMA_DEVICE_TO_MEM_END,
};
// wireless 设备非静态成员
struct WirelessDeviceState
{
    struct PCIDevice parent_obj;

    bool stop;

    MemoryRegion mmio;

    u_int64_t dma_mask;
    bool irq_enable;
    u_int32_t irq_status;
    struct QemuMutex irq_intx_mutex;
    // u_int32_t irq_message;
    QEMUTimer irq_timer;

    // dma
    struct Wireless_DMA_Detail wireless_dma_detail;
    struct QemuMutex dma_access_mutex;
    struct Wireless_Data_Detail tx_ring_buf[WIRELESS_TX_RING_SIZE];
    struct Wireless_Data_Detail rx_ring_buf[WIRELESS_RX_RING_SIZE];
    struct Wireless_Data_Detail *wireless_data_detail;
    struct Wireless_Data_Detail *wireless_data_rx_detail;
    struct QemuMutex dma_out_mutex;
    struct Wireless_Data_Detail *wireless_dma_out_detail;
    struct QemuThread dma_clean_thread;

    // long time event
    enum Wireless_LongTimeEvent long_time_event;
};

DECLARE_INSTANCE_CHECKER(struct WirelessDeviceState, WIRELESS_DEVICE_OBJ, WIRELESS_DEVICE_NAME);

#endif /*WIRELESS_SIMU_POI*/