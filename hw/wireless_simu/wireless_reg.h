#ifndef WIRELESS_SIMU_REG
#define WIRELESS_SIMU_REG

#include "wireless_simu.h"

/* register */
#define HAL_SRNG_REG_R_GROUP_OFFSET(x) (x << 2)
#define HAL_SRNG_REG_R0_GROUP_SIZE 8
#define HAL_SRNG_REG_R2_GROUP_SIZE 2

#define SRNG_TEST_PIPE_COUNT_MAX 1 // ring 数量
#define HAL_TEST_SRNG_REG_GRP 0x00010000 // 基地址
#define HAL_TEST_SRNG_REG_GRP_R0 HAL_TEST_SRNG_REG_GRP
#define HAL_TEST_SRNG_REG_GRP_R0_SIZE ((SRNG_TEST_PIPE_COUNT_MAX * HAL_SRNG_REG_R0_GROUP_SIZE) << 2) 
#define HAL_TEST_SRNG_REG_GRP_R2 (HAL_TEST_SRNG_REG_GRP_R0 + HAL_TEST_SRNG_REG_GRP_R0_SIZE)
#define HAL_TEST_SRNG_REG_GRP_R2_SIZE ((SRNG_TEST_PIPE_COUNT_MAX * HAL_SRNG_REG_R2_GROUP_SIZE) << 2)

void wireless_simu_write32(struct wireless_simu_device_state *wd, hwaddr addr, u_int32_t val);

#endif