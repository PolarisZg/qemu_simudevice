#ifndef WIRELESS_SIMU_HAL
#define WIRELESS_SIMU_HAL

#include "wireless_simu.h"

struct wireless_simu_device_state;

/* SRNG registers are split into two groups R0 and R2 */
#define HAL_SRNG_REG_GRP_R0 0
#define HAL_SRNG_REG_GRP_R2 1
#define HAL_SRNG_NUM_REG_GRP 2

enum hal_srng_dir
{
    HAL_SRNG_DIR_SRC = 1,
    HAL_SRNG_DIR_DST
};

enum hal_srng_ring_id
{
    HAL_SRNG_RING_ID_REO2SW1 = 0,
    HAL_SRNG_RING_ID_REO2SW2,
    HAL_SRNG_RING_ID_REO2SW3,
    HAL_SRNG_RING_ID_REO2SW4,
    HAL_SRNG_RING_ID_REO2TCL,
    HAL_SRNG_RING_ID_SW2REO,

    HAL_SRNG_RING_ID_REO_CMD = 8,
    HAL_SRNG_RING_ID_REO_STATUS,

    HAL_SRNG_RING_ID_SW2TCL1 = 16,
    HAL_SRNG_RING_ID_SW2TCL2,
    HAL_SRNG_RING_ID_SW2TCL3,
    HAL_SRNG_RING_ID_SW2TCL4,

    HAL_SRNG_RING_ID_SW2TCL_CMD = 24,
    HAL_SRNG_RING_ID_TCL_STATUS,

    HAL_SRNG_RING_ID_CE0_SRC = 32,
    HAL_SRNG_RING_ID_CE1_SRC,
    HAL_SRNG_RING_ID_CE2_SRC,
    HAL_SRNG_RING_ID_CE3_SRC,
    HAL_SRNG_RING_ID_CE4_SRC,
    HAL_SRNG_RING_ID_CE5_SRC,
    HAL_SRNG_RING_ID_CE6_SRC,
    HAL_SRNG_RING_ID_CE7_SRC,
    HAL_SRNG_RING_ID_CE8_SRC,
    HAL_SRNG_RING_ID_CE9_SRC,
    HAL_SRNG_RING_ID_CE10_SRC,
    HAL_SRNG_RING_ID_CE11_SRC,

    HAL_SRNG_RING_ID_CE0_DST = 56,
    HAL_SRNG_RING_ID_CE1_DST,
    HAL_SRNG_RING_ID_CE2_DST,
    HAL_SRNG_RING_ID_CE3_DST,
    HAL_SRNG_RING_ID_CE4_DST,
    HAL_SRNG_RING_ID_CE5_DST,
    HAL_SRNG_RING_ID_CE6_DST,
    HAL_SRNG_RING_ID_CE7_DST,
    HAL_SRNG_RING_ID_CE8_DST,
    HAL_SRNG_RING_ID_CE9_DST,
    HAL_SRNG_RING_ID_CE10_DST,
    HAL_SRNG_RING_ID_CE11_DST,

    HAL_SRNG_RING_ID_CE0_DST_STATUS = 80,
    HAL_SRNG_RING_ID_CE1_DST_STATUS,
    HAL_SRNG_RING_ID_CE2_DST_STATUS,
    HAL_SRNG_RING_ID_CE3_DST_STATUS,
    HAL_SRNG_RING_ID_CE4_DST_STATUS,
    HAL_SRNG_RING_ID_CE5_DST_STATUS,
    HAL_SRNG_RING_ID_CE6_DST_STATUS,
    HAL_SRNG_RING_ID_CE7_DST_STATUS,
    HAL_SRNG_RING_ID_CE8_DST_STATUS,
    HAL_SRNG_RING_ID_CE9_DST_STATUS,
    HAL_SRNG_RING_ID_CE10_DST_STATUS,
    HAL_SRNG_RING_ID_CE11_DST_STATUS,

    HAL_SRNG_RING_ID_WBM_IDLE_LINK = 104,
    HAL_SRNG_RING_ID_WBM_SW_RELEASE,
    HAL_SRNG_RING_ID_WBM2SW0_RELEASE,
    HAL_SRNG_RING_ID_WBM2SW1_RELEASE,
    HAL_SRNG_RING_ID_WBM2SW2_RELEASE,
    HAL_SRNG_RING_ID_WBM2SW3_RELEASE,
    HAL_SRNG_RING_ID_WBM2SW4_RELEASE,

    HAL_SRNG_RING_ID_TEST_SW2HW = 125,

    HAL_SRNG_RING_ID_UMAC_ID_END = 127,
    HAL_SRNG_RING_ID_LMAC1_ID_START,

    HAL_SRNG_RING_ID_WMAC1_SW2RXDMA0_BUF = HAL_SRNG_RING_ID_LMAC1_ID_START,
    HAL_SRNG_RING_ID_WMAC1_SW2RXDMA1_BUF,
    HAL_SRNG_RING_ID_WMAC1_SW2RXDMA2_BUF,
    HAL_SRNG_RING_ID_WMAC1_SW2RXDMA0_STATBUF,
    HAL_SRNG_RING_ID_WMAC1_SW2RXDMA1_STATBUF,
    HAL_SRNG_RING_ID_WMAC1_RXDMA2SW0,
    HAL_SRNG_RING_ID_WMAC1_RXDMA2SW1,
    HAL_SRNG_RING_ID_WMAC1_SW2RXDMA1_DESC,
    HAL_SRNG_RING_ID_RXDMA_DIR_BUF,

    HAL_SRNG_RING_ID_LMAC1_ID_END = 143
};

/* SRNG registers are split into two groups R0 and R2 */
#define HAL_SRNG_REG_GRP_R0 0
#define HAL_SRNG_REG_GRP_R2 1
#define HAL_SRNG_NUM_REG_GRP 2

#define HAL_SRNG_NUM_LMACS 3
#define HAL_SRNG_REO_EXCEPTION HAL_SRNG_RING_ID_REO2SW1
#define HAL_SRNG_RINGS_PER_LMAC (HAL_SRNG_RING_ID_LMAC1_ID_END - \
                                 HAL_SRNG_RING_ID_LMAC1_ID_START)
#define HAL_SRNG_NUM_LMAC_RINGS (HAL_SRNG_NUM_LMACS * HAL_SRNG_RINGS_PER_LMAC)
#define HAL_SRNG_RING_ID_MAX (HAL_SRNG_RING_ID_UMAC_ID_END + \
                              HAL_SRNG_NUM_LMAC_RINGS)

#define HAL_SHADOW_NUM_REGS 36

/* Common SRNG ring structure for source and destination rings */
struct hal_srng
{
    /* 指向顶级模块 */
    struct wireless_simu_device_state *wd;

    /* Unique SRNG ring ID */
    uint8_t ring_id;

    /* Ring initialization done */
    uint8_t initialized;

    /* Interrupt/MSI value assigned to this ring */
    int irq;

    /* Physical base address of the ring */
    dma_addr_t ring_base_paddr;

    /* Virtual base address of the ring */
    uint32_t *ring_base_vaddr;

    /* Number of entries in ring */
    uint32_t num_entries;

    /* Ring size */
    uint32_t ring_size;

    /* Ring size mask */
    uint32_t ring_size_mask;

    /* Size of ring entry */
    uint32_t entry_size;

    /* Interrupt timer threshold - in micro seconds */
    uint32_t intr_timer_thres_us;

    /* Interrupt batch counter threshold - in number of ring entries */
    uint32_t intr_batch_cntr_thres_entries;

    /* MSI Address */
    dma_addr_t msi_addr;

    /* MSI data */
    uint32_t msi_data;

    /* Misc flags */
    uint32_t flags;

    /* Lock for serializing ring index updates */
    QemuMutex lock;

    /* Start offset of SRNG register groups for this ring
     * TBD: See if this is required - register address can be derived
     * from ring ID
     */
    uint32_t hwreg_base[HAL_SRNG_NUM_REG_GRP];

    uint64_t timestamp;

    /* Source or Destination ring */
    enum hal_srng_dir ring_dir;

    union
    {
        struct
        {
            /* SW tail pointer */
            uint32_t tp;

            /* Shadow head pointer location to be updated by HW */
            volatile uint32_t *hp_addr;

            /* Cached head pointer */
            uint32_t cached_hp;

            /* Tail pointer location to be updated by SW - This
             * will be a register address and need not be
             * accessed through SW structure
             */
            uint32_t *tp_addr;

            /* Current SW loop cnt */
            uint32_t loop_cnt;

            /* max transfer size */
            uint16_t max_buffer_length;

            /* head pointer at access end */
            uint32_t last_hp;
        } dst_ring;

        struct
        {
            /* SW head pointer */
            uint32_t hp;

            /* SW reap head pointer */
            uint32_t reap_hp;

            /* Shadow tail pointer location to be updated by HW */
            dma_addr_t tp_paddr;
            
            uint32_t tp;

            /* Cached tail pointer */
            uint32_t cached_tp;

            /* Head pointer location to be updated by SW - This
             * will be a register address and need not be accessed
             * through SW structure
             */
            uint32_t *hp_addr;

            /* Low threshold - in number of ring entries */
            uint32_t low_threshold;

            /* tail pointer at access end */
            uint32_t last_tp;
        } src_ring;
    } u;
};

/* HW SRNG configuration table */
struct hal_srng_config
{
    int start_ring_id;
    uint16_t max_rings;
    uint16_t entry_size;
    uint32_t reg_start[HAL_SRNG_NUM_REG_GRP];
    uint16_t reg_size[HAL_SRNG_NUM_REG_GRP];
    uint8_t lmac_ring;
    enum hal_srng_dir ring_dir;
    uint32_t max_size;
};

/*
 * 名为hal的srng子模块
 */
struct wireless_simu_hal
{
    /* HAL internal state for all SRNG rings.
     */
    struct hal_srng srng_list[HAL_SRNG_RING_ID_MAX];

    /* SRNG configuration table */
    struct hal_srng_config *srng_config;

    // struct device *dev;

    /* Remote pointer memory for HW/FW updates */
    struct
    {
        uint32_t *vaddr;
        dma_addr_t paddr;
    } rdp;

    /* Shared memory for ring pointer updates from host to FW */
    struct
    {
        uint32_t *vaddr;
        dma_addr_t paddr;
    } wrp;

    /* Available REO blocking resources bitmap */
    uint8_t avail_blk_resource;

    uint8_t current_blk_index;

    /* shadow register configuration */
    uint32_t shadow_reg_addr[HAL_SHADOW_NUM_REGS];
    int num_shadow_reg_configured;

    // QemuMutex srng_key[HAL_SRNG_RING_ID_MAX];
};

struct hal_test_sw2hw{
    uint32_t buffer_addr_low;
	uint32_t buffer_addr_info; /* %HAL_CE_SRC_DESC_ADDR_INFO_ */
	uint32_t meta_info; /* %HAL_CE_SRC_DESC_META_INFO_ */
    uint32_t write_index;
    uint32_t flags; /* %HAL_CE_SRC_DESC_FLAGS_ */
}__attribute__((packed));

int wireless_hal_reg_handler(struct wireless_simu_device_state *wd, hwaddr addr, uint32_t val);

void wireless_hal_src_ring_tp(gpointer data, gpointer user_data);

#endif