#ifndef WIRELESS_SIMU_IRQ
#define WIRELESS_SIMU_IRQ

#include "wireless_simu.h"

struct wireless_simu_irq
{
    uint32_t irq_addr;
    uint32_t irq_status_val;
    QemuMutex irq_intx_mutex;
    PCIDevice *pci_dev;
    bool msi_enable;
    bool irq_enable;
};

enum WIRELESS_SIMU_ENUM_IRQ_STATUS
{
    WIRELESS_SIMU_IRQ_STATU_START = 0,
    WIRELESS_SIMU_IRQ_STATU_SRNG_DST_DMA_TEST_RING_0,
};

// 仅支持intx中断初始化
int wireless_simu_irq_init(struct wireless_simu_irq *ws_irq, PCIDevice *pdev, uint32_t irq_addr);

// 删除中断
void wireless_simu_irq_deinit(struct wireless_simu_irq *ws_irq);

// 拉起中断
static inline void wireless_simu_irq_raise(struct wireless_simu_irq *ws_irq, uint32_t statu)
{
    
    if (ws_irq->irq_enable)
    {
        qemu_mutex_lock(&ws_irq->irq_intx_mutex);
        ws_irq->irq_status_val = statu;
        pci_set_irq(ws_irq->pci_dev, 1);
    }
}

// 清中断
static inline void wireless_simu_irq_lower(struct wireless_simu_irq *ws_irq)
{
    if (ws_irq->irq_enable)
    {
        ws_irq->irq_status_val = 0;
        pci_set_irq(ws_irq->pci_dev, 0);
    }
    qemu_mutex_unlock(&ws_irq->irq_intx_mutex);
}

// host读中断信息
static inline uint32_t wireless_simu_irq_statu(struct wireless_simu_irq ws_irq)
{
    return ws_irq.irq_status_val;
}

#endif