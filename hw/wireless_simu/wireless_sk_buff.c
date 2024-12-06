#include "wireless_simu.h"

struct sk_buff *alloc_skb(unsigned int size)
{
    unsigned int len = QEMU_ALIGN_UP(size, 8);
    struct sk_buff *skb;

    skb = malloc(sizeof(struct sk_buff));
    if (!skb)
    {
        return NULL;
    }
    memset(skb, 0, sizeof(struct sk_buff));

    skb->truesize = len;
    skb->data = malloc(len);
    skb->data_len = 0;

    return skb;
}

void free_skb(struct sk_buff *skb)
{
    if(skb){
        if(skb->data){
            free(skb->data);
        }
        free(skb);
    }
}
