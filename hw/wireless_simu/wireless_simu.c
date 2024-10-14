#include "wireless_simu.h"

static void wireless_simu_mmio_write(void *opaque, hwaddr addr, u_int64_t val, unsigned size)
{
    printf("%s : %lx reg %u size %lx data \n", WIRELESS_SIMU_DEVICE_NAME, addr, size, val);
}

static u_int64_t wireless_simu_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    return 0;
}
static const struct MemoryRegionOps wireless_simu_mmio_ops = {
    .read = wireless_simu_mmio_read,
    .write = wireless_simu_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void wireless_simu_realize(struct PCIDevice *pci_dev, struct Error **errp)
{
    struct wireless_simu_device_state *wd = WIRELESS_SIMU_OBJ_GET_CLASS(pci_dev);
    
    /* mmio reg 初始化 */
    memory_region_init_io(&wd->mmio,
                          OBJECT(wd),
                          &wireless_simu_mmio_ops,
                          pci_dev,
                          "wireless_simu_mmio",
                          4096);
}

static void wireless_simu_exit(struct PCIDevice *pci_dev)
{
    struct wireless_simu_device_state *wd = WIRELESS_SIMU_OBJ_GET_CLASS(pci_dev);
    wd->dma_mask = 0;
}

static void wireless_simu_class_init(struct ObjectClass *class, void *data)
{
    struct DeviceClass *dc = DEVICE_CLASS(class);
    struct PCIDeviceClass *pci = PCI_DEVICE_CLASS(class);

    pci->realize = wireless_simu_realize;
    pci->exit = wireless_simu_exit;
    pci->vendor_id = PCI_VENDOR_ID_QEMU;
    pci->device_id = WIRELESS_SIMU_DEVICE_ID;
    pci->revision = WIRELESS_SIMU_REVISION;
    pci->class_id = PCI_CLASS_OTHERS;

    dc->desc = "wireless simu qemu device";
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static void wireless_simu_instance_init(struct Object *obj)
{
    struct wireless_simu_device_state *wd = WIRELESS_SIMU_OBJ_GET_CLASS(obj);
    wd->dma_mask = (1UL << WIRELESS_SIMU_DEVICE_DMA_MASK) - 1;
}

static const struct TypeInfo wireless_simu_type_info = {
    .name = WIRELESS_SIMU_DEVICE_NAME,
    .parent = TYPE_PCI_DEVICE,
    .instance_init = wireless_simu_instance_init,
    .instance_size = sizeof(struct wireless_simu_device_state),
    .class_init = wireless_simu_class_init,
    .interfaces = (struct InterfaceInfo[]){
        {INTERFACE_CONVENTIONAL_PCI_DEVICE},
        {},
    },
};

static void wireless_simu_register_types(void)
{
    printf("%s : wireless_simu register start \n", WIRELESS_SIMU_DEVICE_NAME);
    type_register_static(&wireless_simu_type_info);
}

type_init(wireless_simu_register_types);