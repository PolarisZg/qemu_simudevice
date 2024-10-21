#ifndef WIRELESS_SIMU_MAIN
#define WIRELESS_SIMU_MAIN

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

#include "wireless_hal.h"
#include "wireless_reg.h"

#define WIRELESS_SIMU_DEVICE_NAME "wirelesssimu"
#define WIRELESS_SIMU_DEVICE_DMA_MASK 32
#define WIRELESS_SIMU_DEVICE_ID 0x1145
#define WIRELESS_SIMU_REVISION 0x14

struct wireless_simu_device_state
{
    struct PCIDevice parent_obj;

    struct MemoryRegion mmio;

    u_int64_t dma_mask;

    struct wireless_simu_hal hal;
};

DECLARE_INSTANCE_CHECKER(struct wireless_simu_device_state,
                       WIRELESS_SIMU_OBJ,
                       WIRELESS_SIMU_DEVICE_NAME);

#endif