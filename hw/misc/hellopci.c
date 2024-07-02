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

#define TESTPCI_BUFF_SIZE 4096

// 定义设备实例
struct TestDevicePciState
{
    struct PCIDevice parent_obj;

    MemoryRegion mmio;
    MemoryRegion pmio;
    char buff[TESTPCI_BUFF_SIZE];

    // dma 相关，包括dma_mask 中断 锁

    u_int64_t dma_mask;
    u_int32_t bufferPtr;
};

#define TYPE_TESTDEV_PCI "hellodev-pci"

// 类型转换和检查
// #define TESTDEV_PCI(obj) OBJECT_CHECK(struct TestDevicePciState, (obj), TYPE_TESTDEV_PCI)
DECLARE_INSTANCE_CHECKER(struct TestDevicePciState, TESTDEV_PCI, TYPE_TESTDEV_PCI);

// mmio操作 read / write

static __uint64_t
testDevPci_read(void *opaque, hwaddr addr, unsigned size)
{
    printf("test pci read device : %p addr : %lu size : %u \n", opaque, addr, size);
    // struct TestDevicePciState* td = opaque; // 一般来说需要使用宏来进行类型检查，使用下面的方法:
    struct TestDevicePciState *td = TESTDEV_PCI(opaque);
    __uint64_t val = 0LL; // long long 全部置0

    switch (addr)
    {
    case 0x10:
        if (td->bufferPtr < size)
        {
            printf("test pci device : read data to long\n");
            memcpy(&val, td->buff, td->bufferPtr);
            return val;
        }
        memcpy(&val, td->buff + td->bufferPtr - size, size);
        return val;
    case 0x20:
        printf("test pci device : read bufferPtr value \n");
        memcpy(&val, &td->bufferPtr, sizeof(td->bufferPtr));
        return val;
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
    case 0x10:
        if (td->bufferPtr + size >= TESTPCI_BUFF_SIZE)
        {
            printf("test pci device write to long \n");
            return;
        }
        memcpy(td->buff + td->bufferPtr, &val, size);
        td->bufferPtr += size;
        return;
    case 0x20:
        printf("test pci clear buffer \n");
        td->bufferPtr = 0;
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

static __uint64_t
testDevPci_pmio_read(void *opaque, hwaddr addr, unsigned size)
{
    return testDevPci_read(opaque, addr, size);
}

static void
testDevPci_mmio_write(void *opaque, hwaddr addr, __uint64_t val, unsigned size)
{
    testDevPci_write(opaque, addr, val, size);
}

static void
testDevPci_pmio_write(void *opaque, hwaddr addr, __uint64_t val, unsigned size)
{
    testDevPci_write(opaque, addr, val, size);
}

static const struct MemoryRegionOps testDevPci_mmio_ops = {
    .read = testDevPci_mmio_read,
    .write = testDevPci_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const struct MemoryRegionOps testDevPci_pmio_ops = {
    .read = testDevPci_pmio_read,
    .write = testDevPci_pmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void testDevPci_realize(struct PCIDevice *pci_dev, struct Error **errp)
{
    struct TestDevicePciState *td = TESTDEV_PCI(pci_dev);

    // io
    memory_region_init_io(&td->mmio, OBJECT(td), &testDevPci_mmio_ops,
                          pci_dev, "testdevpci-mmio", TESTPCI_BUFF_SIZE);
    pci_register_bar(pci_dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &td->mmio);
    memory_region_init_io(&td->pmio, OBJECT(td), &testDevPci_pmio_ops,
                          pci_dev, "testdevpci_pmio", TESTPCI_BUFF_SIZE);
    pci_register_bar(pci_dev, 1, PCI_BASE_ADDRESS_SPACE_IO, &td->pmio);
}

static void testDevicePci_exit(struct PCIDevice *pdev)
{
    // 还没想好做什么
}

// 非静态成员的初始化函数
static void testDevPci_instance_init(struct Object *obj)
{
    struct TestDevicePciState *td = TESTDEV_PCI(obj);

    //
    td->bufferPtr = 0;

    // dma
    td->dma_mask = (1UL << 28) - 1;
    object_property_add_uint64_ptr(obj, "dma_mask",
                                   &td->dma_mask, OBJ_PROP_FLAG_READWRITE);
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