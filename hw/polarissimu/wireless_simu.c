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

#define WIRELESS_REG_TEST 0x00
#define WIRELESS_REG_EVENT 0x01

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

// wireless 设备非静态成员
struct WirelessDeviceState
{
    struct PCIDevice parent_obj;

    bool stop;

    MemoryRegion mmio;

    u_int64_t dma_mask;
    u_int32_t irq_status;
    struct QemuMutex irq_intx_mutex;
    u_int32_t irq_message;
    QEMUTimer irq_timer;

    // dma
    struct Wireless_DMA_Detail wireless_dma_detail;
    struct QemuMutex dma_access_mutex;
    struct Wireless_Data_Detail wireless_data_detail;
    struct QemuThread dma_clean_thread;

    // long time event
    enum Wireless_LongTimeEvent long_time_event;
};

DECLARE_INSTANCE_CHECKER(struct WirelessDeviceState, WIRELESS_DEVICE_OBJ, WIRELESS_DEVICE_NAME);

static int Wireless_dma_add_node(struct WirelessDeviceState *wd, struct Wireless_DMA_Node *dma_node)
{
    if (dma_node == NULL)
        return -1;
    struct Wireless_DMA_Detail *dma_detail = &wd->wireless_dma_detail;
    if (dma_detail->dma_node_list == NULL)
    {
        dma_detail->dma_node_list = (struct Wireless_DMA_Node *)malloc(sizeof(struct Wireless_DMA_Node));
        dma_detail->dma_node_tail = dma_detail->dma_node_list;
        dma_detail->dma_node_count = 1;
    }
    dma_detail->dma_node_tail->next = dma_node;
    dma_detail->dma_node_tail = dma_node;
    dma_detail->dma_data_length += dma_node->data_length;
    dma_detail->dma_node_count++;
    return 0;
}

static int Wireless_dma_del_node(struct WirelessDeviceState *wd, struct Wireless_DMA_Node *dma_node)
{
    if (dma_node == NULL)
        return 0;
    struct Wireless_DMA_Detail *dma_detail = &wd->wireless_dma_detail;
    struct Wireless_DMA_Node *next = dma_node->next;
    dma_detail->dma_data_length -= dma_node->data_length;
    free(dma_node->data);
    if (next == NULL)
    {
        for (struct Wireless_DMA_Node *i = dma_detail->dma_node_list; i != NULL; i = i->next)
        {
            if (i->next == dma_node)
            {
                dma_detail->dma_node_tail = i;
                break;
            }
        }
        free(dma_node);
        return 0;
    }
    else
    {
        dma_node->data = next->data;
        dma_node->data_length = next->data_length;
        dma_node->next = next->next;
        dma_node->node_id = next->node_id;
        dma_node->flag = next->flag;
        free(next);
        return 0;
    }
}

/*
 * 从内存读
 *
 * 必须提供一个内存中的地址，以及一个待读取的内存长度
 * @DMA_derection DMA_MEMORY_TO_DEVICE
 */
static int Wireless_dma_read_from_mem(struct WirelessDeviceState *wd)
{
    struct Wireless_Data_Detail *data = &wd->wireless_data_detail;
    printf("%s : wirte data to dma : host addr %08lx data length %08x \n derection %d",
           WIRELESS_DEVICE_NAME, data->host_addr, data->data_length, data->DMA_derection);
    if (data->DMA_derection != DMA_MEMORY_TO_DEVICE)
    {
        printf("%s : erro direction \n", WIRELESS_DEVICE_NAME);
        return -5;
    }
    struct Wireless_DMA_Detail *dma_detail = &wd->wireless_dma_detail;
    if (data->data_length > dma_detail->dma_max_mem_size)
    {
        printf("%s : write to dma data too long\n", WIRELESS_DEVICE_NAME);
        return -1;
    }
    struct Wireless_DMA_Node *dma_node = (struct Wireless_DMA_Node *)malloc(sizeof(struct Wireless_DMA_Node));
    if (dma_node == NULL)
    {
        printf("%s : write data malloc node fail \n", WIRELESS_DEVICE_NAME);
        return -2;
    }
    dma_node->node_id = dma_detail->dma_node_count + 1;
    dma_node->data_length = data->data_length;
    dma_node->next = NULL;
    dma_node->data = malloc(dma_node->data_length);
    if (dma_node->data == NULL)
    {
        printf("%s : dma node data malloc fail \n", WIRELESS_DEVICE_NAME);
        free(dma_node);
        return -3;
    }
    if (pci_dma_read(&wd->parent_obj, data->host_addr, dma_node->data, dma_node->data_length))
    {
        printf("%s : write to dma error \n", WIRELESS_DEVICE_NAME);
        free(dma_node->data);
        free(dma_node);
        return -4;
    }
    Wireless_dma_add_node(wd, dma_node);
    data->dma_node_id = dma_node->node_id;
    printf("%s : add dma node success : node id %08x data addr %p data length %08x\n",
           WIRELESS_DEVICE_NAME,
           wd->wireless_dma_detail.dma_node_tail->node_id,
           wd->wireless_dma_detail.dma_node_tail->data,
           wd->wireless_dma_detail.dma_node_tail->data_length);
    return 0;
}

static int Wireless_dma_del_all(struct WirelessDeviceState *wd)
{
    struct Wireless_DMA_Detail *dma_detail = &wd->wireless_dma_detail;
    while (dma_detail->dma_node_list != NULL && dma_detail->dma_node_list != dma_detail->dma_node_tail)
    {
        Wireless_dma_del_node(wd, dma_detail->dma_node_list->next);
    }
    return 0;
}

/*
 * 向内存写
 *
 * 必须提供一个内存地址，以及需要读取的dma node的id
 * @DMA_derection DMA_DEVICE_TO_MEMORY
 */
static int Wireless_dma_read_from_device(struct WirelessDeviceState *wd)
{
    struct Wireless_Data_Detail *data = &wd->wireless_data_detail;
    printf("%s : wirte data to mem : host addr %08lx data length %08x \n derection %d",
           WIRELESS_DEVICE_NAME, data->host_addr, data->data_length, data->DMA_derection);
    if (data->DMA_derection != DMA_DEVICE_TO_MEMORY)
    {
        printf("%s : write data to mem : erro direction \n", WIRELESS_DEVICE_NAME);
        return -5;
    }
    struct Wireless_DMA_Detail *dma_detail = &wd->wireless_dma_detail;
    if (dma_detail == NULL || dma_detail->dma_node_list == NULL)
    {
        printf("%s : write data 2 mem : dma list is null \n", WIRELESS_DEVICE_NAME);
        return -1;
    }
    struct Wireless_DMA_Node *dma_node = NULL;
    for (struct Wireless_DMA_Node *node = dma_detail->dma_node_list; node != NULL; node = node->next)
    {
        if (node->node_id == data->dma_node_id)
        {
            dma_node = node;
            break;
        }
    }
    if (dma_node == NULL)
    {
        printf("%s : write data 2 mem : node 404 \n", WIRELESS_DEVICE_NAME);
        return -2;
    }

    if (pci_dma_write(&wd->parent_obj, data->host_addr, dma_node->data, dma_node->data_length))
    {
        printf("%s : wirte data to mem : read from device err \n", WIRELESS_DEVICE_NAME);
        return -2;
    }
    data->data_length = dma_node->data_length;
    return 0;
}

/*
 * 发出中断
 *
 * 对于Intx 中断，必须清中断
 */
static void Wireless_Interrupt_raise(struct WirelessDeviceState *wd)
{
    if (msi_enabled(&wd->parent_obj))
    {
        msi_notify(&wd->parent_obj, 0);
    }
    else
    {
        qemu_mutex_lock(&wd->irq_intx_mutex);
        pci_set_irq(&wd->parent_obj, 1);
    }
}

/*
 * 清中断
 */
static void Wireless_Interrup_lower(struct WirelessDeviceState *wd)
{
    if (msi_enabled(&wd->parent_obj))
    {
        return;
    }
    else
    {
        pci_set_irq(&wd->parent_obj, 0);
        qemu_mutex_unlock(&wd->irq_intx_mutex);
    }
}

/*
 * 对设备的dma进行管理，防止其占用的内存空间超过memsize发生内存溢出
 *
 * 此函数涉及到敏感的删除操作，需要防止出现悬空指针
 */
static void *Wireless_dma_mem_manager(void *opaque)
{
    printf("%s : dma clean start \n", WIRELESS_DEVICE_NAME);
    struct WirelessDeviceState *wd = opaque;
    struct Wireless_DMA_Detail *dma_detail = &wd->wireless_dma_detail;
    while (1)
    {
        if (wd->stop)
        {
            goto stop;
        }
        qemu_mutex_lock(&wd->dma_access_mutex);
        if (dma_detail->dma_data_length > dma_detail->dma_max_mem_size)
        {
            if (dma_detail->dma_node_list == NULL || dma_detail->dma_node_list == dma_detail->dma_node_tail)
            {
                goto stop;
            }
            for (struct Wireless_DMA_Node *node = dma_detail->dma_node_list; node != NULL; node = node->next)
            {
                /*
                 * 一些对node的判断，对于某些flag的node不能随意删除，比如正在写入到内存的
                 */
                if (node->data_length > 0 && WIRELESS_CHECK_WORD_FLAG(node->flag, WIRELESS_FLAG_DMA_NODE_ISUSING_BIT))
                {
                    Wireless_dma_del_node(wd, node);
                    break;
                }
            }
        }
        qemu_mutex_unlock(&wd->dma_access_mutex);
    }

stop:
    printf("%s : dma clean stop \n", WIRELESS_DEVICE_NAME);
    qemu_mutex_unlock(&wd->dma_access_mutex);
    return NULL;
}

/*
 * 耗时较长的任务*/
static void Wireless_LongTime_Handler(void *opaque)
{
    struct WirelessDeviceState *wd = opaque;
    printf("%s : in long time function %d \n", WIRELESS_DEVICE_NAME, wd->long_time_event);
    switch (wd->long_time_event)
    {
    case WIRELESS_EVENT_DMA:
        if (wd->wireless_data_detail.DMA_derection == DMA_MEMORY_TO_DEVICE)
        {
            Wireless_dma_read_from_mem(wd);
        }
        else if (wd->wireless_data_detail.DMA_derection == DMA_DEVICE_TO_MEMORY)
        {
            Wireless_dma_read_from_device(wd);
        }
        break;
    case WIRELESS_EVENT_CLEAN_DMA:
        Wireless_dma_del_all(wd);
        break;
    case WIRELESS_EVENT_TEST:
        sleep(10);
        break;
    case WIRELESS_EVENT_NOEVENT:
        Wireless_Interrup_lower(wd);
        printf("%s : clean irq \n", WIRELESS_DEVICE_NAME);
        return;
    }
    Wireless_Interrupt_raise(wd);
}

static void Wireless_Add_Task(struct WirelessDeviceState *wd, u_int32_t val)
{
    wd->long_time_event = val;
    if (val == 0)
    {
        Wireless_Interrup_lower(wd);
        printf("%s : clean irq \n", WIRELESS_DEVICE_NAME);
    }
    else
    {
        timer_mod(&wd->irq_timer, qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL) + 100);
    }
}

/*
 * 从 device 的 addr 寄存器读取数据并返回
 *
 * @addr 驱动侧使用 mmio 地址 + addr 读取 addr 对应的寄存器
 * @size 以字节为单位，ioread32 就是 读取 4 个字节, 默认每次读取 4 个字节
 */
static u_int64_t Wireless_read(void *opaque, hwaddr addr, unsigned size)
{
    printf("%s : read : %p : %lu : %u \n", WIRELESS_DEVICE_NAME, opaque, addr, size);
    // struct WirelessDeviceState *wd = opaque;
    u_int64_t val = 0LL;

    switch (addr)
    {
    case WIRELESS_REG_TEST:
        val = 0x114514;
        break;
    default:
        printf("%s : read err no reg %lu \n", WIRELESS_DEVICE_NAME, addr);
        break;
    }
    return val;
}

/*
 * 向 device 的 addr 寄存器 写入数据
 *
 * @addr 驱动侧使用 mmio 地址 + addr 读取 addr 对应的寄存器
 * @data 等待写入的数据
 * @size 从data的最低位开始取出的数据长度, 以字节为单位, 默认(强制要求)驱动侧使用iowrite32
 */
static void Wireless_write(void *opaque, hwaddr addr, u_int64_t data, unsigned size)
{
    printf("%s : write data : %p : %lu : %lu : %u \n", WIRELESS_DEVICE_NAME, opaque, addr, data, size);
    if (size != 4)
    {
        printf("%s : size err \n", WIRELESS_DEVICE_NAME);
        return;
    }

    struct WirelessDeviceState *wd = opaque;
    u_int32_t val = (u_int32_t)(data & 0xffffffff);
    switch (addr)
    {
    case WIRELESS_REG_TEST:
        printf("%s : write data : %08x \n", WIRELESS_DEVICE_NAME, val);
        break;
    case WIRELESS_REG_EVENT:
        Wireless_Add_Task(wd, val);
        break;
    default:
        printf("%s : write err no reg %lu \n", WIRELESS_DEVICE_NAME, addr);
        break;
    }
}

static u_int64_t Wireless_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    return Wireless_read(opaque, addr, size);
}

static void Wireless_mmio_write(void *opaque, hwaddr addr, u_int64_t val, unsigned size)
{
    Wireless_write(opaque, addr, val, size);
}

static const struct MemoryRegionOps Wireless_mmio_ops = {
    .read = Wireless_mmio_read,
    .write = Wireless_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void Wireless_realize(struct PCIDevice *pdev, struct Error **errp)
{
    struct WirelessDeviceState *wd = WIRELESS_DEVICE_OBJ(pdev);

    // intx irq
    pci_config_set_interrupt_pin(pdev->config, 1);

    // msi irq
    if (msi_init(pdev, 0, 1, false, false, errp))
    {
        printf("test pci : msi interrupt malloc falut \n");
    }

    // 需长时间处理的任务
    timer_init_ms(&wd->irq_timer, QEMU_CLOCK_VIRTUAL, Wireless_LongTime_Handler, wd);

    // 多线程锁
    qemu_mutex_init(&wd->dma_access_mutex);
    qemu_mutex_init(&wd->irq_intx_mutex);

    // dma 清理线程
    qemu_thread_create(&wd->dma_clean_thread, "polariswireless-dmaclean",
                       Wireless_dma_mem_manager, wd, QEMU_THREAD_JOINABLE);

    // mmio 最后的数字的作用是限制写入的大小，往大了写就行
    memory_region_init_io(&wd->mmio, OBJECT(wd),
                          &Wireless_mmio_ops, pdev, "polariswireless-mmio", 4096);
    pci_register_bar(pdev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &wd->mmio);
}

static void Wireless_exit(struct PCIDevice *pdev)
{
    struct WirelessDeviceState *wd = WIRELESS_DEVICE_OBJ(pdev);

    qemu_mutex_lock(&wd->dma_access_mutex);
    wd->stop = true;
    qemu_mutex_unlock(&wd->dma_access_mutex);

    qemu_thread_join(&wd->dma_clean_thread);
    qemu_mutex_destroy(&wd->dma_access_mutex);
    qemu_mutex_destroy(&wd->irq_intx_mutex);
    timer_del(&wd->irq_timer);
    msi_uninit(pdev);
}

static void Wireless_instance_init(struct Object *obj)
{
    struct WirelessDeviceState *wd = WIRELESS_DEVICE_OBJ(obj);
    wd->dma_mask = (1UL << 32) - 1;
    wd->irq_message = 0;
    wd->irq_status = 0;
    wd->long_time_event = WIRELESS_EVENT_NOEVENT;
    struct Wireless_DMA_Detail *dma_detail = &wd->wireless_dma_detail;
    dma_detail->dma_max_mem_size = 0xfffff;
    dma_detail->dma_node_list = (struct Wireless_DMA_Node *)malloc(sizeof(struct Wireless_DMA_Node));
    dma_detail->dma_node_tail = dma_detail->dma_node_list;
    dma_detail->dma_data_length = 0;
    dma_detail->dma_node_count = 1;
}

static void Wireless_class_init(struct ObjectClass *class, void *data)
{
    struct DeviceClass *dc = DEVICE_CLASS(class);
    struct PCIDeviceClass *pci = PCI_DEVICE_CLASS(class);

    pci->realize = Wireless_realize;
    pci->exit = Wireless_exit;
    pci->vendor_id = PCI_VENDOR_ID_QEMU;
    pci->device_id = 0x1145;
    pci->revision = 0x14;
    pci->class_id = PCI_CLASS_OTHERS;

    dc->desc = "polaris wireless device";
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const struct TypeInfo Wireless_type_info = {
    .name = WIRELESS_DEVICE_NAME,
    .parent = TYPE_PCI_DEVICE,
    .instance_init = Wireless_instance_init,
    .instance_size = sizeof(struct WirelessDeviceState),
    .class_init = Wireless_class_init,
    .interfaces = (struct InterfaceInfo[]){
        {INTERFACE_CONVENTIONAL_PCI_DEVICE},
        {},
    },
};

static void Wireless_register_types(void)
{
    printf("%s : register start", WIRELESS_DEVICE_NAME);
    type_register_static(&Wireless_type_info);
}

type_init(Wireless_register_types);