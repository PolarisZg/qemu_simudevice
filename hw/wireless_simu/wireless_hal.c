#include "wireless_simu.h"

static const struct hal_srng_config hw_srng_config_template[] = {
    {
        /* HAL_TEST_SRNG  与 enum hal_ring_type 对应*/
        .start_ring_id = HAL_SRNG_RING_ID_TEST_SW2HW,
        .max_rings = 1,
        .entry_size = sizeof(struct hal_test_sw2hw) >> 2, // 右移 2 位代表以32bit为单位
        .lmac_ring = false,
        .ring_dir = HAL_SRNG_DIR_SRC,
        .max_size = HAL_TEST_SW2HW_SIZE,
    },
    {
        /* HAL_TEST_SRNG_DST */
        .start_ring_id = HAL_SRNG_RING_ID_TEST_DST,
        .max_rings = 1,
        .entry_size = sizeof(struct hal_test_dst) >> 2,
        .lmac_ring = false,
        .ring_dir = HAL_SRNG_DIR_SRC,
        .max_size = HAL_TEST_SW2HW_SIZE,
        .hal_srng_handler = ce_dst_ring_handler,
    },
    {
        /* HAL_TEST_SRNG_DST_STATUS 和上一个ring配套使用*/
        .start_ring_id = HAL_SRNG_RING_ID_TEST_DST_STATUS,
        .max_rings = 1,
        .entry_size = sizeof(struct hal_test_dst_status) >> 2,
        .lmac_ring = false,
        .ring_dir = HAL_SRNG_DIR_DST,
        .max_size = HAL_TEST_SW2HW_SIZE,
    },
    {
        /* CE_SRC */
        .start_ring_id = HAL_SRNG_RING_ID_CE0_SRC,
        .max_rings = 12,
        .entry_size = sizeof(struct hal_ce_srng_src_desc) >> 2,
        .lmac_ring = false,
        .ring_dir = HAL_SRNG_DIR_SRC,
        .max_size = HAL_CE_SRC_RING_BASE_MSB_RING_SIZE,
    },
    {
        /* CE_DST */
        .start_ring_id = HAL_SRNG_RING_ID_CE0_DST,
        .max_rings = 12,
        .entry_size = sizeof(struct hal_ce_srng_dest_desc) >> 2,
        .lmac_ring = false,
        .ring_dir = HAL_SRNG_DIR_SRC,
        .max_size = HAL_CE_DST_RING_BASE_MSB_RING_SIZE,
    },
    {
        /* CE_DST_STATUS */
        .start_ring_id = HAL_SRNG_RING_ID_CE0_DST_STATUS,
        .max_rings = 12,
        .entry_size = sizeof(struct hal_ce_srng_dst_status_desc) >> 2,
        .lmac_ring = false,
        .ring_dir = HAL_SRNG_DIR_DST,
        .max_size = HAL_CE_DST_STATUS_RING_BASE_MSB_RING_SIZE,
    },
};

#define isInInterval(val, left, right) ((right >= left) && (val >= left) && (val <= right)) // 判断val是否落在[left, right]区间内

static void wireless_simu_hal_srng_dir_set(int ring_id, struct hal_srng *srng)
{
    // 本身是一个静态的配置，每个ring在设计之初就确定好方向无法修改；
    // 该方向与driver中标记一致，driver中为src在device中也被标记为src

    switch (ring_id)
    {
    case HAL_SRNG_RING_ID_TEST_SW2HW:
        srng->ring_dir = HAL_SRNG_DIR_SRC;
        break;
    case HAL_SRNG_RING_ID_TEST_DST_STATUS ... HAL_SRNG_RING_ID_TEST_DST_STATUS + 6:
        srng->ring_dir = HAL_SRNG_DIR_DST;
        break;
    case HAL_SRNG_RING_ID_TEST_DST ... HAL_SRNG_RING_ID_TEST_DST + 6:
        srng->ring_dir = HAL_SRNG_DIR_SRC;
        break;
    case HAL_SRNG_RING_ID_CE0_SRC ... HAL_SRNG_RING_ID_CE0_SRC + 11:
        srng->ring_dir = HAL_SRNG_DIR_SRC;
        break;
    case HAL_SRNG_RING_ID_CE0_DST ... HAL_SRNG_RING_ID_CE0_DST + 11:
        srng->ring_dir = HAL_SRNG_DIR_SRC;
        break;
    case HAL_SRNG_RING_ID_CE0_DST_STATUS ... HAL_SRNG_RING_ID_CE0_DST_STATUS + 11:
        srng->ring_dir = HAL_SRNG_DIR_DST;
        break;
    default:
        printf("%s : ring id %d err \n", WIRELESS_SIMU_DEVICE_NAME, ring_id);
        break;
    }
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
        // printf("%s : srng set %d ring \n", WIRELESS_SIMU_DEVICE_NAME, ring_id);

        switch (reg_offset)
        {
        case 0:
            srng->ring_base_paddr = val;
            srng->ring_id = ring_id;
            wireless_simu_hal_srng_dir_set(ring_id, srng);
            qemu_mutex_init(&srng->lock);
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
                srng->u.dst_ring.max_buffer_length = (val & 0xffff); // #define HAL_TCL1_RING_CONSR_INT_SETUP_IX1_LOW_THOLD GENMASK(15, 0)
                printf("%s : srng set %d ring %d dst max_buffer_length\n",
                       WIRELESS_SIMU_DEVICE_NAME, ring_id, srng->u.dst_ring.max_buffer_length);
            }
            break;
        case 5:
            if (srng->ring_dir == HAL_SRNG_DIR_SRC)
            {
                srng->u.src_ring.tp_paddr = (((uint64_t)val) & 0xffffffff);
            }
            else
            {
                srng->u.dst_ring.hp_paddr = (((uint64_t)val) & 0xffffffff);
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
                srng->u.dst_ring.hp_paddr |= (((uint64_t)val) << 32); // hw 更新 hp 时直接去操作dma
                srng->u.dst_ring.hp = 0;
                printf("%s : srng set %d ring_id dst ring %016lx hp_paddr \n",
                       WIRELESS_SIMU_DEVICE_NAME, ring_id, srng->u.dst_ring.hp_paddr);
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
        // printf("%s : srng update %d ring \n", WIRELESS_SIMU_DEVICE_NAME, ring_id);
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
                srng->u.dst_ring.tp = val;
                srng->wd = wd;

                /* dst 方向的ring更新无需进行处理 */
                printf("%s : dst ring id %08x tp %08x \n", WIRELESS_SIMU_DEVICE_NAME, srng->ring_id, srng->u.dst_ring.tp);
            }
            break;
        case 1:
            
            break;
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

static uint32_t *get_desc_from_mem(PCIDevice *pci_dev, dma_addr_t paddr, size_t size)
{
    uint32_t *desc = malloc(size);

    // 测试desc地址，非主要日志，需要被注释掉
    /* 经过测试，下方的dma_read函数需要传入一个足够大小的desc来承接数据，因此需要上方的desc进行malloc
     * 下方dma_read运行结束后，desc的起始逻辑地址不发生变动
     */
    // printf("%s : dma desc test vaddr %p \n", WIRELESS_SIMU_DEVICE_NAME, desc);

    uint32_t ret;
    ret = pci_dma_read(pci_dev, paddr, desc, size);
    if (ret)
    {
        printf("%s : srng read from mem %d err\n", WIRELESS_SIMU_DEVICE_NAME, ret);
        return NULL;
    }

    // 测试desc地址，非主要日志，需要被注释掉
    // printf("%s : dma desc test vaddr %p \n", WIRELESS_SIMU_DEVICE_NAME, desc);

    return desc;
}

static int desc_hal_test_sw2hw_handle(struct wireless_simu_device_state *wd, void *desc)
{
    struct hal_test_sw2hw *cmd = (struct hal_test_sw2hw *)desc;
    dma_addr_t data_paddr = cmd->buffer_addr_low | ((uint64_t)(cmd->buffer_addr_info & 0xff) << 32);
    size_t data_size = ((cmd->buffer_addr_info & 0xffff0000) >> 16);
    uint32_t write_index = cmd->write_index;

    printf("%s : src read data %016lx paddr %08lx size %08x write_index \n", WIRELESS_SIMU_DEVICE_NAME, data_paddr, data_size, write_index);

    // 数据 loop
    void *data = (void *)get_desc_from_mem(&wd->parent_obj, data_paddr, data_size);
    if (!data)
        return -EIO;
    uint64_t head = READ_64BIT_FROM_ADDR(data);
    uint64_t tail = READ_64BIT_FROM_ADDR(data + data_size - 8);
    printf("%s : src read data %016lx head %016lx tail \n", WIRELESS_SIMU_DEVICE_NAME, head, tail);

    wireless_simu_ce_post_data(wd, data, data_size);

    /* 不管是否成功失败, free掉空间  */
    free(data);

    return 0;
}

static int hal_srng_ring_ce_src_wmi_handler(struct wireless_simu_device_state *wd, void *desc)
{
    struct hal_ce_srng_src_desc *ce_src_desc = (struct hal_ce_srng_src_desc *)desc;

    /* -- ce ring 数据帧 -- */
    dma_addr_t data_paddr = ce_src_desc->buffer_addr_low | ((uint64_t)(ce_src_desc->buffer_addr_info & 0xff) << 32);
    uint32_t data_size = ((ce_src_desc->buffer_addr_info & 0xffff0000) >> 16);
    printf("%s : ce srng src desc data %016lx paddr %08x size %08x flag \n", WIRELESS_SIMU_DEVICE_NAME, data_paddr, data_size, ce_src_desc->flags);

    /* -- 从 ce ring 中抽取 skb */
    void *data = (void *)get_desc_from_mem(&wd->parent_obj, data_paddr, data_size);
    if (!data)
        return -EIO;

    /* -- skb 中 头部先是 htc 部分 */
    struct wireless_htc_hdr *htc_hdr = (struct wireless_htc_hdr *)data;
    uint32_t skb_len_no_htc = (htc_hdr->htc_info & 0xffff0000) >> 16;
    printf("%s : ce srng src htc skb_len_no_htc %08x \n", WIRELESS_SIMU_DEVICE_NAME, skb_len_no_htc);

    /* -- htc 后面是 wmi_cmd 部分 */
    struct wmi_cmd_hdr *wmi_hdr = (struct wmi_cmd_hdr *)((void *)data + sizeof(struct wireless_htc_hdr));
    uint32_t wmi_cmd = wmi_hdr->cmd_id;
    printf("%s : ce srng src wmi cmd %08x \n", WIRELESS_SIMU_DEVICE_NAME, wmi_cmd);

    switch (wmi_cmd)
    {
    case WMI_MGMT_TX_SEND_CMDID:
        printf("%s : ce srng src wmi_mgmt_send_cmd \n", WIRELESS_SIMU_DEVICE_NAME);
        struct wmi_mgmt_send_cmd *cmd = (struct wmi_mgmt_send_cmd *)((void *)wmi_hdr + sizeof(struct wmi_cmd_hdr));
        wireless_simu_wmi_mgmt_send(wd, cmd);
        break;
    }

    free(data);
    return 0;
}

static int hal_srng_ring_ce_src_handler_default(struct wireless_simu_device_state *wd, void *desc, int ce_id)
{
    int ret = 0;

    printf("%s : this is openwifi tx code ce_id %d \n", WIRELESS_SIMU_DEVICE_NAME, ce_id);
    struct hal_ce_srng_src_desc *ce_src_desc = (struct hal_ce_srng_src_desc *)desc;
    dma_addr_t data_paddr = ce_src_desc->buffer_addr_low | ((uint64_t)(ce_src_desc->buffer_addr_info & 0xff) << 32);
    uint32_t data_size = ((ce_src_desc->buffer_addr_info & 0xffff0000) >> 16);

    /* -- 从 ce ring 中抽取 skb */
    void *data = (void *)get_desc_from_mem(&wd->parent_obj, data_paddr, data_size);
    if (!data)
        return -EIO;

    printf("%s : ce srng src desc data %016lx paddr %08x size %08x flag \n", WIRELESS_SIMU_DEVICE_NAME, data_paddr, data_size, ce_src_desc->flags);

    wireless_simu_openwifi_mgmt_send(wd, data, data_size);

    free(data);

    return ret;
}

static int hal_srng_ring_ce_src_handler(struct wireless_simu_device_state *wd, void *desc, int ce_id)
{
    int ret = 0;
    switch (ce_id)
    {
    case 0:
        ret = hal_srng_ring_ce_src_wmi_handler(wd, desc);
        break;
    default:
        ret = hal_srng_ring_ce_src_handler_default(wd, desc, ce_id);
    }

    return ret;
}

void wireless_hal_src_ring_tp(gpointer data, gpointer user_data)
{
    /* 该函数中所有的 << 2 和 >> 2 都是为了去对driver中定义的以 32bit 为单位去计算的数据长度等参数 */

    struct wireless_simu_device_state *wd = (struct wireless_simu_device_state *)user_data;
    struct hal_srng *srng = (struct hal_srng *)data;
    uint32_t *desc;

    int ret = 0;

    // printf("%s : hal src ring tp thread \n", WIRELESS_SIMU_DEVICE_NAME);

    if (srng->hal_srng_handler && srng->user_data)
    {
        srng->hal_srng_handler(srng->user_data);
        // printf("%s : hal srng %d has handler \n", WIRELESS_SIMU_DEVICE_NAME, srng->ring_id);
        return;
    }

    printf("%s : src ring id %d no srng handler \n", WIRELESS_SIMU_DEVICE_NAME, srng->ring_id);

    // 对srng加锁
    if(!qemu_mutex_trylock(&srng->lock))
        return;
    // qemu_mutex_lock(&srng->lock);

    if (srng->wd != wd)
    {
        printf("%s : src ring tp update err \n", WIRELESS_SIMU_DEVICE_NAME);
        return;
    }

    while (srng->u.src_ring.tp != srng->u.src_ring.hp)
    {
        printf("%s : src ring tp update %d ring id \n", WIRELESS_SIMU_DEVICE_NAME, srng->ring_id);
        
        desc = get_desc_from_mem(&wd->parent_obj, srng->ring_base_paddr + (srng->u.src_ring.tp << 2), srng->entry_size << 2);
        if (desc == NULL)
        {
            printf("%s : src srng read from mem err \n", WIRELESS_SIMU_DEVICE_NAME);
            return;
        }

        /* todo : 如果上级模块的ring_size是一个2的指数，即只有一位为 1 其他位全为 0 的数，
         * 那么这里可以去掉取模运算，改为 & (ring_size - 1) */

        srng->u.src_ring.tp = (srng->u.src_ring.tp + srng->entry_size) % srng->ring_size;

        /* 对 desc 进行处理
         * 正常的思路是为每一个srng挂上一个desc处理函数，直接拉起处理函数进行处理；但是那样需要的辅助 static 太多了
         * 这里暂时根据desc的size选择拉起的desc处理函数
         */
        switch (srng->ring_id)
        {
        case HAL_SRNG_RING_ID_TEST_SW2HW:
            ret = desc_hal_test_sw2hw_handle(wd, desc);
            if (ret)
            {
                printf("%s : %d ring read err \n", WIRELESS_SIMU_DEVICE_NAME, srng->ring_id);
            }
            pci_dma_write(&wd->parent_obj, srng->u.src_ring.tp_paddr, &srng->u.src_ring.tp, sizeof(srng->u.src_ring.tp));
            break;
        case HAL_SRNG_RING_ID_TEST_DST:
            // ret = desc_hal_test_dst_handle(wd, desc);
            break;
        case HAL_SRNG_RING_ID_CE0_SRC ... HAL_SRNG_RING_ID_CE0_SRC + 11:
            // 这里暂时应该只会发送一些mgmt数据, 所以简单把数据抽出来
            hal_srng_ring_ce_src_handler(wd, desc, srng->ring_id - HAL_SRNG_RING_ID_CE0_SRC);
            // 把写ring_buffer的 tp 放在前面, 就能保证中断发出去之前就已经写好数据了
            pci_dma_write(&wd->parent_obj, srng->u.src_ring.tp_paddr, &srng->u.src_ring.tp, sizeof(srng->u.src_ring.tp));
            /* send 完毕, 该使用中断去通知驱动 */
            wireless_simu_irq_raise(&wd->ws_irq, WIRELESS_SIMU_IRQ_STATUS_MGMT_TX_END + srng->ring_id - HAL_SRNG_RING_ID_CE0_SRC);
            break;
        default:
            break;
        }

        // for (int count = 0; count < srng->entry_size; count++)
        // {
        //     printf("%s : src tp update desc data %d : %08x \n", WIRELESS_SIMU_DEVICE_NAME, count, *(uint32_t *)(desc + (count)));
        // }

        // if (srng->entry_size == (sizeof(struct hal_test_sw2hw) >> 2))
        // { // 上方传过来的entrysize以4char为单位
        //     ret = desc_hal_test_sw2hw_handle(wd, desc);
        //     if (ret)
        //     {
        //         printf("%s : %d ring read err \n", WIRELESS_SIMU_DEVICE_NAME, srng->ring_id);
        //     }
        //     pci_dma_write(&wd->parent_obj, srng->u.src_ring.tp_paddr, &srng->u.src_ring.tp, sizeof(srng->u.src_ring.tp));
        //     continue;
        // }
    }

    qemu_mutex_unlock(&srng->lock); // todo : 删除锁还没有做
}

int wireless_hal_srng_setup(struct wireless_simu_device_state *wd,
                            enum hal_ring_type type,
                            int ring_num, int mac_id,
                            struct hal_srng_params *params)
{
    int ret = 0;
    struct hal_srng *srng;

    ret = hw_srng_config_template[type].start_ring_id + ring_num;

    srng = &wd->hal.srng_list[ret];

    if (hw_srng_config_template[type].hal_srng_handler)
        srng->hal_srng_handler = hw_srng_config_template[type].hal_srng_handler;
    srng->user_data = params->user_data;

    /* 和驱动中不一样，qemu设备中很多srng的参数需要等待驱动端的写寄存器配置 */

    return ret;
}

int wireless_hal_srng_read_src_ring(struct wireless_simu_device_state *wd, struct hal_srng *srng, uint32_t **ans)
{
    // printf("%s : get desc from srng %d \n", WIRELESS_SIMU_DEVICE_NAME, srng->ring_id);
    int ret = 0;
    uint32_t *desc;
    if (srng->u.src_ring.tp != srng->u.src_ring.hp)
    {
        desc = get_desc_from_mem(&wd->parent_obj, srng->ring_base_paddr + (srng->u.src_ring.tp << 2), srng->entry_size << 2);
        if (desc == NULL)
        {
            printf("%s : src srng read from mem err \n", WIRELESS_SIMU_DEVICE_NAME);
            ret = -ENOBUFS;
            goto exit;
        }
        *ans = desc;
        srng->u.src_ring.tp = (srng->u.src_ring.tp + srng->entry_size) % srng->ring_size;
        pci_dma_write(&wd->parent_obj, srng->u.src_ring.tp_paddr, &srng->u.src_ring.tp, sizeof(srng->u.src_ring.tp));
    }
    else
    {
        ret = -ENOBUFS;
    }

exit:
    return ret;
}