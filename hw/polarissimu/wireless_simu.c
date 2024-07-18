#include "wireless_simu.h"

static int Wireless_dma_add_node(struct WirelessDeviceState *wd, struct Wireless_DMA_Node *dma_node)
{
    if (dma_node == NULL)
        return -1;
    struct Wireless_DMA_Detail *dma_detail = &wd->wireless_dma_detail;
    if (dma_detail->dma_node_list == NULL)
    {
        dma_detail->dma_node_list = (struct Wireless_DMA_Node *)malloc(sizeof(struct Wireless_DMA_Node));
        dma_detail->dma_node_tail = dma_detail->dma_node_list;
        dma_detail->dma_node_count = 1;
    }
    dma_detail->dma_node_tail->next = dma_node;
    dma_detail->dma_node_tail = dma_node;
    dma_detail->dma_data_length += dma_node->data_length;
    dma_detail->dma_node_count++;
    return 0;
}

static int Wireless_dma_del_node(struct WirelessDeviceState *wd, struct Wireless_DMA_Node *dma_node)
{
    if (dma_node == NULL)
        return 0;
    struct Wireless_DMA_Detail *dma_detail = &wd->wireless_dma_detail;
    struct Wireless_DMA_Node *next = dma_node->next;
    dma_detail->dma_data_length -= dma_node->data_length;
    free(dma_node->data);
    if (next == NULL)
    {
        for (struct Wireless_DMA_Node *i = dma_detail->dma_node_list; i != NULL; i = i->next)
        {
            if (i->next == dma_node)
            {
                dma_detail->dma_node_tail = i;
                break;
            }
        }
        free(dma_node);
        return 0;
    }
    else
    {
        dma_node->data = next->data;
        dma_node->data_length = next->data_length;
        dma_node->next = next->next;
        dma_node->node_id = next->node_id;
        dma_node->flag = next->flag;
        free(next);
        return 0;
    }
    printf("%s : del node success \n", WIRELESS_DEVICE_NAME);
}

/*
 * 从内存读
 *
 * 必须提供一个内存中的地址，以及一个待读取的内存长度
 * @DMA_derection DMA_MEMORY_TO_DEVICE
 */
static int Wireless_dma_read_from_mem(struct WirelessDeviceState *wd, struct Wireless_Data_Detail *data)
{
    // struct Wireless_Data_Detail *data = &wd->wireless_data_detail;
    printf("%s : wirte data to dma : host addr %08lx data length %08x \n derection %d \n",
           WIRELESS_DEVICE_NAME, data->host_addr, data->data_length, data->DMA_derection);
    if (data->DMA_derection != DMA_MEMORY_TO_DEVICE)
    {
        printf("%s : erro direction \n", WIRELESS_DEVICE_NAME);
        return -5;
    }
    struct Wireless_DMA_Detail *dma_detail = &wd->wireless_dma_detail;
    if (data->data_length > dma_detail->dma_max_mem_size)
    {
        printf("%s : write to dma data too long\n", WIRELESS_DEVICE_NAME);
        return -1;
    }
    struct Wireless_DMA_Node *dma_node = (struct Wireless_DMA_Node *)malloc(sizeof(struct Wireless_DMA_Node));
    if (dma_node == NULL)
    {
        printf("%s : write data malloc node fail \n", WIRELESS_DEVICE_NAME);
        return -2;
    }
    dma_node->node_id = dma_detail->dma_node_count + 1;
    dma_node->data_length = data->data_length;
    dma_node->next = NULL;
    dma_node->data = malloc(dma_node->data_length);
    if (dma_node->data == NULL)
    {
        printf("%s : dma node data malloc fail \n", WIRELESS_DEVICE_NAME);
        free(dma_node);
        return -3;
    }
    if (pci_dma_read(&wd->parent_obj, data->host_addr, dma_node->data, dma_node->data_length))
    {
        printf("%s : write to dma error \n", WIRELESS_DEVICE_NAME);
        free(dma_node->data);
        free(dma_node);
        return -4;
    }
    dma_node->flag |= 1;
    Wireless_dma_add_node(wd, dma_node);
    // data->dma_node_id = dma_node->node_id;
    printf("%s : add dma node success : node id %08x data addr %p data length %08x\n",
           WIRELESS_DEVICE_NAME,
           wd->wireless_dma_detail.dma_node_tail->node_id,
           wd->wireless_dma_detail.dma_node_tail->data,
           wd->wireless_dma_detail.dma_node_tail->data_length);
    printf("%s : write device dma last data : %02x \n", WIRELESS_DEVICE_NAME, ((u_int8_t *)dma_node->data)[dma_node->data_length - 1]);
    return 0;
}

static int Wireless_dma_del_all(struct WirelessDeviceState *wd)
{
    printf("%s : cleam dma list \n", WIRELESS_DEVICE_NAME);
    struct Wireless_DMA_Detail *dma_detail = &wd->wireless_dma_detail;
    while (dma_detail->dma_node_list != NULL && dma_detail->dma_node_list != dma_detail->dma_node_tail)
    {
        Wireless_dma_del_node(wd, dma_detail->dma_node_list->next);
        printf("%s : dma clear : dma length : %ld \n", WIRELESS_DEVICE_NAME, dma_detail->dma_max_mem_size);
    }
    return 0;
}

/*
 * 向内存写
 *
 * 在data中必须提供一个host内存地址，以及需要读取的dma node的id
 * @DMA_derection DMA_DEVICE_TO_MEMORY
 */
static int Wireless_dma_read_from_device(struct WirelessDeviceState *wd, struct Wireless_Data_Detail *data)
{
    // struct Wireless_Data_Detail *data = &wd->wireless_data_detail;
    printf("%s : wirte data to mem : host addr %08lx data length %08x \n derection %d",
           WIRELESS_DEVICE_NAME, data->host_addr, data->data_length, data->DMA_derection);
    if (data->DMA_derection != DMA_DEVICE_TO_MEMORY)
    {
        printf("%s : write data to mem : erro direction \n", WIRELESS_DEVICE_NAME);
        return -5;
    }
    struct Wireless_DMA_Detail *dma_detail = &wd->wireless_dma_detail;
    if (dma_detail == NULL || dma_detail->dma_node_list == NULL)
    {
        printf("%s : write data 2 mem : dma list is null \n", WIRELESS_DEVICE_NAME);
        return -1;
    }
    struct Wireless_DMA_Node *dma_node = NULL;
    for (struct Wireless_DMA_Node *node = dma_detail->dma_node_list; node != NULL; node = node->next)
    {
        if (node->node_id == data->dma_node_id)
        {
            dma_node = node;
            break;
        }
    }
    if (dma_node == NULL)
    {
        printf("%s : write data 2 mem : node 404 \n", WIRELESS_DEVICE_NAME);
        return -2;
    }

    if (dma_node->data_length >= data->data_max_length)
    {
        printf("%s : write data to mem : length err \n", WIRELESS_DEVICE_NAME);
        return -4;
    }
    if (data->host_addr == 0)
    {
        printf("%s : wirte data to mem : host addr null \n", WIRELESS_DEVICE_NAME);
        return -5;
    }
    if (pci_dma_write(&wd->parent_obj, data->host_addr, dma_node->data, dma_node->data_length))
    {
        printf("%s : wirte data to mem : read from device err \n", WIRELESS_DEVICE_NAME);
        return -3;
    }
    data->data_length = dma_node->data_length;
    return 0;
}

/*
 * 发出中断
 *
 * 对于Intx 中断，必须清中断
 */
static void Wireless_Interrupt_raise(struct WirelessDeviceState *wd, u_int32_t irq_status)
{
    qemu_mutex_lock(&wd->irq_intx_mutex);
    wd->irq_status = irq_status;
    if (msi_enabled(&wd->parent_obj))
    {
        msi_notify(&wd->parent_obj, 0);
    }
    else
    {
        pci_set_irq(&wd->parent_obj, 1);
    }
}

/*
 * 清中断
 */
static void Wireless_Interrup_lower(struct WirelessDeviceState *wd)
{
    if (!msi_enabled(&wd->parent_obj))
    {
        pci_set_irq(&wd->parent_obj, 0);
    }
    wd->irq_status = 0;
    qemu_mutex_unlock(&wd->irq_intx_mutex);
}

/*
 * 对设备的dma进行管理，防止其占用的内存空间超过memsize发生内存溢出
 *
 * 此函数涉及到敏感的删除操作，需要防止出现悬空指针
 */
// static void *Wireless_dma_mem_manager(void *opaque)
// {
//     printf("%s : dma clean start \n", WIRELESS_DEVICE_NAME);
//     struct WirelessDeviceState *wd = opaque;
//     // struct Wireless_DMA_Detail *dma_detail = &wd->wireless_dma_detail;
//     // while (1)
//     // {
//     //     if (wd->stop)
//     //     {
//     //         goto stop;
//     //     }
//     //     qemu_mutex_lock(&wd->dma_access_mutex);
//     //     if (dma_detail->dma_data_length > dma_detail->dma_max_mem_size)
//     //     {
//     //         if (dma_detail->dma_node_list == NULL || dma_detail->dma_node_list == dma_detail->dma_node_tail)
//     //         {
//     //             goto stop;
//     //         }
//     //         for (struct Wireless_DMA_Node *node = dma_detail->dma_node_list; node != NULL; node = node->next)
//     //         {
//     //             /*
//     //              * 一些对node的判断，对于某些flag的node不能随意删除，比如正在写入到内存的
//     //              */
//     //             if (node->data_length > 0 && WIRELESS_CHECK_WORD_FLAG(node->flag, WIRELESS_FLAG_DMA_NODE_ISUSING_BIT))
//     //             {
//     //                 Wireless_dma_del_node(wd, node);
//     //                 break;
//     //             }
//     //         }
//     //     }
//     //     qemu_mutex_unlock(&wd->dma_access_mutex);
//     //     sleep(20);
//     // }

//     // stop:
//     printf("%s : dma clean stop \n", WIRELESS_DEVICE_NAME);
//     qemu_mutex_unlock(&wd->dma_access_mutex);
//     return NULL;
// }

/*
 * DMA 控制器
 *
 * @doc 这个史不就叠起来了吗
 */
static void Wireless_DMA_Process(struct WirelessDeviceState *wd)
{
    if (wd->stop)
        return;
    // 加锁保证DMA控制器同一时间只会有一个线程在运行
    // 虽然控制器只能有一个，但控制器的tx/rx ring缓冲区可以被多线程共同修改
    qemu_mutex_lock(&wd->dma_access_mutex);
    for (int i = 0; i < WIRELESS_TX_RING_SIZE; i++)
    {
        struct Wireless_Data_Detail *data_detail = &wd->tx_ring_buf[i];
        if (data_detail->flag & 1)
        {
            sleep(10); // 拉长dma时间便于调试
            Wireless_dma_read_from_mem(wd, data_detail);
            data_detail->flag &= ~(1U);
            qemu_mutex_lock(&wd->dma_out_mutex);
            wd->wireless_dma_out_detail = &wd->tx_ring_buf[i];
            Wireless_Interrupt_raise(wd, WIRELESS_IRQ_DNA_MEM_TO_DEVICE_END);
        }
    }
    qemu_mutex_unlock(&wd->dma_access_mutex);

    qemu_mutex_lock(&wd->dma_access_mutex);
    for (int i = 0; i < WIRELESS_RX_RING_SIZE; i++)
    {
        struct Wireless_Data_Detail *data_detail = &wd->rx_ring_buf[i];
        if (WIRELESS_BITCHECK(data_detail->flag, WIRELESS_RX_RING_BUF_IS_USING) &&
            WIRELESS_BITCHECK(data_detail->flag, WIRELESS_RX_RING_BUF_INIT_END))
        {
            sleep(10); // 拉长dma时间便于调试

            Wireless_dma_read_from_device(wd, data_detail);
            // WIRELESS_BITCLR(data_detail->flag, WIRELESS_RX_RING_BUF_IS_USING);
            qemu_mutex_lock(&wd->dma_out_mutex);
            wd->wireless_dma_out_detail = data_detail;
            Wireless_Interrupt_raise(wd, WIRELESS_IRQ_DMA_DEVICE_TO_MEM_END);
        }
    }
    qemu_mutex_unlock(&wd->dma_access_mutex);
}

/*
 * 耗时较长的任务*/
static void Wireless_LongTime_Handler(void *opaque)
{
    struct WirelessDeviceState *wd = opaque;
    if (wd->stop)
        return;
    while (!wd->irq_enable)
    {
        // 虽然这里用条件变量更节省cpu，但是这个enable反正只会在开头和结尾被设置一次，所以没差啦
        if (wd->stop)
            return;
    }
    printf("%s : in long time function %d \n", WIRELESS_DEVICE_NAME, wd->long_time_event);
    switch (wd->long_time_event)
    {
    case WIRELESS_EVENT_DMA:
        Wireless_DMA_Process(wd);
        // DMA控制器会自动发中断, 直接return
        return;
    case WIRELESS_EVENT_CLEAN_DMA:
        Wireless_dma_del_all(wd);
        Wireless_Interrupt_raise(wd, WIRELESS_IRQ_DMA_DELALL_END);
        break;
    case WIRELESS_EVENT_TEST:
        sleep(10);
        Wireless_Interrupt_raise(wd, WIRELESS_IRQ_TEST);
        break;
    case WIRELESS_EVENT_NOEVENT:
        Wireless_Interrup_lower(wd);
        printf("%s : clean irq \n", WIRELESS_DEVICE_NAME);
        return;
    }
}

static void Wireless_Add_Task(struct WirelessDeviceState *wd, u_int32_t val)
{
    wd->long_time_event = val;
    if (wd->long_time_event == WIRELESS_EVENT_NOEVENT)
    {
        Wireless_Interrup_lower(wd);
        printf("%s : clean irq \n", WIRELESS_DEVICE_NAME);
        return;
    }
    timer_mod(&wd->irq_timer, qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL) + 100);
}

/*
 * 测试dma，每隔20s将写入到device的数据写回memory
 * 暂时还没写出来device到mem的部分
 */
static void *Wireless_dma_test(void *opaque)
{
    printf("%s : dma test start \n", WIRELESS_DEVICE_NAME);
    struct WirelessDeviceState *wd = opaque;

    while (1)
    {
        if (wd->stop)
            break;
        sleep(20);
        struct Wireless_DMA_Node *node = NULL;
        struct Wireless_DMA_Detail *dma_detail = &wd->wireless_dma_detail;
        if (dma_detail == NULL || dma_detail->dma_data_length <= 0 || dma_detail->dma_node_list == NULL)
            continue;
        for (node = dma_detail->dma_node_list; node != NULL; node = node->next)
        {
            if (node->data_length > 0 && (node->flag & 1)) // 这里通过读取node的flag来确定是否应该将该node发送给驱动
            {
                int i = 0;
                for (; i < WIRELESS_RX_RING_SIZE; i++)
                {
                    if (wd->rx_ring_buf[i].host_addr != 0 &&
                        node->data_length < wd->rx_ring_buf[i].data_max_length &&
                        WIRELESS_BITCHECK(wd->rx_ring_buf[i].flag, WIRELESS_RX_RING_BUF_INIT_END) &&
                        !WIRELESS_BITCHECK(wd->rx_ring_buf[i].flag, WIRELESS_RX_RING_BUF_IS_USING))
                    {
                        break;
                    }
                }
                if (i == WIRELESS_RX_RING_SIZE)
                {
                    continue;
                }
                // WIRELESS_BITCLR(wd->rx_ring_buf[i].flag, WIRELESS_RX_RING_BUF_INIT_END);
                WIRELESS_BITSET(wd->rx_ring_buf[i].flag, WIRELESS_RX_RING_BUF_IS_USING);
                wd->rx_ring_buf[i].dma_node_id = node->node_id;
                printf("%s : rx node %08x , rx detail %d \n", WIRELESS_DEVICE_NAME, node->node_id, i);
                Wireless_Add_Task(wd, WIRELESS_EVENT_DMA);
                node->flag &= ~(1U);
            }
        }
    }

    printf("%s : dma test end \n", WIRELESS_DEVICE_NAME);
    return NULL;
}

/*
 * 从 device 的 addr 寄存器读取数据并返回
 *
 * @addr 驱动侧使用 mmio 地址 + addr 读取 addr 对应的寄存器
 * @size 以字节为单位，ioread32 就是 读取 4 个字节, 默认每次读取 4 个字节
 */
static u_int64_t Wireless_read(void *opaque, hwaddr addr, unsigned size)
{
    printf("%s : read reg : %lx size %u \n", WIRELESS_DEVICE_NAME, addr, size);
    struct WirelessDeviceState *wd = opaque;
    u_int64_t val = 0LL;

    switch (addr)
    {
    case WIRELESS_REG_TEST:
        val = 0x114514;
        break;
    case WIRELESS_REG_DMA_IN_HOSTADDR:
        if (wd->wireless_data_detail == NULL)
        {
            printf("%s : no cmd in dma \n", WIRELESS_DEVICE_NAME);
            val = 0x114514;
            break;
        }
        val = wd->wireless_data_detail->host_addr;
        break;
    case WIRELESS_REG_DMA_IN_LENGTH:
        if (wd->wireless_data_detail == NULL)
        {
            printf("%s : no cmd in dma \n", WIRELESS_DEVICE_NAME);
            val = 0x114514;
            break;
        }
        val = wd->wireless_data_detail->data_length;
        break;
    case WIRELESS_REG_DMA_IN_BUFF_ID:
        if (wd->wireless_data_detail == NULL)
        {
            printf("%s : no cmd in dma \n", WIRELESS_DEVICE_NAME);
            val = 0x114514;
            break;
        }
        val = wd->wireless_data_detail->host_buffer_id;
        break;
    case WIRELESS_REG_DMA_IN_FLAG:
        if (wd->wireless_data_detail == NULL)
        {
            printf("%s : no cmd in dma \n", WIRELESS_DEVICE_NAME);
            val = 0x114514;
            break;
        }
        val = wd->wireless_data_detail->flag;
        break;
    case WIRELESS_REG_IRQ_STATUS:
        val = wd->irq_status;
        break;
    case WIRELESS_REG_DMA_OUT_BUFF_ID:
        if (wd->wireless_dma_out_detail == NULL)
        {
            printf("%s : no cmd out dma \n", WIRELESS_DEVICE_NAME);
            val = 0x114514;
            break;
        }
        val = wd->wireless_dma_out_detail->host_buffer_id;
        break;
    case WIRELESS_REG_DMA_OUT_HOSTADDR:
        if (wd->wireless_dma_out_detail == NULL)
        {
            printf("%s : no cmd out dma \n", WIRELESS_DEVICE_NAME);
            val = 0x114514;
            break;
        }
        val = wd->wireless_dma_out_detail->host_addr;
        break;
    case WIRELESS_REG_DMA_OUT_LENGTH:
        if (wd->wireless_dma_out_detail == NULL)
        {
            printf("%s : no cmd out dma \n", WIRELESS_DEVICE_NAME);
            val = 0x114514;
            break;
        }
        val = wd->wireless_dma_out_detail->data_length;
        break;
    case WIRELESS_REG_DMA_OUT_FLAG:
        if (wd->wireless_dma_out_detail == NULL)
        {
            printf("%s : no cmd out dma \n", WIRELESS_DEVICE_NAME);
            val = 0x114514;
            break;
        }
        val = wd->wireless_dma_out_detail->flag;
        qemu_mutex_unlock(&wd->dma_out_mutex);
        break;
    case WIRELESS_REG_DMA_RX_RING_BUF_ID:
        if (wd->wireless_data_rx_detail == NULL)
        {
            printf("%s : rx ring init id NULL \n", WIRELESS_DEVICE_NAME);
            break;
        }
        val = wd->wireless_data_rx_detail->dma_node_id;
        break;
    case WIRELESS_REG_DMA_RX_RING_HOSTADDR:
        if (wd->wireless_data_rx_detail == NULL)
        {
            printf("%s : rx ring init id NULL \n", WIRELESS_DEVICE_NAME);
            break;
        }
        val = wd->wireless_data_rx_detail->host_addr;
        break;
    case WIRELESS_REG_DMA_RX_RING_FLAG:
        if (wd->wireless_data_rx_detail == NULL)
        {
            printf("%s : rx ring init id NULL \n", WIRELESS_DEVICE_NAME);
            break;
        }
        val = wd->wireless_data_rx_detail->flag;
        break;
    case WIRELESS_REG_DMA_RX_RING_LENGTH:
        if (wd->wireless_data_rx_detail == NULL)
        {
            printf("%s : rx ring init id NULL \n", WIRELESS_DEVICE_NAME);
            break;
        }
        val = wd->wireless_data_rx_detail->data_max_length;
        break;
    case WIRELESS_REG_IRQ_ENABLE:
        val = wd->irq_enable;
        break;
    default:
        printf("%s : read err no reg %lu \n", WIRELESS_DEVICE_NAME, addr);
        break;
    }
    return val;
}

/*
 * 向 device 的 addr 寄存器 写入数据
 *
 * @addr 驱动侧使用 mmio 地址 + addr 读取 addr 对应的寄存器
 * @data 等待写入的数据
 * @size 从data的最低位开始取出的数据长度, 以字节为单位, 默认(强制要求)驱动侧使用iowrite32
 */
static void Wireless_write(void *opaque, hwaddr addr, u_int64_t data, unsigned size)
{
    printf("%s : write reg %lx data %lx size %u \n", WIRELESS_DEVICE_NAME, addr, data, size);
    if (size != 4)
    {
        printf("%s : size err \n", WIRELESS_DEVICE_NAME);
        return;
    }

    struct WirelessDeviceState *wd = opaque;
    u_int32_t val = (u_int32_t)(data & 0xffffffff);
    switch (addr)
    {
    case WIRELESS_REG_TEST:
        printf("%s : write data : %08x \n", WIRELESS_DEVICE_NAME, val);
        break;
    case WIRELESS_REG_EVENT:
        Wireless_Add_Task(wd, val);
        break;
    case WIRELESS_REG_DMA_IN_HOSTADDR:
        wd->wireless_data_detail->host_addr = val;
        wd->wireless_data_detail->DMA_derection = DMA_MEMORY_TO_DEVICE;
        break;
    case WIRELESS_REG_DMA_IN_LENGTH:
        wd->wireless_data_detail->data_length = val;
        break;
    case WIRELESS_REG_DMA_IN_FLAG:
        wd->wireless_data_detail->flag = val;
        break;
    case WIRELESS_REG_DMA_IN_BUFF_ID:
        wd->wireless_data_detail = &wd->tx_ring_buf[val];
        wd->wireless_data_detail->host_buffer_id = val;
        wd->wireless_data_detail->DMA_derection = DMA_MEMORY_TO_DEVICE;
        break;
    case WIRELESS_REG_DMA_RX_RING_BUF_ID:
        if (val >= WIRELESS_RX_RING_SIZE)
        {
            printf("%s : rx ring init err id BIG \n", WIRELESS_DEVICE_NAME);
            break;
        }
        wd->wireless_data_rx_detail = &wd->rx_ring_buf[val];
        wd->wireless_data_rx_detail->host_buffer_id = val;
        wd->wireless_data_rx_detail->DMA_derection = DMA_DEVICE_TO_MEMORY;
        break;
    case WIRELESS_REG_DMA_RX_RING_HOSTADDR:
        if (wd->wireless_data_rx_detail == NULL)
        {
            printf("%s : rx ring init id NULL \n", WIRELESS_DEVICE_NAME);
            break;
        }
        wd->wireless_data_rx_detail->host_addr = val;
        break;
    case WIRELESS_REG_DMA_RX_RING_FLAG:
        if (wd->wireless_data_rx_detail == NULL)
        {
            printf("%s : rx ring init id NULL \n", WIRELESS_DEVICE_NAME);
            break;
        }
        wd->wireless_data_rx_detail->flag = val;
        break;
    case WIRELESS_REG_DMA_RX_RING_LENGTH:
        if (wd->wireless_data_rx_detail == NULL)
        {
            printf("%s : rx ring init id NULL \n", WIRELESS_DEVICE_NAME);
            break;
        }
        wd->wireless_data_rx_detail->data_max_length = val;
        wd->wireless_data_rx_detail->data_length = val;
        break;
    case WIRELESS_REG_IRQ_ENABLE:
        wd->irq_enable = val == 0 ? 0 : 1;
        break;
    default:
        printf("%s : write err no reg %lu \n", WIRELESS_DEVICE_NAME, addr);
        break;
    }
}

static u_int64_t Wireless_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    return Wireless_read(opaque, addr, size);
}

static void Wireless_mmio_write(void *opaque, hwaddr addr, u_int64_t val, unsigned size)
{
    Wireless_write(opaque, addr, val, size);
}

static const struct MemoryRegionOps Wireless_mmio_ops = {
    .read = Wireless_mmio_read,
    .write = Wireless_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void Wireless_realize(struct PCIDevice *pdev, struct Error **errp)
{
    struct WirelessDeviceState *wd = WIRELESS_DEVICE_OBJ(pdev);

    // intx irq
    pci_config_set_interrupt_pin(pdev->config, 1);

    // msi irq
    if (msi_init(pdev, 0, 1, false, false, errp))
    {
        printf("test pci : msi interrupt malloc falut \n");
    }

    // 需长时间处理的任务
    timer_init_ms(&wd->irq_timer, QEMU_CLOCK_VIRTUAL, Wireless_LongTime_Handler, wd);

    // 多线程锁
    qemu_mutex_init(&wd->dma_access_mutex);
    qemu_mutex_init(&wd->irq_intx_mutex);
    qemu_mutex_init(&wd->dma_out_mutex);

    // dma 测试
    qemu_thread_create(&wd->dma_clean_thread, "polariswireless-dmaclean",
                       Wireless_dma_test, wd, QEMU_THREAD_JOINABLE);

    // mmio 最后的数字的作用是限制写入的大小，往大了写就行
    memory_region_init_io(&wd->mmio, OBJECT(wd),
                          &Wireless_mmio_ops, pdev, "polariswireless-mmio", 4096);
    pci_register_bar(pdev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &wd->mmio);
}

static void Wireless_exit(struct PCIDevice *pdev)
{
    struct WirelessDeviceState *wd = WIRELESS_DEVICE_OBJ(pdev);

    qemu_mutex_lock(&wd->dma_access_mutex);
    qemu_mutex_lock(&wd->dma_out_mutex);
    wd->stop = true;
    qemu_mutex_unlock(&wd->dma_out_mutex);
    qemu_mutex_unlock(&wd->dma_access_mutex);

    qemu_thread_join(&wd->dma_clean_thread);
    qemu_mutex_destroy(&wd->dma_access_mutex);
    qemu_mutex_destroy(&wd->dma_out_mutex);
    qemu_mutex_destroy(&wd->irq_intx_mutex);
    timer_del(&wd->irq_timer);
    msi_uninit(pdev);
}

static void Wireless_instance_init(struct Object *obj)
{
    struct WirelessDeviceState *wd = WIRELESS_DEVICE_OBJ(obj);
    wd->dma_mask = (1UL << 32) - 1;
    // wd->irq_message = 0;
    wd->irq_status = 0;
    wd->long_time_event = WIRELESS_EVENT_NOEVENT;
    struct Wireless_DMA_Detail *dma_detail = &wd->wireless_dma_detail;
    dma_detail->dma_max_mem_size = 0xfffff;
    dma_detail->dma_node_list = (struct Wireless_DMA_Node *)malloc(sizeof(struct Wireless_DMA_Node));
    dma_detail->dma_node_list->data_length = 0;
    dma_detail->dma_node_list->flag |= 1;
    dma_detail->dma_node_tail = dma_detail->dma_node_list;
    dma_detail->dma_data_length = 0;
    dma_detail->dma_node_count = 1;
}

static void Wireless_class_init(struct ObjectClass *class, void *data)
{
    struct DeviceClass *dc = DEVICE_CLASS(class);
    struct PCIDeviceClass *pci = PCI_DEVICE_CLASS(class);

    pci->realize = Wireless_realize;
    pci->exit = Wireless_exit;
    pci->vendor_id = PCI_VENDOR_ID_QEMU;
    pci->device_id = 0x1145;
    pci->revision = 0x14;
    pci->class_id = PCI_CLASS_OTHERS;

    dc->desc = "polaris wireless device";
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const struct TypeInfo Wireless_type_info = {
    .name = WIRELESS_DEVICE_NAME,
    .parent = TYPE_PCI_DEVICE,
    .instance_init = Wireless_instance_init,
    .instance_size = sizeof(struct WirelessDeviceState),
    .class_init = Wireless_class_init,
    .interfaces = (struct InterfaceInfo[]){
        {INTERFACE_CONVENTIONAL_PCI_DEVICE},
        {},
    },
};

static void Wireless_register_types(void)
{
    printf("%s : register start", WIRELESS_DEVICE_NAME);
    type_register_static(&Wireless_type_info);
}

type_init(Wireless_register_types);