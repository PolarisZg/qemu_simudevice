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

#define TESTPCI_BUFF_SIZE 4096
#define WIRELESS_DMA_SIZE_BIT 16 // 使用 2 ^ WIRELESS_DMA_SIZE_BIT 大小的数组来存放发送到设备的数据，便于之后的大小比较
#define WIRELESS_DMA_SIZE ((1 << WIRELESS_DMA_SIZE_BIT) - 1)

/*
 * @brief 真实存放数据的Node
 * 
 * 所有指向该数据结构的结构都要在本结构中有一个备份，用来防止在删除node时防止出现悬空指针
 */
struct Wireless_DMA_Node
{
    u_int32_t node_id;
    u_int32_t data_length;
    void *data;
    struct Wireless_DMA_Node *next;

    // 下述结构指向该Node结构
    struct Wireless_Data_Detail *data_detail;
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
    DMA_MEMORY_TO_DEVICE = 0,
    DMA_DEVICE_TO_MEMORY;
};

#define WIRELESS_DATA_DETAIL_BUF_SIZE 20
/*
 * @brief 数据源
 */
struct Wireless_Data_Detail
{
    u_int64_t host_addr;
    u_int32_t dma_node_id;
    enum Wireless_DMA_Direction DMA_derection;
    struct Wireless_DMA_Node *dma_node;
    u_int32_t data_length;
    u_int16_t flags;
};

// wireless 设备非静态成员
struct WirelessDeviceState
{
    struct PCIDevice parent_obj;

    bool stop;

    MemoryRegion mmio;
    MemoryRegion pmio;
    char buff[TESTPCI_BUFF_SIZE];

    u_int64_t dma_mask;
    u_int32_t bufferPtr;
    u_int32_t irq_status;
    struct QemuMutex irq_intx_mutex;
    u_int32_t irq_message;
    QEMUTimer irq_timer;

    // data
    struct Wireless_Data_Detail[WIRELESS_DATA_DETAIL_BUF_SIZE];
    
    // dma
    struct Wireless_DMA_Detail wireless_dma_detail;
    struct QemuMutex dma_access_mutex;
};

// 定义设备实例
struct TestDevicePciState
{
    struct PCIDevice parent_obj;

    MemoryRegion mmio;
    MemoryRegion pmio;
    char buff[TESTPCI_BUFF_SIZE];

    u_int64_t dma_mask;
    u_int32_t bufferPtr;
    u_int32_t irq_status;
    QEMUTimer irq_timer;
};

#define TYPE_TESTDEV_PCI "hellodev-pci"

// 类型转换和检查
// #define TESTDEV_PCI(obj) OBJECT_CHECK(struct TestDevicePciState, (obj), TYPE_TESTDEV_PCI)
DECLARE_INSTANCE_CHECKER(struct TestDevicePciState, TESTDEV_PCI, TYPE_TESTDEV_PCI);

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
    dma_node->data_detail->dma_node = NULL;
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
        dma_node->data_detail = next->data_detail
                                    dma_node->data_length = next->data_length;
        dma_node->next = next->next;
        dma_node->node_id = next->node_id;
        free(next);
        return 0;
    }
}

static int Wireless_dma_read_from_mem(struct WirelessDeviceState *wd, struct Wireless_Data_Detail *data)
{
    printf("wireless device : wirte data to dma : host addr %08x data length %08x \n derection %d",
           data->host_addr, data->data_length, data->DMA_derection);
    if (data->DMA_derection != DMA_MEMORY_TO_DEVICE)
    {
        printf("wireless device : erro direction \n");
        return -5;
    }
    struct Wireless_DMA_Detail *dma_detail = &wd->wireless_dma_detail;
    if (data->data_length > dma_detail->dma_max_mem_size)
    {
        printf("wireless device : write to dma data too long\n");
        return -1;
    }
    struct Wireless_DMA_Node *dma_node = (struct Wireless_DMA_Node *)malloc(sizeof(struct Wireless_DMA_Node));
    if (dma_node == NULL)
    {
        printf("wirelsee device : write data malloc node fail \n");
        return -2;
    }
    dma_node->node_id = dma_detail->dma_node_count + 1;
    dma_node->data_length = data->data_length;
    dma_node->next = NULL;
    dma_node->data = malloc(dma_node->data_length);
    if (dma_node->data == NULL)
    {
        printf("wireless device : dma node data malloc fail \n");
        free(dma_node);
        return -3;
    }
    if (pci_dma_read(wd->parent_obj, data->host_addr, dma_node->data, dma_node->data_length))
    {
        printf("wireless device : write to dma error \n");
        free(dma_node->data);
        free(dma_node);
        return -4;
    }
    data->dma_node = dma_node;
    data->dma_node_id = dma_node->node_id;
    dma_node->data_detail = data;
    Wireless_dma_add_node(wd, dma_node);
    printf("wireless device : add dma node success : node id %08x data addr %p data length %08x\n",
           wd->wireless_dma_detail.dma_node_tail->node_id,
           wd->wireless_dma_detail.dma_node_tail->data,
           wd->wireless_dma_detail.dma_node_tail->data_length);
    return 0;
}

static int Wireless_dma_del_data(struct WirelessDeviceState *wd, struct Wireless_Data_Detail *data)
{
    printf("wireless device : remove data in dma\n");
    Wireless_dma_del_node(wd, data->dma_node);
    data->dma_node = NULL;
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

static int Wireless_dma_read_from_device(struct WirelessDeviceState *wd, struct Wireless_Data_Detail *data)
{
    printf("wireless device : wirte data to mem : host addr %08x data length %08x \n derection %d",
           data->host_addr, data->data_length, data->DMA_derection);
    if (data->DMA_derection != DMA_DEVICE_TO_MEMORY)
    {
        printf("wireless device : wirte data to mem : erro direction \n");
        return -5;
    }
    if (data->dma_node == null || data->dma_node->data_length == 0 || data->dma_node->data == null)
    {
        printf("wireless device : wirte data to mem : dma node null \n");
        return -1;
    }
    if (pci_dma_write(&wd->parent_obj, data->host_addr, data->dma_node->data, data->dma_node->data_length))
    {
        printf("wireless device : wirte data to mem : read from device err \n");
        return -2;
    }
    data->data_length = data->dma_node->data_length;
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
            Wireless_dma_del_node(wd, dma_detail->dma_node_list->next);
        }
        qemu_mutex_unlock(&wd->dma_access_mutex);
    }

stop:
    qemu_mutex_unlock(&wd->dma_access_mutex);
    return;
}

static void Wireless_Interrupt_Timer(void *opaque)
{
    struct TestDevicePciState *td = TESTDEV_PCI(opaque);
    printf("test pci : interrupt task irq func time %016lx \n", qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL));

    for(int i = 0 ; i < WIRELESS_DATA_DETAIL_BUF_SIZE; i++)
    {
        struct Wire
    }
}

static void addTask(struct TestDevicePciState *td, uint32_t taskType)
{
    td->irq_status |= taskType;
    if (td->irq_status)
    {
        printf("test pci : start prepare irq type : %08x qemu time : %016lx \n", td->irq_status, qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL));
        timer_mod(&td->irq_timer, qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL) + 100);
    }
    else
    {
        if (!msi_enabled(&td->parent_obj))
            pci_set_irq(&td->parent_obj, 0);
    }
}

static __uint64_t
testDevPci_read(void *opaque, hwaddr addr, unsigned size)
{
    printf("test pci read device : %p addr : %lu size : %u \n", opaque, addr, size);
    // struct TestDevicePciState* td = opaque; // 一般来说需要使用宏来进行类型检查，使用下面的方法:
    struct TestDevicePciState *td = TESTDEV_PCI(opaque);
    __uint64_t val = 0LL; // long long 全部置0

    switch (addr)
    {
    case 0x10: // 读取写入数据
        if (td->bufferPtr < size)
        {
            printf("test pci device : read data to long\n");
            memcpy(&val, td->buff, td->bufferPtr);
            return val;
        }
        memcpy(&val, td->buff + td->bufferPtr - size, size);
        return val;
    case 0x20: // 读取已写入的数据的长度
        printf("test pci device : read bufferPtr value \n");
        memcpy(&val, &td->bufferPtr, sizeof(td->bufferPtr));
        return val;
    case 0x30: // 读取中断状态
        printf("test pci device : read irq status \n");
        memcpy(&val, &td->irq_status, sizeof(td->irq_status));
        return val;
    case 0x40:
        printf("test pci read : msi msi_present(PCIDevice) %08x \n", msi_present(&td->parent_obj));
        printf("test pci read : msi pci msi flags %08x \n", pci_get_word(td->parent_obj.config + td->parent_obj.msi_cap + PCI_MSI_FLAGS));
        return msi_enabled(&td->parent_obj);
    default:
        printf("test pci device : read wrong register \n");
        return ~0LL;
    }
}

// @def size : 这里的 size 指的是 64 bit 的 val 中有效的数据长度, 以 char 为单位;
// 暂时默认不会写入超出 64bit 的数据, 毕竟Linux Kernel 中并没有暴露 64 bit 以上的数据写入方法

static void
testDevPci_write(void *opaque, hwaddr addr, __uint64_t val, unsigned size)
{
    printf("test pci write device : %p addr : %lu size : %u : %lx\n", opaque, addr, size, val);
    struct TestDevicePciState *td = TESTDEV_PCI(opaque);

    switch (addr)
    {
    case 0x10: // 写入数据
        if (td->bufferPtr + size >= TESTPCI_BUFF_SIZE)
        {
            printf("test pci device write to long \n");
            return;
        }
        memcpy(td->buff + td->bufferPtr, &val, size);
        td->bufferPtr += size;
        return;
    case 0x20: // 清空写入数据的长度
        printf("test pci clear buffer \n");
        td->bufferPtr = 0;
        return;
    case 0x30: // 启动一次中断
        printf("test pci start test irq \n");
        addTask(td, (uint32_t)val);
        return;
    default:
        printf("test pci write no use register\n");
        return;
    }
}

static __uint64_t
testDevPci_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    return testDevPci_read(opaque, addr, size);
}

static void
testDevPci_mmio_write(void *opaque, hwaddr addr, __uint64_t val, unsigned size)
{
    testDevPci_write(opaque, addr, val, size);
}

static const struct MemoryRegionOps testDevPci_mmio_ops = {
    .read = testDevPci_mmio_read,
    .write = testDevPci_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void testDevPci_realize(struct PCIDevice *pci_dev, struct Error **errp)
{
    struct TestDevicePciState *td = TESTDEV_PCI(pci_dev);

    // intx interrupt
    pci_config_set_interrupt_pin(pci_dev->config, 1);

    // msi interrupt
    if (msi_init(pci_dev, 0, 1, false, false, errp))
    {
        printf("test pci : msi interrupt malloc falut \n");
    }
    printf("test pci realize : msi msi_present(PCIDevice) %08x \n", msi_present(pci_dev));
    printf("test pci realize : msi pci msi flags %08x \n", pci_get_word(pci_dev->config + pci_dev->msi_cap + PCI_MSI_FLAGS));

    // 需长时间处理的延时任务
    timer_init_ms(&td->irq_timer, QEMU_CLOCK_VIRTUAL, Wireless_Interrupt_Timer, td);

    // io
    memory_region_init_io(&td->mmio, OBJECT(td), &testDevPci_mmio_ops,
                          pci_dev, "testdevpci-mmio", TESTPCI_BUFF_SIZE);
    pci_register_bar(pci_dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &td->mmio);
}

static void testDevicePci_exit(struct PCIDevice *pdev)
{
    struct TestDevicePciState *td = TESTDEV_PCI(pdev);
    timer_del(&td->irq_timer);
    msi_uninit(pdev);
    printf("test pci : exit \n");
}

// 非静态成员的初始化函数
static void testDevPci_instance_init(struct Object *obj)
{
    struct TestDevicePciState *td = TESTDEV_PCI(obj);

    td->bufferPtr = 0;
    td->irq_status = 0;

    // dma
    td->dma_mask = (1UL << 36) - 1;
    object_property_add_uint64_ptr(obj, "dma_mask",
                                   &td->dma_mask, OBJ_PROP_FLAG_READWRITE);

    printf("test pci : capabilities %08x \n", td->parent_obj.cap_present);
}

// 静态成员初始化函数
static void testDevPci_class_init(struct ObjectClass *class, void *data)
{
    struct DeviceClass *dc = DEVICE_CLASS(class);
    struct PCIDeviceClass *pci = PCI_DEVICE_CLASS(class);

    pci->realize = testDevPci_realize;
    pci->exit = testDevicePci_exit,
    pci->vendor_id = PCI_VENDOR_ID_QEMU;
    pci->device_id = 0x0721;
    pci->revision = 0x14;
    pci->class_id = PCI_CLASS_OTHERS;

    dc->desc = "polaris test pci device";
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

// 注册type info
static const struct TypeInfo testdevpci_info = {
    .name = TYPE_TESTDEV_PCI,
    .parent = TYPE_PCI_DEVICE,
    .instance_init = testDevPci_instance_init,
    .instance_size = sizeof(struct TestDevicePciState),
    // .class_size = sizeof(struct PCIDeviceClass),
    .class_init = testDevPci_class_init,
    .interfaces = (struct InterfaceInfo[]){
        {INTERFACE_CONVENTIONAL_PCI_DEVICE},
        {},
    },
};

static void testDevPci_register_types(void)
{
    printf("test test pci device\n");
    type_register_static(&testdevpci_info);
}

type_init(testDevPci_register_types);