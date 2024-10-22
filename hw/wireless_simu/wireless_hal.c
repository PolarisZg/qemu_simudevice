
#include "wireless_simu.h"

#define isInInterval(val, left, right) ((right >= left) && (val >= left) && (val <= right)) // 判断val是否落在[left, right]区间内

static void wireless_simu_hal_srng_dir_set(int ring_id, struct hal_srng *srng)
{
    // 本身是一个静态的配置，每个ring在设计之初就确定好方向无法修改；
    // 该方向与driver中标记一致，driver中为src在device中也被标记为src

    if (isInInterval(ring_id, HAL_SRNG_RING_ID_TEST_SW2HW, HAL_SRNG_RING_ID_TEST_SW2HW))
        srng->ring_dir = HAL_SRNG_DIR_SRC;
    else
        srng->ring_dir = HAL_SRNG_DIR_DST;
}

int wireless_hal_reg_handler(struct wireless_simu_device_state *wd, hwaddr addr, uint32_t val)
{
    // srng->hwreg_base 的初始化，该处和硬件设计强相关，需特别注意寄存器的地址和功能的对应
    /*
     * |-------- 16bit --------|---- 8 bit ----|- 1 bit -|--- 5 bit ---|- 2 bit -|
     * |          0001         |    ring_id    |  grp    |    reg      | 4 char  |*/

    int ring_id = ((addr >> 8) & (0xff));
    int grp_count = ((addr >> 7) & (0x01));
    int reg_offset = ((addr >> 2) & (0x1f));
    struct hal_srng *srng;

    if (ring_id >= HAL_SRNG_RING_ID_MAX)
    {
        return -EINVAL;
    }

    srng = &wd->hal.srng_list[ring_id];

    if (grp_count == HAL_SRNG_REG_GRP_R0)
    {
        printf("%s : srng set %d ring \n", WIRELESS_SIMU_DEVICE_NAME, ring_id);

        switch (reg_offset)
        {
        case 0:
            srng->ring_base_paddr = val;
            wireless_simu_hal_srng_dir_set(ring_id, srng);
            printf("%s : srng set %d ring_id %d type \n", WIRELESS_SIMU_DEVICE_NAME, ring_id, srng->ring_dir);
            break;
        case 1:
            srng->ring_base_paddr |= (((uint64_t)(val) & 0xff) << 32); // #define HAL_TCL1_RING_BASE_MSB_RING_BASE_ADDR_MSB GENMASK(7, 0)
            srng->ring_size = (((uint64_t)(val) & 0xfffff00) >> 8);    // 单位 32 bit #define HAL_TCL1_RING_BASE_MSB_RING_SIZE GENMASK(27, 8)
            printf("%s : srng set %d ring %016lx ring_base_addr %08x ring_size\n",
                   WIRELESS_SIMU_DEVICE_NAME, ring_id, srng->ring_base_paddr, srng->ring_size);
            break;
        case 2:
            srng->entry_size = (val & 0xff); // #define HAL_REO1_RING_ID_ENTRY_SIZE GENMASK(7, 0)
            if (srng->entry_size == 0)
            {
                return -EINVAL;
            }
            srng->num_entries = (srng->ring_size / srng->entry_size);
            printf("%s : srng set %d ring %08x entry_size %08x num_entries \n",
                   WIRELESS_SIMU_DEVICE_NAME, ring_id, srng->entry_size, srng->num_entries);
            break;
        case 3:
            srng->intr_timer_thres_us = ((val & 0xffff0000) >> 16); // #define HAL_TCL1_RING_CONSR_INT_SETUP_IX0_INTR_TMR_THOLD GENMASK(31, 16)
            if (srng->entry_size == 0)
            {
                return -EINVAL;
            }
            srng->intr_batch_cntr_thres_entries = ((val & 0x7fff) / srng->entry_size); // #define HAL_TCL1_RING_CONSR_INT_SETUP_IX0_BATCH_COUNTER_THOLD GENMASK(14, 0)
            printf("%s : srng set %d ring %08x intr_time %08x intr_cntr_ent \n",
                   WIRELESS_SIMU_DEVICE_NAME, ring_id, srng->intr_timer_thres_us, srng->intr_batch_cntr_thres_entries);
            break;
        case 4:
            if (srng->ring_dir == HAL_SRNG_DIR_SRC)
            {
                srng->u.src_ring.low_threshold = (val & 0xffff); // #define HAL_TCL1_RING_CONSR_INT_SETUP_IX1_LOW_THOLD GENMASK(15, 0)
                printf("%s : srng set %d ring %d src low_threshold\n",
                       WIRELESS_SIMU_DEVICE_NAME, ring_id, srng->u.src_ring.low_threshold);
            }
            else
            {
                printf("%s : srng set %d ring no dst logic has\n", WIRELESS_SIMU_DEVICE_NAME, ring_id);
            }
            break;
        case 5:
            if (srng->ring_dir == HAL_SRNG_DIR_SRC)
            {
                srng->u.src_ring.tp_paddr = (((uint64_t)val) & 0xffffffff);
            }
            else
            {
                printf("%s : srng set %d ring no dst logic has\n", WIRELESS_SIMU_DEVICE_NAME, ring_id);
            }
            break;
        case 6:
            if (srng->ring_dir == HAL_SRNG_DIR_SRC)
            {
                srng->u.src_ring.tp_paddr |= (((uint64_t)val) << 32); // hw 更新 tp 时直接去操作dma
                srng->u.src_ring.tp = 0;
                printf("%s : srng set %d ring_id src ring %016lx tp_paddr \n",
                       WIRELESS_SIMU_DEVICE_NAME, ring_id, srng->u.src_ring.tp_paddr);
            }
            else
            {
                printf("%s : srng set %d ring no dst logic has\n", WIRELESS_SIMU_DEVICE_NAME, ring_id);
            }
            break;
        case 7:
            srng->flags = val;
            printf("%s : srng set %d ring_id %08x flag \n", WIRELESS_SIMU_DEVICE_NAME, ring_id, srng->flags);
            break;
        default:
            printf("%s : srng set err reg num %d \n", WIRELESS_SIMU_DEVICE_NAME, reg_offset);
            return -EINVAL;
        }
    }
    else if (grp_count == HAL_SRNG_REG_GRP_R2)
    {
        printf("%s : srng update %d ring \n", WIRELESS_SIMU_DEVICE_NAME, ring_id);
        switch (reg_offset)
        {
        case 0:
            // 在src_ring中，0号寄存器用于sw hp的更新
            if (srng->ring_dir == HAL_SRNG_DIR_SRC)
            {
                srng->u.src_ring.hp = val;
                printf("%s : srng update %d src ring %d hp count \n",
                       WIRELESS_SIMU_DEVICE_NAME, ring_id, srng->u.src_ring.hp);
            }
            else
            {
                printf("%s : srng set %d ring no dst logic has\n", WIRELESS_SIMU_DEVICE_NAME, ring_id);
            }
            break;
        case 1:
            // 1 号寄存器暂时没找到用途
        default:
            break;
        }
    }
    else
    {
        return -EINVAL;
    }

    return 0;
}