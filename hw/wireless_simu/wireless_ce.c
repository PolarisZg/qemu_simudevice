#include "wireless_simu.h"

static const struct ce_attr ce_ring_configs[] = {
    {
        .flags = 0,
        .dest_nentries = 32,
        .src_sz_max = 2048, // 在dest_ring中，该参数指定了每个包的最长大小
        .src_nentries = 0,
    },
};

static struct wireless_simu_ce_ring *hal_srng_alloc_ring(struct wireless_simu_device_state *wd, int nentries, int desc_sz)
{
    struct wireless_simu_ce_ring *ring;
    size_t size = sizeof(struct wireless_simu_ce_ring) + sizeof(struct sk_buff) * nentries;

    ring = malloc(size);
    if (!ring)
        return NULL;
    memset(ring, 0, size);

    ring->skb = (void *)ring + sizeof(struct wireless_simu_ce_ring);
    ring->nentries = nentries;
    ring->nentries_mask = nentries - 1;
    ring->sw_index = 0;
    ring->write_index = 0;

    return ring;
}

static int ce_alloc_pipe(struct copy_engine *ce, int pipe_num)
{
    int ret = 0;
    struct wireless_simu_ce_pipe *pipe = &ce->pipes[pipe_num];
    const struct ce_attr *attr = &ce->host_config[pipe_num];
    struct wireless_simu_ce_ring *ring;
    int nentries;
    int desc_sz;

    pipe->pipe_num = pipe_num;
    pipe->wd = ce->wd;
    pipe->attr_flags = attr->flags;
    pipe->buf_sz = attr->src_sz_max;
    pthread_mutex_init(&pipe->pipe_lock, NULL);

    if (attr->dest_nentries)
    {
        pipe->recv_cb = attr->recv_cb;
        pipe->send_cb = attr->send_cb;

        nentries = roundup_pow_of_two(attr->dest_nentries);
        desc_sz = sizeof(struct hal_test_dst);
        ring = hal_srng_alloc_ring(ce->wd, nentries, desc_sz);
        if (!ring)
        {
            printf("%s : ce alloc skb err \n", WIRELESS_SIMU_DEVICE_NAME);
            return -ENOMEM;
        }
        pipe->dst_ring = ring;

        /* status_ring */
        desc_sz = sizeof(struct hal_test_dst_status);
        ring = hal_srng_alloc_ring(ce->wd, 0, desc_sz); // nentries 置零表示不去分配skb数组空间
        if (!ring)
        {
            printf("%s : ce alloc skb err \n", WIRELESS_SIMU_DEVICE_NAME);
            free(pipe->dst_ring);
            pipe->dst_ring = NULL;
            return -ENOMEM;
        }
        pipe->status_ring = ring;
    }
    else
    {
        ret = -EINVAL;
    }

    return ret;
}

void ce_dst_ring_handler(void *user_data)
{
    // printf("%s : ce dst handler in \n", WIRELESS_SIMU_DEVICE_NAME);

    struct wireless_simu_ce_pipe *pipe = (struct wireless_simu_ce_pipe *)user_data;
    struct wireless_simu_device_state *wd = pipe->wd;
    struct wireless_simu_ce_ring *dst_ring = pipe->dst_ring;
    if (!dst_ring)
    {
        printf("%s : ce dst ring no dst \n", WIRELESS_SIMU_DEVICE_NAME);
        return;
    }
    pthread_mutex_lock(&pipe->pipe_lock);
    struct hal_srng *srng = &wd->hal.srng_list[dst_ring->hal_ring_id];
    uint32_t *desc;
    struct hal_test_dst *entry;
    struct sk_buff *skb;
    dma_addr_t paddr;
    uint32_t index;

    qemu_mutex_lock(&srng->lock);

    while (wireless_hal_srng_read_src_ring(wd, srng, &desc) == 0)
    {
        entry = (struct hal_test_dst *)desc;
        index = dst_ring->sw_index;
        skb = &dst_ring->skb[index];
        paddr = entry->buffer_addr_low +
                (((uint64_t)entry->buffer_addr_info & 0xff) << 32);
        WIRELESS_SIMU_SKB_CB(skb)->paddr = paddr;
        printf("%s : dst ring %08x skb %08x paddr %016lx \n",
               WIRELESS_SIMU_DEVICE_NAME, dst_ring->hal_ring_id, index, WIRELESS_SIMU_SKB_CB(skb)->paddr);
        index = (index + 1) & dst_ring->nentries_mask;
        dst_ring->sw_index = index;
        free(desc);
    }

    qemu_mutex_unlock(&srng->lock);

    pthread_mutex_unlock(&pipe->pipe_lock);

    return;
}

static int ce_init_ring(struct wireless_simu_ce_pipe *pipe, struct wireless_simu_ce_ring *ring, int id, enum hal_ring_type type)
{
    int ret = 0;
    struct hal_srng_params params = {0};

    // 填充params
    // params.hal_srng_handler = ce_dst_ring_handler;
    params.user_data = (void *)pipe;

    ret = wireless_hal_srng_setup(pipe->wd, type, id, 0, &params);
    if (ret < 0)
    {
        printf("%s : ce init ring err \n", WIRELESS_SIMU_DEVICE_NAME);
        return ret;
    }

    ring->hal_ring_id = ret;

    return 0;
}

int wireless_simu_ce_init(struct wireless_simu_device_state *wd)
{
    if (!wd)
        return -EINVAL;

    int ret = 0;
    struct copy_engine *ce;
    struct wireless_simu_ce_pipe *pipe;
    wd->ce_count_num = WIRELESS_SIMU_CE_COUNT;

    for (int ce_num = 0; ce_num < wd->ce_count_num; ce_num++)
    {
        ce = &wd->ce_group[ce_num];
        ce->pipes_count = SRNG_TEST_PIPE_COUNT_MAX;
        ce->wd = wd;

        ce->host_config = ce_ring_configs; // todo : 这个应该使用复制操作，一定要改

        pthread_mutex_init(&ce->ce_lock, NULL);

        for (int pipe_num = 0; pipe_num < ce->pipes_count; pipe_num++)
        {
            /* 为dst_ring申请 skb 数组
             */
            ret = ce_alloc_pipe(ce, pipe_num);
            if (ret)
            {
                printf("%s : ce pipe alloc err %d ce id %d pipe id %d \n",
                       WIRELESS_SIMU_DEVICE_NAME, ret, ce_num, pipe_num);
                return ret;
            }

            /* 将ce pipe与实际的ring对应
             */
            pipe = &ce->pipes[pipe_num];
            if (pipe->dst_ring)
            {
                ret = ce_init_ring(pipe, pipe->dst_ring, pipe_num, HAL_TEST_SRNG_DST);
                ret = ce_init_ring(pipe, pipe->status_ring, pipe_num, HAL_TEST_SRNG_DST_STATUS);
            }
            else
            {
                printf("%s : ce pipe dst ring err \n", WIRELESS_SIMU_DEVICE_NAME);
                return -EINVAL;
            }
        }
    }

    return ret;
}

void wireless_simu_ce_post_data(struct wireless_simu_device_state *wd, void *data, size_t data_size)
{
    struct copy_engine *ce;
    struct wireless_simu_ce_pipe *pipe;
    unsigned int sw_index;
    unsigned int write_index;
    struct wireless_simu_ce_ring *dst_ring;
    struct wireless_simu_ce_ring *status_ring;
    struct sk_buff *skb;
    struct hal_srng *status_srng;
    dma_addr_t data_paddr;
    struct hal_test_dst_status desc;

    for (int ce_num = 0; ce_num < wd->ce_count_num; ce_num++)
    {
        ce = &wd->ce_group[ce_num];
        pthread_mutex_lock(&ce->ce_lock);

        for (int pipe_num = 0; pipe_num < ce->pipes_count; pipe_num++)
        {
            pipe = &ce->pipes[pipe_num];
            if (!pipe->dst_ring || !pipe->status_ring)
                continue;

            /* 数据发送 */
            pthread_mutex_lock(&pipe->pipe_lock);

            dst_ring = pipe->dst_ring;
            sw_index = dst_ring->sw_index;
            write_index = dst_ring->write_index;

            if (sw_index == write_index)
            {
                pthread_mutex_unlock(&pipe->pipe_lock);
                continue;
            }

            status_ring = pipe->status_ring;
            status_srng = &wd->hal.srng_list[status_ring->hal_ring_id];
            if (status_srng->u.dst_ring.hp_paddr == 0)
            {
                pthread_mutex_unlock(&pipe->pipe_lock);
                continue;
            }

            skb = &dst_ring->skb[write_index];
            write_index = (write_index + 1) & dst_ring->nentries_mask;
            dst_ring->write_index = write_index;

            /* 对数据的发送分为两个部分:
             * 1. 将数据拷贝至内存区域内
             * 2. 将数据的描述符放入ring */

            /* 1. */
            data_paddr = WIRELESS_SIMU_SKB_CB(skb)->paddr;
            pci_dma_write(&wd->parent_obj, data_paddr, data, (uint64_t)data_size);

            /* 2.
             * 这一步没考虑驱动超慢导致的头指针套圈 */
            desc.buffer_length = (uint32_t)data_size;
            pci_dma_write(&wd->parent_obj,
                          status_srng->ring_base_paddr + (status_srng->u.dst_ring.hp << 2),
                          &desc, sizeof(desc));
            status_srng->u.dst_ring.hp = (status_srng->u.dst_ring.hp + status_srng->entry_size) %
                                         status_srng->ring_size;
            pci_dma_write(&wd->parent_obj, status_srng->u.dst_ring.hp_paddr,
                          &status_srng->u.dst_ring.hp,
                          sizeof(status_srng->u.dst_ring.hp));

            printf("%s : ce %d pipe %d srng %d hp %08x hp_paddr %016lx data size %016lx \n",
                   WIRELESS_SIMU_DEVICE_NAME,
                   ce_num, pipe_num,
                   status_srng->ring_id, status_srng->u.dst_ring.hp,
                   status_srng->u.dst_ring.hp_paddr, (uint64_t)data_size);

            pthread_mutex_unlock(&pipe->pipe_lock);
            pthread_mutex_unlock(&ce->ce_lock);
            wireless_simu_irq_raise(&wd->ws_irq, WIRELESS_SIMU_IRQ_STATU_SRNG_DST_DMA_TEST_RING_0 + pipe_num);
            goto end;
        }

        pthread_mutex_unlock(&ce->ce_lock);
    }

end:
    return;
}