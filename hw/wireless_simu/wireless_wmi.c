#include "wireless_wmi.h"

static void print_hex_dump(const char *prefix_str, const void *buf, size_t len)
{
    const uint8_t *ptr = buf;
    size_t i;

    printf("%s : %p : %zu \n", prefix_str, buf, len);
    for (i = 0; i < len; i++)
    {
        if (i % 16 == 0)
        {
            printf("\n %s : ", prefix_str);
        }
        printf("%02x ", ptr[i]);
    }
    printf("\n");
}

int wireless_simu_wmi_mgmt_send(struct wireless_simu_device_state *wd, struct wmi_mgmt_send_cmd *cmd)
{
    dma_addr_t mgmt_skb_paddr = cmd->paddr_lo | ((uint64_t)cmd->paddr_hi << 32);
    uint32_t mgmt_skb_len = cmd->frame_len;
    uint32_t mgmt_skb_buf_len = cmd->buf_len;

    struct wmi_tlv *frame_tlv = (struct wmi_tlv *)((void *)cmd + sizeof(struct wmi_mgmt_send_cmd));
    void* skb_data;
    int ret = 0;

    if(mgmt_skb_len == mgmt_skb_buf_len){
        skb_data = (void*)frame_tlv->value;
    } else {
        skb_data = malloc(mgmt_skb_len);
        if(!skb_data){
            printf("%s : wmi mgmt send malloc err \n", WIRELESS_SIMU_DEVICE_NAME);
            return -ENOMEM;
        }
        ret = pci_dma_read(&wd->parent_obj, mgmt_skb_paddr, skb_data, mgmt_skb_len);
        if(ret){
            printf("%s : wmi mgmt send dma read err \n", WIRELESS_SIMU_DEVICE_NAME);
            free(skb_data);
            return ret;
        }
    }

    /* 到这里应该就拿到了所有的 skb 数据, 可以进行 send 操作 这里选择直接从高到低打印到控制台 */
    print_hex_dump("wmi mgmt skb", skb_data, mgmt_skb_len);

    return 0;
}

int wireless_simu_openwifi_mgmt_send(struct wireless_simu_device_state *wd, void* data, size_t len){
    int ret = 0;

    // 帧发送
    wireless_tx_data(data, len);
    // print_hex_dump("openwifi skb", data, len);

    return ret;
}

void wireless_simu_openwifi_mgmt_receive(void* data, size_t len, void* device){
    struct wireless_simu_device_state *wd = (struct wireless_simu_device_state *)device;

    printf("%s : socket reveive handler \n", WIRELESS_SIMU_DEVICE_NAME);

    wireless_simu_ce_post_data(wd, data, len);
}