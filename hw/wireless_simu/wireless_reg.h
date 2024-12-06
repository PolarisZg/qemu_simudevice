#ifndef WIRELESS_SIMU_REG
#define WIRELESS_SIMU_REG

#include "wireless_simu.h"

/* register */

#define HAL_TEST_SRNG_REG_GRP 0x00010000 // 基地址
// srng->hwreg_base 的初始化，该处和硬件设计强相关，需特别注意寄存器的地址和功能的对应
/*
* |-------- 16bit --------|---- 8 bit ----|- 1 bit -|--- 5 bit ---|- 2 bit -|
* |          0001         |    ring_id    |  grp    |    reg      | 4 char  |*/

// 未确定寄存器组，高16bit为0000
#define REG_WIRELESS_SIMU_IRQ_STATUS 0x00000001

void wireless_simu_write32(struct wireless_simu_device_state *wd, hwaddr addr, u_int32_t val);

uint32_t wireless_simu_read32(struct wireless_simu_device_state *wd, hwaddr addr);

#endif