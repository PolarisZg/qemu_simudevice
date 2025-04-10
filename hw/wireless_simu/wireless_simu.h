#ifndef WIRELESS_SIMU_MAIN
#define WIRELESS_SIMU_MAIN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <pthread.h>

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
#include "wireless_irq.h"
#include "wireless_ce.h"
#include "wireless_sk_buff.h"
#include "wireless_num.h"
#include "wireless_wmi.h"
#include "wireless_txrx.h"

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
    int ce_count_num;
    struct copy_engine ce_group[WIRELESS_SIMU_CE_COUNT];

    GThreadPool *hal_srng_handle_pool;
    GError *hal_srng_handle_err;

    // irq module
    struct wireless_simu_irq ws_irq;
};

DECLARE_INSTANCE_CHECKER(struct wireless_simu_device_state,
                       WIRELESS_SIMU_OBJ,
                       WIRELESS_SIMU_DEVICE_NAME);

#endif