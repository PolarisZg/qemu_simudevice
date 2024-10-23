#include "wireless_simu.h"

int wireless_simu_irq_init(struct wireless_simu_irq *ws_irq, PCIDevice *pdev, uint32_t irq_addr)
{
    if (ws_irq->pci_dev == NULL ||
        ws_irq->irq_addr == 0 ||
        irq_addr == 0 ||
        pdev == NULL ||
        ws_irq->msi_enable)
    {
        printf("%s : irq init err \n", WIRELESS_SIMU_DEVICE_NAME);
        return -EINVAL;
    }

    ws_irq->pci_dev = pdev;
    ws_irq->irq_addr = irq_addr;
    ws_irq->irq_status_val = 0;

    pci_config_set_interrupt_pin(ws_irq->pci_dev->config, 1);

    qemu_mutex_init(&ws_irq->irq_intx_mutex);

    // 不对msi进行配置
    ws_irq->msi_enable = false;

    ws_irq->irq_enable = true;

    return 0;
}

void wireless_simu_irq_deinit(struct wireless_simu_irq *ws_irq)
{
    ws_irq->irq_enable = false;

    qemu_mutex_unlock(&ws_irq->irq_intx_mutex);
    qemu_mutex_destroy(&ws_irq->irq_intx_mutex);

    ws_irq->irq_addr = 0;

    ws_irq->irq_status_val = 0;

    if (ws_irq->msi_enable)
    {
        msi_uninit(ws_irq->pci_dev);
        ws_irq->msi_enable = false;
    }

    ws_irq->pci_dev = NULL;

    return;
}