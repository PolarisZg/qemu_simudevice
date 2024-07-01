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

#define HELLOPCI_BUFF_SIZE 0x100

// 定义设备实例
struct HelloDevicePciState
{
    struct PCIDevice parent_obj;

    MemoryRegion mmio;
    MemoryRegion pmio;
    __uint8_t buff[HELLOPCI_BUFF_SIZE]; // 保证和memcpy时的size对齐
};

#define TYPE_HELLODEV_PCI "hellodev-pci"

// 类型转换和检查
// #define HELLODEV_PCI(obj) OBJECT_CHECK(struct HelloDevicePciState, (obj), TYPE_HELLODEV_PCI)
DECLARE_INSTANCE_CHECKER(struct HelloDevicePciState, HELLODEV_PCI, TYPE_HELLODEV_PCI);

// mmio操作 read / write

static __uint64_t
helloDevPci_read(void *opaque, hwaddr addr, unsigned size)
{
    printf("hello pci read device : %p addr : %lu size : %u \n", opaque, addr, size);
    // struct HelloDevicePciState* hd = opaque; // 一般来说需要使用宏来进行类型检查，使用下面的方法:
    struct HelloDevicePciState *hd = HELLODEV_PCI(opaque);
    __uint64_t val = ~0LL; // long long 全部置1

    if (size > 8)
        return val; // 64是8个字节

    if (addr + size > HELLOPCI_BUFF_SIZE)
    {
        return val;
    }

    memcpy(&val, &hd->buff[addr], size);
    return val;
}

static void
helloDevPci_write(void *opaque, hwaddr addr, __uint64_t val, unsigned size)
{
    printf("hello pci write device : %p addr : %lu size : %u : %lx\n", opaque, addr, size, val);
    struct HelloDevicePciState *hd = HELLODEV_PCI(opaque);

    if (size > 8)
        return;
    if (addr + size > HELLOPCI_BUFF_SIZE)
        return;

    memcpy(&hd->buff[addr], &val, size);
}

static __uint64_t
helloDevPci_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    return helloDevPci_read(opaque, addr, size);
}

static __uint64_t
helloDevPci_pmio_read(void *opaque, hwaddr addr, unsigned size)
{
    return helloDevPci_read(opaque, addr, size);
}

static void
helloDevPci_mmio_write(void *opaque, hwaddr addr, __uint64_t val, unsigned size)
{
    helloDevPci_write(opaque, addr, val, size);
}

static void
helloDevPci_pmio_write(void *opaque, hwaddr addr, __uint64_t val, unsigned size)
{
    helloDevPci_write(opaque, addr, val, size);
}

static const struct MemoryRegionOps helloDevPci_mmio_ops = {
    .read = helloDevPci_mmio_read,
    .write = helloDevPci_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const struct MemoryRegionOps helloDevPci_pmio_ops = {
    .read = helloDevPci_pmio_read,
    .write = helloDevPci_pmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void helloDevPci_realize(struct PCIDevice *pci_dev, struct Error **errp)
{
    struct HelloDevicePciState *hd = HELLODEV_PCI(pci_dev);
    memory_region_init_io(&hd->mmio, OBJECT(hd), &helloDevPci_mmio_ops,
                          pci_dev, "hellodevpci-mmio", HELLOPCI_BUFF_SIZE);
    pci_register_bar(pci_dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &hd->mmio);
    memory_region_init_io(&hd->pmio, OBJECT(hd), &helloDevPci_pmio_ops,
                          pci_dev, "hellodevpci_pmio", HELLOPCI_BUFF_SIZE);
    pci_register_bar(pci_dev, 1, PCI_BASE_ADDRESS_SPACE_IO, &hd->pmio);
}

static void helloDevicePci_exit(struct PCIDevice *pdev)
{
    // 还没想好做什么
}

// 实例的初始化函数
static void helloDevPci_instance_init(struct Object *obj) {}

// 类初始化函数
static void helloDevPci_class_init(struct ObjectClass *class, void *data)
{
    struct DeviceClass *dc = DEVICE_CLASS(class);
    struct PCIDeviceClass *pci = PCI_DEVICE_CLASS(class);

    pci->realize = helloDevPci_realize;
    pci->exit = helloDevicePci_exit,
    pci->vendor_id = PCI_VENDOR_ID_QEMU;
    pci->device_id = 0x0721;
    pci->revision = 0x14;
    pci->class_id = PCI_CLASS_OTHERS;

    dc->desc = "polaris test pci device";
    set_bit(DEVICE_CATEGORY_NETWORK, dc->categories);
}

// 注册type info
static const struct TypeInfo hellodevpci_info = {
    .name = TYPE_HELLODEV_PCI,
    .parent = TYPE_PCI_DEVICE,
    .instance_init = helloDevPci_instance_init,
    .instance_size = sizeof(struct HelloDevicePciState),
    // .class_size = sizeof(struct PCIDeviceClass),
    .class_init = helloDevPci_class_init,
    .interfaces = (struct InterfaceInfo[]){
        {INTERFACE_CONVENTIONAL_PCI_DEVICE},
        {},
    },
};

static void helloDevPci_register_types(void) {
    printf("hello \n");
    type_register_static(&hellodevpci_info);
}

type_init(helloDevPci_register_types);