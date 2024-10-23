
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
            srng->ring_id = ring_id;
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
                srng->wd = wd;
                printf("%s : srng update %d src ring %d hp count \n",
                       WIRELESS_SIMU_DEVICE_NAME, ring_id, srng->u.src_ring.hp);
                g_thread_pool_push(wd->hal_srng_handle_pool, (void *)srng, &wd->hal_srng_handle_err);
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

static void *get_desc_from_mem(PCIDevice *pci_dev, dma_addr_t paddr, size_t size)
{
    void *desc = malloc(size);

    // 测试desc地址，非主要日志，需要被注释掉
    printf("%s : dma desc test vaddr %p \n", WIRELESS_SIMU_DEVICE_NAME, desc);

    uint32_t ret;
    ret = pci_dma_read(pci_dev, paddr, desc, size);
    if(ret){
        printf("%s : srng read from mem %d err\n", WIRELESS_SIMU_DEVICE_NAME, ret);
        return NULL;
    }

    // 测试desc地址，非主要日志，需要被注释掉
    printf("%s : dma desc test vaddr %p \n", WIRELESS_SIMU_DEVICE_NAME, desc);

    return desc;
}

static int desc_hal_test_sw2hw_handle(struct wireless_simu_device_state *wd, void *desc){
    struct hal_test_sw2hw *cmd = (struct hal_test_sw2hw *)desc;
    dma_addr_t data_paddr = cmd->buffer_addr_low | ((uint64_t)(cmd->buffer_addr_info & 0xff) << 32);
    size_t data_size = ((cmd->buffer_addr_info & 0xffff0000) >> 16);
    uint32_t write_index = cmd->write_index;

    printf("%s : src read data %016lx paddr %08lx size %08x write_index \n", WIRELESS_SIMU_DEVICE_NAME, data_paddr, data_size, write_index);

    return 0;
}

void wireless_hal_src_ring_tp(gpointer data, gpointer user_data){
    struct wireless_simu_device_state *wd = (struct wireless_simu_device_state *)user_data;
    struct hal_srng *srng = (struct hal_srng *)data;
    void *desc;

    int ret = 0;

    // 对srng加锁，删除锁操作还没做

    if(srng->wd != wd)
    {
        printf("%s : src srng tp update err \n", WIRELESS_SIMU_DEVICE_NAME);
        return;
    }

    printf("%s : src srng tp update %d ring id \n", WIRELESS_SIMU_DEVICE_NAME, srng->ring_id);

    while(srng->u.src_ring.tp < srng->u.src_ring.hp){
        desc = get_desc_from_mem(&wd->parent_obj, srng->ring_base_paddr, srng->entry_size);
        if(desc == NULL){
            printf("%s : src srng read from mem err \n",WIRELESS_SIMU_DEVICE_NAME);
            return;
        }

        srng->u.src_ring.tp = (srng->u.src_ring.tp + srng->entry_size) % srng->ring_size;
        
        /* 对 desc 进行处理
         * 正常的思路是为每一个srng挂上一个desc处理函数，直接拉起处理函数进行处理；但是那样需要的辅助 static 太多了
         * 这里暂时根据desc的size选择拉起的desc处理函数
         */

        if(srng->entry_size == sizeof(struct hal_test_sw2hw)){
            ret = desc_hal_test_sw2hw_handle(wd, desc);
            if(ret){
                printf("%s : %d ring read err \n", WIRELESS_SIMU_DEVICE_NAME, srng->ring_id);
            }
            pci_dma_write(&wd->parent_obj, srng->u.src_ring.tp_paddr, &srng->u.src_ring.tp, sizeof(srng->u.src_ring.tp));
            continue;
        }
        
    }
}