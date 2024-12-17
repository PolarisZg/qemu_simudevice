#include "wireless_simu.h"

void wireless_simu_write32(struct wireless_simu_device_state *wd, hwaddr addr, u_int32_t val)
{
    int ret = 0;

    /* hal_srng group */
    if ((addr & 0xffff0000) == HAL_TEST_SRNG_REG_GRP)
    {
        ret = wireless_hal_reg_handler(wd, addr, val);
        if (ret)
        {
            printf("%s : reg err %d \n", WIRELESS_SIMU_DEVICE_NAME, ret);
        }
    }

    if (addr == HAL_BASIC_REG(WIRELESS_REG_BASIC_IRQ_STATUS))
    {
        if (val == 0)
            wireless_simu_irq_lower(&wd->ws_irq);
    }

    if (addr == HAL_BASIC_REG(WIRELESS_REG_BASIC_IRQ_ENABLE))
    {
        if (val)
            wd->ws_irq.irq_enable = true;
        else
        {
            wd->ws_irq.irq_enable = false;
        }
    }
}

uint32_t wireless_simu_read32(struct wireless_simu_device_state *wd, hwaddr addr)
{
    if (addr == HAL_BASIC_REG(WIRELESS_REG_BASIC_IRQ_STATUS))
        return wireless_simu_irq_statu(wd->ws_irq);
    return 0;
}