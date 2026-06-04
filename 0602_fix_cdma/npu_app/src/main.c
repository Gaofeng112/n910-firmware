#include <stdint.h>
#include <stddef.h>

#include "platform.h"

#include "AddrMap.h"
#include "Interrupt/interrupt.h"

#if defined(__has_include)
#if __has_include("xil_printf.h")
#include "xil_printf.h"
#define HAVE_XIL_PRINTF 1
#else
#define HAVE_XIL_PRINTF 0
#endif
#if __has_include("xil_io.h")
#include "xil_io.h"
#define HAVE_XIL_IO 1
#else
#define HAVE_XIL_IO 0
#endif

#if (HAVE_XIL_PRINTF == 0)
#include <stdio.h>
#define xil_printf printf
#endif
#else
#include <stdio.h>
#define xil_printf printf
#define HAVE_XIL_PRINTF 0
#define HAVE_XIL_IO 0
#endif

#if defined(__GNUC__)
#define MAYBE_UNUSED __attribute__((unused))
#else
#define MAYBE_UNUSED
#endif

#define CM_REG_DMAC_EN_IDX            0x00UL
#define CM_REG_DMAC_GO_IDX            0x01UL
#define CM_REG_DMAC_ABORT_DBG_IDX     0x02UL
#define CM_REG_DMAC_PAUSE_DBG_IDX     0x03UL
#define CM_REG_CDMA_SAR_IDX           0x04UL
#define CM_REG_CDMA_DAR_IDX           0x05UL
#define CM_REG_CDMA_LEN_IDX           0x06UL
#define CM_REG_CDMA_CFG_IDX           0x07UL
#define CM_REG_CDMA_INTMASK_DBG_IDX   0x08UL
#define CM_REG_CDMA_INTERRUPT_IDX     0x09UL
#define CM_REG_CDMA_DRAM_AR_DBG_IDX   0x0AUL
#define CM_REG_CDMA_SRAM_AR_DBG_IDX   0x0BUL
#define CM_REG_CDMA_FSM_DBG_IDX       0x0CUL

#define NPU_REG_CTRL1_EN_IDX          0x00UL
#define NPU_REG_CTRL2_GO_IDX          0x01UL

#define AXI_INTC_ISR_OFFSET           0x00UL
#define AXI_INTC_IPR_OFFSET           0x04UL
#define AXI_INTC_IER_OFFSET           0x08UL
#define AXI_INTC_IAR_OFFSET           0x0CUL
#define AXI_INTC_MER_OFFSET           0x1CUL

#define NPU_REG_DMAC_EN_IDX           0x18UL
#define NPU_REG_CH1_CH_NUM_IDX        0x23UL
#define NPU_REG_CH1_ROW_NUM_IDX       0x29UL
#define NPU_REG_CH1_CFG_IDX           0x30UL

#define WORD(addr, idx) (*(volatile uint32_t *)((uintptr_t)(addr) + ((uintptr_t)(idx) * sizeof(uint32_t))))

#define GRAPH_CONFIG_MAX_DATA_NODES   8U
#define GRAPH_OPFLOW_MAX_DATA_NODES   32U
#define GRAPH_CMD_ORIG_ADDR           0x40000000UL
#define GRAPH_WEIGHT_ORIG_ADDR        0x50000000UL
#define GRAPH_WEIGHT_OLD_BOARD_ADDR   0x84000000UL
#define GRAPH_DATA_ORIG_ADDR_1        0x60000000UL
#define GRAPH_DATA_ORIG_ADDR_2        0x60200000UL
#define GRAPH_DATA_ORIG_ADDR_ALT_1    0x70000000UL
#define GRAPH_DATA_ORIG_ADDR_ALT_2    0x70003200UL

#define TB_NODE_GAP_CYCLES            2000UL
#define TB_NPU_EN_GO_GAP_CYCLES       20000UL
#define CDMA_INTC_MASK                (1UL << 3)

typedef struct {
    uint32_t config_size;
    uint32_t config_opflow_size;
    uint32_t opflow_size;
    uint32_t cmd_size;
    uint32_t weight_size;
    uint32_t data_size;
    uint32_t golden_size;
    uint32_t net_size;
} case_sizes_t;

typedef struct {
    uint32_t is_multilayer;
    uint32_t little_endian;
    uint32_t cmd_addr;
    uint32_t weight_addr;
    uint32_t datain_num;
    uint32_t datain_index[GRAPH_CONFIG_MAX_DATA_NODES];
    uint32_t dataout_num;
    uint32_t dataout_index[GRAPH_CONFIG_MAX_DATA_NODES];
} graph_config_t;

typedef struct {
    uint32_t index;
    uint32_t addr;
} data_node_info_t;

static inline uintptr_t reg_addr_by_idx(uintptr_t base, uint32_t idx)
{
    return base + ((uintptr_t)idx * REG_STRIDE_BYTES);
}

static inline void reg_write32(uintptr_t addr, uint32_t val)
{
#if (HAVE_XIL_IO == 1)
    Xil_Out32(addr, val);
#else
    *(volatile uint32_t *)(uintptr_t)addr = val;
#endif
}

static inline uint32_t reg_read32(uintptr_t addr)
{
#if (HAVE_XIL_IO == 1)
    return Xil_In32(addr);
#else
    return *(volatile uint32_t *)(uintptr_t)addr;
#endif
}

static void tb_wait_cycles(uint32_t cycles)
{
    volatile uint32_t i;

    for (i = 0U; i < cycles; ++i) {
        __asm__ volatile ("nop");
    }
}

static void intc_ack_mask(uint32_t mask)
{
    reg_write32(AXI_INTC_BASE_ADDR + AXI_INTC_IAR_OFFSET, mask);
}

static uint32_t case_desc_read(uint32_t idx)
{
    return reg_read32(CASE_SIZE_DESC_ADDR + ((uintptr_t)idx * sizeof(uint32_t)));
}

static void case_status_write(uint32_t status)
{
    reg_write32(CASE_STATUS_ADDR, status);
}

static uint32_t choose_size(uint32_t runtime_size, uint32_t fallback_size)
{
    return (runtime_size == 0U) ? fallback_size : runtime_size;
}

static void load_case_sizes(case_sizes_t *sizes)
{
    uint32_t magic = case_desc_read(CASE_SIZE_MAGIC_IDX);
    uint32_t version = case_desc_read(CASE_SIZE_VERSION_IDX);

    sizes->config_size = CONFIG_BIN_SIZE_BYTES;
    sizes->config_opflow_size = CONFIG_OPFLOW_SIZE_BYTES;
    sizes->opflow_size = OPFLOW_BIN_SIZE_BYTES;
    sizes->cmd_size = CMD_BIN_SIZE_BYTES;
    sizes->weight_size = WEIGHT_BIN_SIZE_BYTES;
    sizes->data_size = DATA_BIN_SIZE_BYTES;
    sizes->golden_size = GOLDEN_BIN_SIZE_BYTES;
    sizes->net_size = NET_BIN_SIZE_BYTES;

    if ((magic != CASE_SIZE_MAGIC) || (version != CASE_SIZE_VERSION)) {
        xil_printf("[CASE] size descriptor missing, use fallback sizes\r\n");
        return;
    }

    sizes->config_size = choose_size(case_desc_read(CASE_SIZE_CONFIG_IDX), CONFIG_BIN_SIZE_BYTES);
    sizes->config_opflow_size = choose_size(case_desc_read(CASE_SIZE_CONFIG_OPFLOW_IDX), CONFIG_OPFLOW_SIZE_BYTES);
    sizes->opflow_size = choose_size(case_desc_read(CASE_SIZE_OPFLOW_IDX), OPFLOW_BIN_SIZE_BYTES);
    sizes->cmd_size = choose_size(case_desc_read(CASE_SIZE_CMD_IDX), CMD_BIN_SIZE_BYTES);
    sizes->weight_size = choose_size(case_desc_read(CASE_SIZE_WEIGHT_IDX), WEIGHT_BIN_SIZE_BYTES);
    sizes->data_size = choose_size(case_desc_read(CASE_SIZE_DATA_IDX), DATA_BIN_SIZE_BYTES);
    sizes->golden_size = choose_size(case_desc_read(CASE_SIZE_GOLDEN_IDX), GOLDEN_BIN_SIZE_BYTES);
    sizes->net_size = choose_size(case_desc_read(CASE_SIZE_NET_IDX), NET_BIN_SIZE_BYTES);

    xil_printf("[CASE] sizes cfg=%lu cfg_op=%lu opflow=%lu cmd=%lu weight=%lu data=%lu golden=%lu net=%lu\r\n",
               (unsigned long)sizes->config_size,
               (unsigned long)sizes->config_opflow_size,
               (unsigned long)sizes->opflow_size,
               (unsigned long)sizes->cmd_size,
               (unsigned long)sizes->weight_size,
               (unsigned long)sizes->data_size,
               (unsigned long)sizes->golden_size,
               (unsigned long)sizes->net_size);
}

static uint32_t read_be32_mem(uintptr_t addr)
{
    uint32_t raw = *(volatile uint32_t *)(uintptr_t)addr;

    return ((raw & 0x000000FFUL) << 24) |
           ((raw & 0x0000FF00UL) << 8) |
           ((raw & 0x00FF0000UL) >> 8) |
           ((raw & 0xFF000000UL) >> 24);
}

static uint32_t read_config_word(uintptr_t config_addr, uint32_t idx, uint32_t little_endian)
{
    uintptr_t addr = config_addr + ((uintptr_t)idx * sizeof(uint32_t));

    if (little_endian != 0U) {
        return *(volatile uint32_t *)addr;
    }

    return read_be32_mem(addr);
}

static uint32_t graph_addr_to_board(uint32_t addr)
{
    if (addr == GRAPH_CMD_ORIG_ADDR) {
        return DRAM_CMD_START_ADDR;
    }
    if ((addr == GRAPH_WEIGHT_ORIG_ADDR) || (addr == GRAPH_WEIGHT_OLD_BOARD_ADDR)) {
    // if (addr == GRAPH_WEIGHT_ORIG_ADDR) {
        return DRAM_WEIGHT_START_ADDR;
    }
    if ((addr == GRAPH_DATA_ORIG_ADDR_1) || (addr == GRAPH_DATA_ORIG_ADDR_ALT_1)) {
        return DRAM_DATA_START_ADDR_1;
    }
    if ((addr == GRAPH_DATA_ORIG_ADDR_2) || (addr == GRAPH_DATA_ORIG_ADDR_ALT_2)) {
        return DRAM_DATA_START_ADDR_2;
    }

    return addr;
}

static int parse_graph_config(uintptr_t config_addr, uint32_t config_size, graph_config_t *cfg)
{
    uint32_t i;
    uint32_t pos;
    uint32_t first_be;
    uint32_t first_le;
    uint32_t words = config_size / sizeof(uint32_t);

    cfg->is_multilayer = 0U;
    cfg->little_endian = 0U;
    cfg->cmd_addr = DRAM_CMD_START_ADDR;
    cfg->weight_addr = DRAM_WEIGHT_START_ADDR;
    cfg->datain_num = 0U;
    cfg->dataout_num = 0U;
    for (i = 0U; i < GRAPH_CONFIG_MAX_DATA_NODES; ++i) {
        cfg->datain_index[i] = 0U;
        cfg->dataout_index[i] = 0U;
    }

    if ((config_size < (7U * sizeof(uint32_t))) || ((config_size & 0x3U) != 0U)) {
        return 0;
    }

    first_be = read_be32_mem(config_addr);
    first_le = *(volatile uint32_t *)config_addr;
    if (first_be == 1U) {
        cfg->little_endian = 0U;
    } else if (first_le == 1U) {
        cfg->little_endian = 1U;
    } else {
        return 0;
    }

    cfg->is_multilayer = 1U;
    cfg->cmd_addr = graph_addr_to_board(read_config_word(config_addr, 1U, cfg->little_endian));
    cfg->weight_addr = graph_addr_to_board(read_config_word(config_addr, 2U, cfg->little_endian));
    cfg->datain_num = read_config_word(config_addr, 3U, cfg->little_endian);

    if (cfg->datain_num > GRAPH_CONFIG_MAX_DATA_NODES) {
        xil_printf("[CFG] too many input data nodes=%lu\r\n", (unsigned long)cfg->datain_num);
        return -1;
    }

    pos = 4U;
    if ((pos + cfg->datain_num) >= words) {
        xil_printf("[CFG] bad input node list size words=%lu\r\n", (unsigned long)words);
        return -2;
    }

    for (i = 0U; i < cfg->datain_num; ++i) {
        cfg->datain_index[i] = read_config_word(config_addr, pos + i, cfg->little_endian);
    }

    pos += cfg->datain_num;
    cfg->dataout_num = read_config_word(config_addr, pos, cfg->little_endian);
    pos++;

    if (cfg->dataout_num > GRAPH_CONFIG_MAX_DATA_NODES) {
        xil_printf("[CFG] too many output data nodes=%lu\r\n", (unsigned long)cfg->dataout_num);
        return -3;
    }

    if ((pos + cfg->dataout_num) > words) {
        xil_printf("[CFG] bad output node list size words=%lu\r\n", (unsigned long)words);
        return -4;
    }

    for (i = 0U; i < cfg->dataout_num; ++i) {
        cfg->dataout_index[i] = read_config_word(config_addr, pos + i, cfg->little_endian);
    }

    xil_printf("[CFG] multilayer endian=%s cmd=0x%08lx weight=0x%08lx in_num=%lu out_num=%lu\r\n",
               (cfg->little_endian != 0U) ? "LE" : "BE",
               (unsigned long)cfg->cmd_addr,
               (unsigned long)cfg->weight_addr,
               (unsigned long)cfg->datain_num,
               (unsigned long)cfg->dataout_num);
    for (i = 0U; i < cfg->datain_num; ++i) {
        xil_printf("[CFG] input node[%lu]=0x%08lx\r\n", (unsigned long)i, (unsigned long)cfg->datain_index[i]);
    }
    for (i = 0U; i < cfg->dataout_num; ++i) {
        xil_printf("[CFG] output node[%lu]=0x%08lx\r\n", (unsigned long)i, (unsigned long)cfg->dataout_index[i]);
    }

    return 1;
}

static void apply_config_bin(uintptr_t config_addr, uint32_t config_size)
{
    uint32_t i;
    uint32_t words = config_size / sizeof(uint32_t);

    xil_printf("[CFG] apply addr=0x%08lx size=%lu\r\n",
               (unsigned long)config_addr,
               (unsigned long)config_size);

    for (i = 0U; i < words; ++i) {
        uint32_t raw = read_be32_mem(config_addr + ((uintptr_t)i * sizeof(uint32_t)));
        uint32_t reg_idx = (raw >> 16) & 0xFFFFU;
        uint32_t reg_val = raw & 0xFFFFU;

        reg_write32(reg_addr_by_idx(NPUREG_BASE_ADDR, reg_idx), reg_val);
    }
}

static int compare_output_golden(uintptr_t output_addr, uintptr_t golden_addr, uint32_t size)
{
    uint32_t i;
    uint32_t words = size / sizeof(uint32_t);
    uint32_t err = 0U;

    xil_printf("[CMP] output=0x%08lx golden=0x%08lx size=%lu\r\n",
               (unsigned long)output_addr,
               (unsigned long)golden_addr,
               (unsigned long)size);

    for (i = 0U; i < words; ++i) {
        uint32_t out = *(volatile uint32_t *)(output_addr + ((uintptr_t)i * sizeof(uint32_t)));
        uint32_t gold = *(volatile uint32_t *)(golden_addr + ((uintptr_t)i * sizeof(uint32_t)));

        if (out != gold) {
            if (err < 8U) {
                xil_printf("[CMP] mismatch word=%lu out=0x%08lx gold=0x%08lx\r\n",
                           (unsigned long)i,
                           (unsigned long)out,
                           (unsigned long)gold);
            }
            err++;
        }
    }

    if (err == 0U) {
        xil_printf("[CMP] PASS\r\n");
        case_status_write(CASE_STATUS_PASS);
        return 0;
    }

    xil_printf("[CMP] FAIL err_words=%lu\r\n", (unsigned long)err);
    case_status_write(CASE_STATUS_FAIL);
    return -1;
}

static void dump_hw_status(const char *tag, uint32_t loop_i)
{
    uint32_t isr = reg_read32(AXI_INTC_BASE_ADDR + AXI_INTC_ISR_OFFSET);
    uint32_t ipr = reg_read32(AXI_INTC_BASE_ADDR + AXI_INTC_IPR_OFFSET);
    uint32_t ier = reg_read32(AXI_INTC_BASE_ADDR + AXI_INTC_IER_OFFSET);
    uint32_t mer = reg_read32(AXI_INTC_BASE_ADDR + AXI_INTC_MER_OFFSET);
    uint32_t cdma_intr = reg_read32(reg_addr_by_idx(CM_CDMA_BASE_ADDR, CM_REG_CDMA_INTERRUPT_IDX));
    uint32_t cdma_fsm = reg_read32(reg_addr_by_idx(CM_CDMA_BASE_ADDR, CM_REG_CDMA_FSM_DBG_IDX));
    uint32_t npu_en = reg_read32(reg_addr_by_idx(NPUREG_BASE_ADDR, NPU_REG_CTRL1_EN_IDX));
    uint32_t npu_go = reg_read32(reg_addr_by_idx(NPUREG_BASE_ADDR, NPU_REG_CTRL2_GO_IDX));
    uint32_t dmac_en = reg_read32(reg_addr_by_idx(NPUREG_BASE_ADDR, NPU_REG_DMAC_EN_IDX));
    uint32_t ch1_cfg = reg_read32(reg_addr_by_idx(NPUREG_BASE_ADDR, NPU_REG_CH1_CFG_IDX));
    uint32_t ch1_ch_num = reg_read32(reg_addr_by_idx(NPUREG_BASE_ADDR, NPU_REG_CH1_CH_NUM_IDX));
    uint32_t ch1_row_num = reg_read32(reg_addr_by_idx(NPUREG_BASE_ADDR, NPU_REG_CH1_ROW_NUM_IDX));

    xil_printf("[STAT][%s] i=%lu ISR=0x%08lx IPR=0x%08lx IER=0x%08lx MER=0x%08lx CDMA_INTR=0x%08lx CDMA_FSM=0x%08lx NPU_EN=0x%08lx NPU_GO=0x%08lx DMAC_EN=0x%08lx CH1_CFG=0x%08lx CH1_CH=0x%08lx CH1_ROW=0x%08lx\r\n",
               tag,
               (unsigned long)loop_i,
               (unsigned long)isr,
               (unsigned long)ipr,
               (unsigned long)ier,
               (unsigned long)mer,
               (unsigned long)cdma_intr,
               (unsigned long)cdma_fsm,
               (unsigned long)npu_en,
               (unsigned long)npu_go,
               (unsigned long)dmac_en,
               (unsigned long)ch1_cfg,
               (unsigned long)ch1_ch_num,
               (unsigned long)ch1_row_num);
    xil_printf("[IRQ_BITS][%s] WDMA/DODMA(bit0)=%lu DDMA/DIDMA(bit1)=%lu GPDMA(bit2)=%lu CM_CDMA(bit3)=%lu NPU_DONE(11:4)=0x%02lx ISR=0x%08lx IPR=0x%08lx IER=0x%08lx\r\n",
               tag,
               (unsigned long)((isr >> 0) & 0x1U),
               (unsigned long)((isr >> 1) & 0x1U),
               (unsigned long)((isr >> 2) & 0x1U),
               (unsigned long)((isr >> 3) & 0x1U),
               (unsigned long)((isr >> 4) & 0xFFU),
               (unsigned long)isr,
               (unsigned long)ipr,
               (unsigned long)ier);

}

static int wait_cm_cdma_done(void)
{
    uint32_t i;

    for (i = 0U; i < CDMA_TIMEOUT_CYCLES; ++i) {
        uint32_t intr = reg_read32(reg_addr_by_idx(CM_CDMA_BASE_ADDR, CM_REG_CDMA_INTERRUPT_IDX));
        uint32_t isr = reg_read32(AXI_INTC_BASE_ADDR + AXI_INTC_ISR_OFFSET);

        if (((intr & 0x1U) != 0U) || ((isr & CDMA_INTC_MASK) != 0U)) {
            if ((intr & 0x1U) == 0U) {
                (void)reg_read32(reg_addr_by_idx(CM_CDMA_BASE_ADDR, CM_REG_CDMA_INTERRUPT_IDX));
            }
            intc_ack_mask(CDMA_INTC_MASK);
            xil_printf("[CDMA] done intr=0x%08lx isr=0x%08lx\r\n",
                       (unsigned long)intr,
                       (unsigned long)isr);
            return 0;
        }
    }

    xil_printf("[CDMA] timeout\r\n");
    return -1;
}

static int wait_npu_done_intc(void)
{
    int status = InterruptWaitNpuDone(NPU_DONE_INTC_MASK, NPU_TIMEOUT_CYCLES);
    uint32_t done_mask = InterruptGetNpuDoneMask();

    if (status == 0) {
        xil_printf("[NPU] interrupt done mask=0x%08lx\r\n", (unsigned long)done_mask);
        return 0;
    }

    xil_printf("[NPU] interrupt timeout mask=0x%08lx ISR=0x%08lx\r\n",
               (unsigned long)done_mask,
               (unsigned long)reg_read32(AXI_INTC_BASE_ADDR + AXI_INTC_ISR_OFFSET));
    dump_hw_status("npu_irq_timeout", 0U);
    return -1;
}

static int run_cdma_node(uint32_t sar, uint32_t len, uint32_t cmd_addr, const case_sizes_t *sizes)
{
    uintptr_t cmd_end = (uintptr_t)cmd_addr + (uintptr_t)sizes->cmd_size;
    uintptr_t node_end = (uintptr_t)sar + (uintptr_t)len;

    if ((len == 0U) || (len > CMD_LEN_MAX_BYTES) || ((len & 0x3U) != 0U)) {
        xil_printf("[CDMA] invalid len=%lu\r\n", (unsigned long)len);
        return -1;
    }

    if (((uintptr_t)sar < (uintptr_t)cmd_addr) || (node_end < (uintptr_t)sar) || (node_end > cmd_end)) {
        xil_printf("[CDMA] range error sar=0x%08lx len=%lu cmd=[0x%08lx,0x%08lx)\r\n",
                   (unsigned long)sar,
                   (unsigned long)len,
                   (unsigned long)cmd_addr,
                   (unsigned long)cmd_end);
        return -2;
    }

    tb_wait_cycles(TB_NODE_GAP_CYCLES);
    intc_ack_mask(CDMA_INTC_MASK);
    (void)reg_read32(reg_addr_by_idx(CM_CDMA_BASE_ADDR, CM_REG_CDMA_INTERRUPT_IDX));

    reg_write32(reg_addr_by_idx(CM_CDMA_BASE_ADDR, CM_REG_DMAC_EN_IDX), 0x1U);
    reg_write32(reg_addr_by_idx(CM_CDMA_BASE_ADDR, CM_REG_CDMA_SAR_IDX), sar);
    (void)reg_read32(reg_addr_by_idx(CM_CDMA_BASE_ADDR, CM_REG_CDMA_SAR_IDX));
    reg_write32(reg_addr_by_idx(CM_CDMA_BASE_ADDR, CM_REG_CDMA_DAR_IDX), CDMA_DAR_RESERVED_VALUE);
    reg_write32(reg_addr_by_idx(CM_CDMA_BASE_ADDR, CM_REG_CDMA_LEN_IDX), len);
    reg_write32(reg_addr_by_idx(CM_CDMA_BASE_ADDR, CM_REG_CDMA_CFG_IDX), 0x0U);
    dump_hw_status("before_cdma_go", 0U);
    reg_write32(reg_addr_by_idx(CM_CDMA_BASE_ADDR, CM_REG_DMAC_GO_IDX), 0x1U);

    xil_printf("[CDMA] sar=0x%08lx dar=0x%08lx len=%lu go\r\n",
               (unsigned long)sar,
               (unsigned long)CDMA_DAR_RESERVED_VALUE,
               (unsigned long)len);

    if (wait_cm_cdma_done() != 0) {
        return -3;
    }

    dump_hw_status("after_cdma_done", 0U);
    return 0;
}

static int run_npu_node(void)
{
    tb_wait_cycles(TB_NODE_GAP_CYCLES);
    InterruptClearNpuDone();
    intc_ack_mask(NPU_DONE_INTC_MASK);
    reg_write32(reg_addr_by_idx(NPUREG_BASE_ADDR, NPU_REG_CTRL1_EN_IDX), NPU_CTRL_EN_VALUE);
    dump_hw_status("after_npu_en", 0U);
    tb_wait_cycles(TB_NPU_EN_GO_GAP_CYCLES);
    reg_write32(reg_addr_by_idx(NPUREG_BASE_ADDR, NPU_REG_CTRL2_GO_IDX), NPU_CTRL_GO_VALUE);
    dump_hw_status("after_npu_go", 0U);
    xil_printf("[NPU] en/go sent, waiting done mask=0x%08lx\r\n", (unsigned long)NPU_DONE_INTC_MASK);

    if (wait_npu_done_intc() != 0) {
        return -1;
    }

    return 0;
}

static int MAYBE_UNUSED run_npu_once(const case_sizes_t *sizes)
{
    (void)sizes;
    xil_printf("[RUN] run_npu_once disabled; use opflow executor\r\n");
    return -100;
}

static int find_data_node_addr(const data_node_info_t *nodes,
                               uint32_t count,
                               uint32_t index,
                               uint32_t *addr)
{
    uint32_t i;

    for (i = 0U; i < count; ++i) {
        if (nodes[i].index == index) {
            *addr = nodes[i].addr;
            return 0;
        }
    }

    return -1;
}

static int parse_opflow_and_run(uintptr_t opflow_addr, uint32_t opflow_size, const case_sizes_t *sizes)
{
    uintptr_t addr = opflow_addr;
    uintptr_t end = opflow_addr + opflow_size;
    uint32_t cdma_count = 0U;
    uint32_t npu_count = 0U;
    graph_config_t graph_cfg;
    int graph_cfg_status;
    uint32_t cmd_addr = DRAM_CMD_START_ADDR;
    data_node_info_t data_nodes[GRAPH_OPFLOW_MAX_DATA_NODES];
    uint32_t data_node_count = 0U;
    uint32_t output_addr = DRAM_DATA_START_ADDR_2;

    xil_printf("[OPFLOW] parse addr=0x%08lx size=%lu\r\n",
               (unsigned long)opflow_addr,
               (unsigned long)opflow_size);

    if ((opflow_size == 0U) || ((opflow_size & 0x3U) != 0U)) {
        xil_printf("[OPFLOW] invalid size=%lu\r\n", (unsigned long)opflow_size);
        return -1;
    }

    graph_cfg_status = parse_graph_config(DRAM_CONFIG_START_ADDR, sizes->config_size, &graph_cfg);
    if (graph_cfg_status < 0) {
        return -11;
    }
    if (graph_cfg.is_multilayer != 0U) {
        cmd_addr = graph_cfg.cmd_addr;
    } else {
        xil_printf("[CFG] skip firmware apply_config_bin; config by cmd stream\r\n");
    }

    while (addr < end) {
        uint32_t index = WORD(addr, 0U);
        uint32_t node_type = WORD(addr, 1U);

        if (node_type == 0U) {
            uint32_t opcode;
            uint32_t sar;
            uint32_t len;

            if ((addr + (6U * sizeof(uint32_t))) > end) {
                xil_printf("[OPFLOW] short cdma node addr=0x%08lx idx=%lu\r\n",
                           (unsigned long)addr,
                           (unsigned long)index);
                return -2;
            }

            opcode = WORD(addr, 3U);
            if (opcode != 2U) {
                xil_printf("[OPFLOW] unsupported cpu node idx=%lu opcode=%lu\r\n",
                           (unsigned long)index,
                           (unsigned long)opcode);
                return -3;
            }

            sar = WORD(addr, 4U);
            len = WORD(addr, 5U);
            xil_printf("[OPFLOW] cdma idx=%lu sar=0x%08lx len=%lu\r\n",
                       (unsigned long)index,
                       (unsigned long)sar,
                       (unsigned long)len);

            if (run_cdma_node(sar, len, cmd_addr, sizes) != 0) {
                return -4;
            }

            addr += 6U * sizeof(uint32_t);
            cdma_count++;
            continue;
        }

        if (node_type == 1U) {
            if ((addr + (3U * sizeof(uint32_t))) > end) {
                xil_printf("[OPFLOW] short npu node addr=0x%08lx idx=%lu\r\n",
                           (unsigned long)addr,
                           (unsigned long)index);
                return -5;
            }

            xil_printf("[OPFLOW] npu idx=%lu\r\n", (unsigned long)index);
            if (run_npu_node() != 0) {
                return -6;
            }

            addr += 3U * sizeof(uint32_t);
            npu_count++;
            continue;
        }

        if (node_type == 2U) {
            uint32_t data_addr;

            if ((addr + (7U * sizeof(uint32_t))) > end) {
                xil_printf("[OPFLOW] short data node addr=0x%08lx idx=%lu\r\n",
                           (unsigned long)addr,
                           (unsigned long)index);
                return -7;
            }

            data_addr = graph_addr_to_board(WORD(addr, 2U));

            xil_printf("[OPFLOW] data idx=%lu addr=0x%08lx fmt=%lu h=%lu w=%lu c=%lu\r\n",
                       (unsigned long)index,
                       (unsigned long)data_addr,
                       (unsigned long)WORD(addr, 3U),
                       (unsigned long)WORD(addr, 4U),
                       (unsigned long)WORD(addr, 5U),
                       (unsigned long)WORD(addr, 6U));

            if (data_node_count >= GRAPH_OPFLOW_MAX_DATA_NODES) {
                xil_printf("[OPFLOW] too many data nodes\r\n");
                return -12;
            }

            data_nodes[data_node_count].index = index;
            data_nodes[data_node_count].addr = data_addr;
            data_node_count++;

            addr += 7U * sizeof(uint32_t);
            continue;
        }

        xil_printf("[OPFLOW] unsupported node idx=%lu type=%lu\r\n",
                   (unsigned long)index,
                   (unsigned long)node_type);
        return -8;
    }

    xil_printf("[OPFLOW] done cdma=%lu npu=%lu\r\n",
               (unsigned long)cdma_count,
               (unsigned long)npu_count);

    if ((cdma_count == 0U) || (npu_count == 0U)) {
        xil_printf("[OPFLOW] missing executable nodes\r\n");
        return -9;
    }

    if ((graph_cfg.is_multilayer != 0U) && (graph_cfg.dataout_num > 0U)) {
        if (find_data_node_addr(data_nodes,
                                data_node_count,
                                graph_cfg.dataout_index[0],
                                &output_addr) != 0) {
            xil_printf("[OPFLOW] output data node 0x%08lx not found\r\n",
                       (unsigned long)graph_cfg.dataout_index[0]);
            return -13;
        }
    }

    xil_printf("[OPFLOW] compare output node addr=0x%08lx\r\n", (unsigned long)output_addr);

    if (compare_output_golden(output_addr,
                              DRAM_GOLDEN_START_ADDR,
                              sizes->golden_size) != 0) {
        return -10;
    }

    return 0;
}

int main(void)
{
    int ret;
    case_sizes_t sizes;

    init_platform();
    case_status_write(CASE_STATUS_RUNNING);
    load_case_sizes(&sizes);

    xil_printf("[MIN_FW] start\r\n");
    xil_printf("[MIN_FW] config=0x%08lx opflow=0x%08lx cmd=0x%08lx\r\n",
               (unsigned long)DRAM_CONFIG_START_ADDR,
               (unsigned long)DRAM_OPFLOW_START_ADDR,
               (unsigned long)DRAM_CMD_START_ADDR);

    if (InterruptInitAll() != 0) {
        xil_printf("[MIN_FW] interrupt init failed\r\n");
        case_status_write(CASE_STATUS_FAIL);
        cleanup_platform();
        return -1;
    }
    dump_hw_status("after_intc_init", 0U);

    ret = parse_opflow_and_run(DRAM_OPFLOW_START_ADDR, sizes.opflow_size, &sizes);
    if (ret != 0) {
        xil_printf("[MIN_FW] fail ret=%d\r\n", ret);
        case_status_write(CASE_STATUS_FAIL);
        cleanup_platform();
        return ret;
    }

    xil_printf("[MIN_FW] done\r\n");
    case_status_write(CASE_STATUS_PASS);

    cleanup_platform();
    return 0;
}
