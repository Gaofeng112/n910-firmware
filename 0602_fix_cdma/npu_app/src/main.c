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

#define GRAPH_CONFIG_MAX_DATA_NODES   8U
#define GRAPH_OPFLOW_MAX_DATA_NODES   128U
#define GRAPH_CPU_MAX_INPUT_NODES     8U
#define GRAPH_CMD_ORIG_ADDR           0x40000000UL
#define GRAPH_WEIGHT_ORIG_ADDR        0x50000000UL
#define GRAPH_WEIGHT_OLD_BOARD_ADDR   0x84000000UL
#define GRAPH_DATA_ORIG_ADDR_1        0x60000000UL
#define GRAPH_DATA_ORIG_ADDR_2        0x60200000UL
#define GRAPH_DATA_ORIG_ADDR_ALT_1    0x70000000UL
#define GRAPH_DATA_ORIG_ADDR_ALT_2    0x70003200UL
#define GRAPH_ADDR_LARGE_WINDOW       0x10000000UL
#define GRAPH_DATA_ADDR_WINDOW        0x10000000UL

#define TB_NODE_GAP_CYCLES            2000UL
#define TB_NPU_EN_GO_GAP_CYCLES       20000UL
#define CDMA_INTC_MASK                (1UL << 3)

#define OP_CDMA                       2U
#define OP_LAYER_NORM                 23U
#define LAYERNORM_PARAM_MAX_RANK      8U
#define CPU_COMPARE_START_ADDR        DRAM_NET_START_ADDR

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
    uint32_t raw_addr;
    uint32_t addr;
    uint32_t format;
    uint32_t height;
    uint32_t width;
    uint32_t channel;
} data_node_info_t;

typedef struct {
    uint32_t index;
    uint32_t next_node_idx;
    uint32_t opcode_id;
    uint32_t num_of_input_nodes;
    uint32_t input_nodes_idx[GRAPH_CPU_MAX_INPUT_NODES];
    uint32_t output_node_idx;
    uint32_t start_addr;
} cpu_node_info_t;

typedef struct {
    int32_t axis;
    int32_t stash_type;
    uint32_t epsilon_word;
    uint32_t x_radix_num;
    uintptr_t x_radix_addr;
    uint32_t y_radix_num;
    uintptr_t y_radix_addr;
    uint32_t scale_rank;
    uint32_t scale_shape[LAYERNORM_PARAM_MAX_RANK];
    uint32_t scale_len;
    uintptr_t scale_addr;
    uint32_t bias_rank;
    uint32_t bias_shape[LAYERNORM_PARAM_MAX_RANK];
    uint32_t bias_len;
    uintptr_t bias_addr;
} layernorm_param_info_t;

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
    sizes->golden_size = case_desc_read(CASE_SIZE_GOLDEN_IDX);
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

static uint32_t read_graph_word(uintptr_t addr, uint32_t idx, uint32_t little_endian)
{
    uintptr_t word_addr = addr + ((uintptr_t)idx * sizeof(uint32_t));

    if (little_endian != 0U) {
        return *(volatile uint32_t *)word_addr;
    }

    return read_be32_mem(word_addr);
}

static uint32_t read_graph_u32_at(uintptr_t addr, uint32_t byte_offset, uint32_t little_endian)
{
    uintptr_t word_addr = addr + (uintptr_t)byte_offset;

    if (little_endian != 0U) {
        return *(volatile uint32_t *)word_addr;
    }

    return read_be32_mem(word_addr);
}

static float word_to_float(uint32_t word)
{
    union {
        uint32_t word;
        float val;
    } raw;

    raw.word = word;
    return raw.val;
}

static float read_graph_float_at(uintptr_t addr, uint32_t byte_offset, uint32_t little_endian)
{
    return word_to_float(read_graph_u32_at(addr, byte_offset, little_endian));
}

static uint32_t graph_addr_range_to_board(uint32_t addr,
                                          uint32_t graph_base,
                                          uint32_t board_base,
                                          uint32_t window_size)
{
    if ((addr >= graph_base) && (addr < (graph_base + window_size))) {
        return board_base + (addr - graph_base);
    }

    return addr;
}

static uint32_t graph_addr_to_board(uint32_t addr)
{
    uint32_t mapped = graph_addr_range_to_board(addr,
                                                GRAPH_CMD_ORIG_ADDR,
                                                DRAM_CMD_START_ADDR,
                                                GRAPH_ADDR_LARGE_WINDOW);

    if (mapped != addr) {
        return mapped;
    }

    mapped = graph_addr_range_to_board(addr,
                                       GRAPH_WEIGHT_ORIG_ADDR,
                                       DRAM_WEIGHT_START_ADDR,
                                       GRAPH_ADDR_LARGE_WINDOW);
    if (mapped != addr) {
        return mapped;
    }

    mapped = graph_addr_range_to_board(addr,
                                       GRAPH_WEIGHT_OLD_BOARD_ADDR,
                                       DRAM_WEIGHT_START_ADDR,
                                       GRAPH_ADDR_LARGE_WINDOW);
    if (mapped != addr) {
        return mapped;
    }

    mapped = graph_addr_range_to_board(addr,
                                       GRAPH_DATA_ORIG_ADDR_2,
                                       DRAM_DATA_START_ADDR_2,
                                       GRAPH_DATA_ADDR_WINDOW);
    if (mapped != addr) {
        return mapped;
    }

    if (addr == GRAPH_DATA_ORIG_ADDR_ALT_2) {
        return DRAM_DATA_START_ADDR_2;
    }

    mapped = graph_addr_range_to_board(addr,
                                       GRAPH_DATA_ORIG_ADDR_1,
                                       DRAM_DATA_START_ADDR_1,
                                       GRAPH_DATA_ADDR_WINDOW);
    if (mapped != addr) {
        return mapped;
    }

    mapped = graph_addr_range_to_board(addr,
                                       GRAPH_DATA_ORIG_ADDR_ALT_1,
                                       DRAM_DATA_START_ADDR_1,
                                       GRAPH_DATA_ADDR_WINDOW);
    if (mapped != addr) {
        return mapped;
    }

    return addr;
}

static MAYBE_UNUSED uint32_t get_dram_data_offset_format_1(uint32_t height,
                                                           uint32_t width,
                                                           uint32_t c,
                                                           uint32_t h,
                                                           uint32_t w)
{
    uint32_t c_group = c >> 5;
    uint32_t c_in_group = c & 0x1FU;
    uint32_t c_quad = (c_in_group >> 2) & 0x3U;
    uint32_t c_hi = c_in_group >> 4;
    uint32_t c_low = c & 0x3U;
    uint32_t offset = c_group;

    offset = (offset * height) + h;
    offset = (offset * width) + w;
    offset = (offset * 4U) + c_quad;
    offset = (offset * 2U) + c_hi;
    offset = (offset * 4U) + c_low;
    return offset;
}

static MAYBE_UNUSED float get_radix_float(int16_t radix)
{
    uint32_t i;
    float scale = 1.0f;

    if (radix > 0) {
        for (i = 0U; i < (uint32_t)radix; ++i) {
            scale *= 2.0f;
        }
    } else if (radix < 0) {
        for (i = 0U; i < (uint32_t)(-radix); ++i) {
            scale *= 0.5f;
        }
    }

    return scale;
}

static MAYBE_UNUSED int16_t float2short_round(float val)
{
    if (val >= 0.0f) {
        return (int16_t)(val + 0.5f);
    }

    return (int16_t)(val - 0.5f);
}

static MAYBE_UNUSED int16_t read_le16s_mem(uintptr_t addr)
{
    uint32_t lo = *(volatile uint8_t *)addr;
    uint32_t hi = *(volatile uint8_t *)(addr + 1U);

    return (int16_t)((hi << 8) | lo);
}

static MAYBE_UNUSED int16_t read_be16s_mem(uintptr_t addr)
{
    uint32_t hi = *(volatile uint8_t *)addr;
    uint32_t lo = *(volatile uint8_t *)(addr + 1U);

    return (int16_t)((hi << 8) | lo);
}

static int16_t read_graph_i16_at(uintptr_t addr, uint32_t byte_offset, uint32_t little_endian)
{
    uintptr_t half_addr = addr + (uintptr_t)byte_offset;

    if (little_endian != 0U) {
        return read_le16s_mem(half_addr);
    }

    return read_be16s_mem(half_addr);
}

static MAYBE_UNUSED void write_le16s_mem(uintptr_t addr, int16_t val)
{
    *(volatile uint8_t *)addr = (uint8_t)((uint16_t)val & 0xFFU);
    *(volatile uint8_t *)(addr + 1U) = (uint8_t)(((uint16_t)val >> 8) & 0xFFU);
}

static MAYBE_UNUSED int data_node_read_short_format_1(const data_node_info_t *node,
                                                      uint32_t c,
                                                      uint32_t h,
                                                      uint32_t w,
                                                      int16_t *val)
{
    uint32_t offset;

    if (node->format != 1U) {
        return -1;
    }

    offset = get_dram_data_offset_format_1(node->height, node->width, c, h, w);
    *val = read_le16s_mem((uintptr_t)node->addr + ((uintptr_t)offset * sizeof(int16_t)));
    return 0;
}

static MAYBE_UNUSED int data_node_write_short_format_1(const data_node_info_t *node,
                                                       uint32_t c,
                                                       uint32_t h,
                                                       uint32_t w,
                                                       int16_t val)
{
    uint32_t offset;

    if (node->format != 1U) {
        return -1;
    }

    offset = get_dram_data_offset_format_1(node->height, node->width, c, h, w);
    write_le16s_mem((uintptr_t)node->addr + ((uintptr_t)offset * sizeof(int16_t)), val);
    return 0;
}

static MAYBE_UNUSED int data_node_read_float_format_1(const data_node_info_t *node,
                                                      uint32_t c,
                                                      uint32_t h,
                                                      uint32_t w,
                                                      int16_t radix,
                                                      float *val)
{
    int16_t raw;

    if (data_node_read_short_format_1(node, c, h, w, &raw) != 0) {
        return -1;
    }

    *val = (float)raw / get_radix_float(radix);
    return 0;
}

static MAYBE_UNUSED int data_node_write_float_format_1(const data_node_info_t *node,
                                                       uint32_t c,
                                                       uint32_t h,
                                                       uint32_t w,
                                                       int16_t radix,
                                                       float val)
{
    int16_t raw = float2short_round(val * get_radix_float(radix));

    return data_node_write_short_format_1(node, c, h, w, raw);
}

static uint32_t data_node_element_count(const data_node_info_t *node)
{
    return node->height * node->width * node->channel;
}

static uint32_t data_node_normal_offset(const data_node_info_t *node,
                                        uint32_t c,
                                        uint32_t h,
                                        uint32_t w)
{
    return ((h * node->width) + w) * node->channel + c;
}

static void copy_bytes(uintptr_t dst, uintptr_t src, uint32_t size)
{
    uint32_t i;

    for (i = 0U; i < size; ++i) {
        *(volatile uint8_t *)(dst + (uintptr_t)i) = *(volatile uint8_t *)(src + (uintptr_t)i);
    }
}

static int data_node_normal_to_format_1(const data_node_info_t *node, uintptr_t normal_addr)
{
    uint32_t h;
    uint32_t w;
    uint32_t c;

    for (h = 0U; h < node->height; ++h) {
        for (w = 0U; w < node->width; ++w) {
            for (c = 0U; c < node->channel; ++c) {
                uint32_t normal_offset = data_node_normal_offset(node, c, h, w);
                int16_t raw = read_le16s_mem(normal_addr + ((uintptr_t)normal_offset * sizeof(int16_t)));

                if (data_node_write_short_format_1(node, c, h, w, raw) != 0) {
                    return -1;
                }
            }
        }
    }

    return 0;
}

static int data_node_format_1_to_normal(const data_node_info_t *node, uintptr_t normal_addr)
{
    uint32_t h;
    uint32_t w;
    uint32_t c;

    for (h = 0U; h < node->height; ++h) {
        for (w = 0U; w < node->width; ++w) {
            for (c = 0U; c < node->channel; ++c) {
                uint32_t normal_offset = data_node_normal_offset(node, c, h, w);
                int16_t raw;

                if (data_node_read_short_format_1(node, c, h, w, &raw) != 0) {
                    return -1;
                }
                write_le16s_mem(normal_addr + ((uintptr_t)normal_offset * sizeof(int16_t)), raw);
            }
        }
    }

    return 0;
}

static float layernorm_sqrtf(float val)
{
    uint32_t i;
    float x;

    if (val <= 0.0f) {
        return 0.0f;
    }

    x = (val > 1.0f) ? val : 1.0f;
    for (i = 0U; i < 16U; ++i) {
        x = 0.5f * (x + (val / x));
    }

    return x;
}

static int16_t layernorm_radix_at(uintptr_t radix_addr, uint32_t little_endian, uint32_t radix_num, uint32_t c)
{
    uint32_t idx = (radix_num == 1U) ? 0U : c;

    return read_graph_i16_at(radix_addr, idx * sizeof(int16_t), little_endian);
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

static const data_node_info_t *find_data_node(const data_node_info_t *nodes,
                                              uint32_t count,
                                              uint32_t index)
{
    uint32_t i;

    for (i = 0U; i < count; ++i) {
        if (nodes[i].index == index) {
            return &nodes[i];
        }
    }

    return NULL;
}

static int parse_cpu_node(uintptr_t addr,
                          uintptr_t end,
                          uint32_t little_endian,
                          cpu_node_info_t *node,
                          uint32_t *node_size)
{
    uint32_t i;
    uint32_t words;

    if ((addr + (5U * sizeof(uint32_t))) > end) {
        return -1;
    }

    node->index = read_graph_word(addr, 0U, little_endian);
    node->next_node_idx = read_graph_word(addr, 2U, little_endian);
    node->opcode_id = read_graph_word(addr, 3U, little_endian);
    node->num_of_input_nodes = read_graph_word(addr, 4U, little_endian);

    if (node->num_of_input_nodes > GRAPH_CPU_MAX_INPUT_NODES) {
        return -2;
    }

    words = 7U + node->num_of_input_nodes;
    if ((addr + ((uintptr_t)words * sizeof(uint32_t))) > end) {
        return -3;
    }

    for (i = 0U; i < node->num_of_input_nodes; ++i) {
        node->input_nodes_idx[i] = read_graph_word(addr, 5U + i, little_endian);
    }

    node->output_node_idx = read_graph_word(addr, 5U + node->num_of_input_nodes, little_endian);
    node->start_addr = graph_addr_to_board(read_graph_word(addr, 6U + node->num_of_input_nodes, little_endian));
    *node_size = words * sizeof(uint32_t);
    return 0;
}

static int parse_layernorm_tensor(uintptr_t param_addr,
                                  uint32_t little_endian,
                                  uint32_t *byte_offset,
                                  uint32_t *rank,
                                  uint32_t shape[LAYERNORM_PARAM_MAX_RANK],
                                  uint32_t *data_len,
                                  uintptr_t *data_addr)
{
    uint32_t i;

    *rank = read_graph_u32_at(param_addr, *byte_offset, little_endian);
    *byte_offset += sizeof(uint32_t);
    if (*rank > LAYERNORM_PARAM_MAX_RANK) {
        return -1;
    }

    for (i = 0U; i < *rank; ++i) {
        shape[i] = read_graph_u32_at(param_addr, *byte_offset, little_endian);
        *byte_offset += sizeof(uint32_t);
    }

    *data_len = read_graph_u32_at(param_addr, *byte_offset, little_endian);
    *byte_offset += sizeof(uint32_t);
    *data_addr = param_addr + (uintptr_t)(*byte_offset);
    *byte_offset += (*data_len * sizeof(uint32_t));
    return 0;
}

static void parse_layernorm_radix(uintptr_t param_addr,
                                  uint32_t little_endian,
                                  uint32_t *byte_offset,
                                  uint32_t *radix_num,
                                  uintptr_t *radix_addr)
{
    *radix_num = read_graph_u32_at(param_addr, *byte_offset, little_endian);
    *byte_offset += sizeof(uint32_t);
    *radix_addr = param_addr + (uintptr_t)(*byte_offset);
    *byte_offset += (*radix_num * sizeof(int16_t));
}

static int parse_layernorm_param(uintptr_t param_addr,
                                 uint32_t little_endian,
                                 layernorm_param_info_t *param)
{
    uint32_t byte_offset = 0U;

    param->axis = (int32_t)read_graph_u32_at(param_addr, byte_offset, little_endian);
    byte_offset += sizeof(uint32_t);

    param->stash_type = (int32_t)read_graph_u32_at(param_addr, byte_offset, little_endian);
    byte_offset += sizeof(uint32_t);

    param->epsilon_word = read_graph_u32_at(param_addr, byte_offset, little_endian);
    byte_offset += sizeof(uint32_t);

    parse_layernorm_radix(param_addr,
                          little_endian,
                          &byte_offset,
                          &param->x_radix_num,
                          &param->x_radix_addr);

    parse_layernorm_radix(param_addr,
                          little_endian,
                          &byte_offset,
                          &param->y_radix_num,
                          &param->y_radix_addr);

    if (parse_layernorm_tensor(param_addr,
                               little_endian,
                               &byte_offset,
                               &param->scale_rank,
                               param->scale_shape,
                               &param->scale_len,
                               &param->scale_addr) != 0) {
        return -2;
    }

    if (parse_layernorm_tensor(param_addr,
                               little_endian,
                               &byte_offset,
                               &param->bias_rank,
                               param->bias_shape,
                               &param->bias_len,
                               &param->bias_addr) != 0) {
        return -3;
    }

    return 0;
}

static void print_layernorm_shape(const char *name, const uint32_t *shape, uint32_t rank, uint32_t len, uintptr_t addr)
{
    uint32_t i;

    xil_printf("[CPU][LN] %s rank=%lu len=%lu addr=0x%08lx shape=",
               name,
               (unsigned long)rank,
               (unsigned long)len,
               (unsigned long)addr);
    for (i = 0U; i < rank; ++i) {
        xil_printf("%s%lu", (i == 0U) ? "[" : ",", (unsigned long)shape[i]);
    }
    xil_printf("]\r\n");
}

static void print_layernorm_radix(const char *name, uintptr_t param_addr, uint32_t little_endian, uint32_t num, uintptr_t addr)
{
    int first_radix = 0;

    if (num > 0U) {
        first_radix = (int)read_graph_i16_at(param_addr, (uint32_t)(addr - param_addr), little_endian);
    }

    xil_printf("[CPU][LN] %s_radix num=%lu addr=0x%08lx first=%d\r\n",
               name,
               (unsigned long)num,
               (unsigned long)addr,
               first_radix);
}

static int exec_cpu_layernorm_node(const cpu_node_info_t *node,
                                   const data_node_info_t *data_nodes,
                                   uint32_t data_node_count,
                                   uint32_t little_endian)
{
    const data_node_info_t *input_node;
    const data_node_info_t *output_node;
    layernorm_param_info_t param;
    uint32_t h;
    uint32_t w;
    uint32_t c;
    float epsilon;
    int ret;

    if (node->num_of_input_nodes != 1U) {
        xil_printf("[CPU][LN] idx=%lu unsupported input num=%lu\r\n",
                   (unsigned long)node->index,
                   (unsigned long)node->num_of_input_nodes);
        return -1;
    }

    input_node = find_data_node(data_nodes, data_node_count, node->input_nodes_idx[0]);
    output_node = find_data_node(data_nodes, data_node_count, node->output_node_idx);
    if ((input_node == NULL) || (output_node == NULL)) {
        xil_printf("[CPU][LN] idx=%lu missing data node in=0x%08lx out=0x%08lx\r\n",
                   (unsigned long)node->index,
                   (unsigned long)node->input_nodes_idx[0],
                   (unsigned long)node->output_node_idx);
        return -2;
    }

    xil_printf("[CPU][LN] idx=%lu param=0x%08lx in_raw=0x%08lx in_addr=0x%08lx out_raw=0x%08lx out_addr=0x%08lx in_fmt=%lu out_fmt=%lu in_chw=[%lu,%lu,%lu] out_chw=[%lu,%lu,%lu]\r\n",
               (unsigned long)node->index,
               (unsigned long)node->start_addr,
               (unsigned long)input_node->raw_addr,
               (unsigned long)input_node->addr,
               (unsigned long)output_node->raw_addr,
               (unsigned long)output_node->addr,
               (unsigned long)input_node->format,
               (unsigned long)output_node->format,
               (unsigned long)input_node->channel,
               (unsigned long)input_node->height,
               (unsigned long)input_node->width,
               (unsigned long)output_node->channel,
               (unsigned long)output_node->height,
               (unsigned long)output_node->width);

    ret = parse_layernorm_param((uintptr_t)node->start_addr, little_endian, &param);
    if (ret != 0) {
        xil_printf("[CPU][LN] bad param ret=%d param=0x%08lx\r\n",
                   ret,
                   (unsigned long)node->start_addr);
        return -3;
    }

    xil_printf("[CPU][LN] axis=%ld stash_type=%ld epsilon_word=0x%08lx\r\n",
               (long)param.axis,
               (long)param.stash_type,
               (unsigned long)param.epsilon_word);
    print_layernorm_radix("x", (uintptr_t)node->start_addr, little_endian, param.x_radix_num, param.x_radix_addr);
    print_layernorm_radix("y", (uintptr_t)node->start_addr, little_endian, param.y_radix_num, param.y_radix_addr);
    print_layernorm_shape("scale", param.scale_shape, param.scale_rank, param.scale_len, param.scale_addr);
    print_layernorm_shape("bias", param.bias_shape, param.bias_rank, param.bias_len, param.bias_addr);

    if ((input_node->format != 1U) || (output_node->format != 1U)) {
        xil_printf("[CPU][LN] unsupported data format %lu -> %lu\r\n",
                   (unsigned long)input_node->format,
                   (unsigned long)output_node->format);
        return -100;
    }

    if ((input_node->height != output_node->height) ||
        (input_node->width != output_node->width) ||
        (input_node->channel != output_node->channel)) {
        xil_printf("[CPU][LN] shape mismatch in=[%lu,%lu,%lu] out=[%lu,%lu,%lu]\r\n",
                   (unsigned long)input_node->channel,
                   (unsigned long)input_node->height,
                   (unsigned long)input_node->width,
                   (unsigned long)output_node->channel,
                   (unsigned long)output_node->height,
                   (unsigned long)output_node->width);
        return -4;
    }

    if (input_node->addr == output_node->addr) {
        xil_printf("[CPU][LN] input and output addr are same, cannot use output as input convert temp\r\n");
        return -5;
    }

    if ((param.scale_len < input_node->channel) ||
        (param.bias_len < input_node->channel) ||
        ((param.x_radix_num != 1U) && (param.x_radix_num < input_node->channel)) ||
        ((param.y_radix_num != 1U) && (param.y_radix_num < output_node->channel))) {
        xil_printf("[CPU][LN] bad param len scale=%lu bias=%lu x_radix=%lu y_radix=%lu channel=%lu\r\n",
                   (unsigned long)param.scale_len,
                   (unsigned long)param.bias_len,
                   (unsigned long)param.x_radix_num,
                   (unsigned long)param.y_radix_num,
                   (unsigned long)input_node->channel);
        return -6;
    }

    copy_bytes((uintptr_t)output_node->addr,
               (uintptr_t)input_node->addr,
               data_node_element_count(input_node) * sizeof(int16_t));
    if (data_node_normal_to_format_1(input_node, (uintptr_t)output_node->addr) != 0) {
        xil_printf("[CPU][LN] input normal to format1 failed\r\n");
        return -7;
    }

    epsilon = word_to_float(param.epsilon_word);
    for (h = 0U; h < input_node->height; ++h) {
        for (w = 0U; w < input_node->width; ++w) {
            float sum = 0.0f;
            float mean;
            float var_sum = 0.0f;
            float denom;

            for (c = 0U; c < input_node->channel; ++c) {
                float val;
                int16_t x_radix = layernorm_radix_at(param.x_radix_addr, little_endian, param.x_radix_num, c);

                if (data_node_read_float_format_1(input_node, c, h, w, x_radix, &val) != 0) {
                    return -8;
                }
                sum += val;
            }

            mean = sum / (float)input_node->channel;
            for (c = 0U; c < input_node->channel; ++c) {
                float val;
                float diff;
                int16_t x_radix = layernorm_radix_at(param.x_radix_addr, little_endian, param.x_radix_num, c);

                if (data_node_read_float_format_1(input_node, c, h, w, x_radix, &val) != 0) {
                    return -9;
                }
                diff = val - mean;
                var_sum += diff * diff;
            }

            denom = layernorm_sqrtf((var_sum / (float)input_node->channel) + epsilon);
            for (c = 0U; c < input_node->channel; ++c) {
                float val;
                float scale = read_graph_float_at(param.scale_addr, c * sizeof(uint32_t), little_endian);
                float bias = read_graph_float_at(param.bias_addr, c * sizeof(uint32_t), little_endian);
                float out_val;
                int16_t x_radix = layernorm_radix_at(param.x_radix_addr, little_endian, param.x_radix_num, c);
                int16_t y_radix = layernorm_radix_at(param.y_radix_addr, little_endian, param.y_radix_num, c);

                if (data_node_read_float_format_1(input_node, c, h, w, x_radix, &val) != 0) {
                    return -10;
                }

                out_val = ((val - mean) / denom) * scale + bias;
                if (data_node_write_float_format_1(output_node, c, h, w, y_radix, out_val) != 0) {
                    return -11;
                }
            }
        }
    }

    xil_printf("[CPU][LN] done h=%lu w=%lu c=%lu\r\n",
               (unsigned long)input_node->height,
               (unsigned long)input_node->width,
               (unsigned long)input_node->channel);
    return 0;
}

static int exec_cpu_node(const cpu_node_info_t *node,
                         const data_node_info_t *data_nodes,
                         uint32_t data_node_count,
                         uint32_t little_endian)
{
    xil_printf("[CPU] idx=%lu opcode=%lu inputs=%lu out=0x%08lx param=0x%08lx\r\n",
               (unsigned long)node->index,
               (unsigned long)node->opcode_id,
               (unsigned long)node->num_of_input_nodes,
               (unsigned long)node->output_node_idx,
               (unsigned long)node->start_addr);

    if (node->opcode_id == OP_LAYER_NORM) {
        return exec_cpu_layernorm_node(node, data_nodes, data_node_count, little_endian);
    }

    xil_printf("[CPU] unsupported opcode=%lu idx=%lu\r\n",
               (unsigned long)node->opcode_id,
               (unsigned long)node->index);
    return -1;
}

static int parse_opflow_and_run(uintptr_t opflow_addr, uint32_t opflow_size, const case_sizes_t *sizes)
{
    uintptr_t addr;
    uintptr_t end = opflow_addr + opflow_size;
    uint32_t cdma_count = 0U;
    uint32_t npu_count = 0U;
    uint32_t cpu_count = 0U;
    uint32_t layernorm_cpu_count = 0U;
    graph_config_t graph_cfg;
    int graph_cfg_status;
    uint32_t cmd_addr = DRAM_CMD_START_ADDR;
    data_node_info_t data_nodes[GRAPH_OPFLOW_MAX_DATA_NODES];
    uint32_t data_node_count = 0U;
    uint32_t output_addr = DRAM_DATA_START_ADDR_2;
    const data_node_info_t *output_node = NULL;

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

    addr = opflow_addr;
    while (addr < end) {
        uint32_t index = read_graph_word(addr, 0U, graph_cfg.little_endian);
        uint32_t node_type = read_graph_word(addr, 1U, graph_cfg.little_endian);

        if (node_type == 0U) {
            uint32_t opcode = read_graph_word(addr, 3U, graph_cfg.little_endian);

            if (opcode == OP_CDMA) {
                if ((addr + (6U * sizeof(uint32_t))) > end) {
                    xil_printf("[OPFLOW] short cdma node addr=0x%08lx idx=%lu\r\n",
                               (unsigned long)addr,
                               (unsigned long)index);
                    return -2;
                }
                addr += 6U * sizeof(uint32_t);
                continue;
            }

            {
                cpu_node_info_t cpu_node;
                uint32_t node_size;
                int ret = parse_cpu_node(addr, end, graph_cfg.little_endian, &cpu_node, &node_size);
                if (ret != 0) {
                    xil_printf("[OPFLOW] bad cpu node idx=%lu ret=%d\r\n",
                               (unsigned long)index,
                               ret);
                    return -3;
                }
                addr += node_size;
                continue;
            }
        }

        if (node_type == 1U) {
            if ((addr + (3U * sizeof(uint32_t))) > end) {
                xil_printf("[OPFLOW] short npu node addr=0x%08lx idx=%lu\r\n",
                           (unsigned long)addr,
                           (unsigned long)index);
                return -5;
            }

            addr += 3U * sizeof(uint32_t);
            continue;
        }

        if (node_type == 2U) {
            data_node_info_t data_node;

            if ((addr + (7U * sizeof(uint32_t))) > end) {
                xil_printf("[OPFLOW] short data node addr=0x%08lx idx=%lu\r\n",
                           (unsigned long)addr,
                           (unsigned long)index);
                return -7;
            }

            if (data_node_count >= GRAPH_OPFLOW_MAX_DATA_NODES) {
                xil_printf("[OPFLOW] too many data nodes\r\n");
                return -12;
            }

            data_node.index = index;
            data_node.raw_addr = read_graph_word(addr, 2U, graph_cfg.little_endian);
            data_node.addr = graph_addr_to_board(data_node.raw_addr);
            data_node.format = read_graph_word(addr, 3U, graph_cfg.little_endian);
            data_node.height = read_graph_word(addr, 4U, graph_cfg.little_endian);
            data_node.width = read_graph_word(addr, 5U, graph_cfg.little_endian);
            data_node.channel = read_graph_word(addr, 6U, graph_cfg.little_endian);
            data_nodes[data_node_count] = data_node;
            data_node_count++;

            addr += 7U * sizeof(uint32_t);
            continue;
        }

        xil_printf("[OPFLOW] unsupported node idx=%lu type=%lu\r\n",
                   (unsigned long)index,
                   (unsigned long)node_type);
        return -8;
    }

    xil_printf("[OPFLOW] first scan data_nodes=%lu\r\n", (unsigned long)data_node_count);

    addr = opflow_addr;
    while (addr < end) {
        uint32_t index = read_graph_word(addr, 0U, graph_cfg.little_endian);
        uint32_t node_type = read_graph_word(addr, 1U, graph_cfg.little_endian);

        if (node_type == 0U) {
            uint32_t opcode = read_graph_word(addr, 3U, graph_cfg.little_endian);

            if (opcode == OP_CDMA) {
                uint32_t sar = graph_addr_to_board(read_graph_word(addr, 4U, graph_cfg.little_endian));
                uint32_t len = read_graph_word(addr, 5U, graph_cfg.little_endian);

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

            {
                cpu_node_info_t cpu_node;
                uint32_t node_size;
                int ret = parse_cpu_node(addr, end, graph_cfg.little_endian, &cpu_node, &node_size);
                if (ret != 0) {
                    xil_printf("[OPFLOW] bad cpu node idx=%lu ret=%d\r\n",
                               (unsigned long)index,
                               ret);
                    return -14;
                }
                ret = exec_cpu_node(&cpu_node, data_nodes, data_node_count, graph_cfg.little_endian);
                if (ret != 0) {
                    return -15;
                }
                if (cpu_node.opcode_id == OP_LAYER_NORM) {
                    layernorm_cpu_count++;
                }
                addr += node_size;
                cpu_count++;
                continue;
            }
        }

        if (node_type == 1U) {
            xil_printf("[OPFLOW] npu idx=%lu\r\n", (unsigned long)index);
            if (run_npu_node() != 0) {
                return -6;
            }

            addr += 3U * sizeof(uint32_t);
            npu_count++;
            continue;
        }

        if (node_type == 2U) {
            const data_node_info_t *data_node = find_data_node(data_nodes, data_node_count, index);
            if (data_node != NULL) {
                xil_printf("[OPFLOW] data idx=%lu raw=0x%08lx addr=0x%08lx fmt=%lu h=%lu w=%lu c=%lu\r\n",
                           (unsigned long)data_node->index,
                           (unsigned long)data_node->raw_addr,
                           (unsigned long)data_node->addr,
                           (unsigned long)data_node->format,
                           (unsigned long)data_node->height,
                           (unsigned long)data_node->width,
                           (unsigned long)data_node->channel);
            }

            addr += 7U * sizeof(uint32_t);
            continue;
        }

        return -16;
    }

    xil_printf("[OPFLOW] done cdma=%lu npu=%lu cpu=%lu\r\n",
               (unsigned long)cdma_count,
               (unsigned long)npu_count,
               (unsigned long)cpu_count);

    if ((cdma_count == 0U) && (npu_count == 0U) && (cpu_count == 0U)) {
        xil_printf("[OPFLOW] missing executable nodes\r\n");
        return -9;
    }

    if ((graph_cfg.is_multilayer != 0U) && (graph_cfg.dataout_num > 0U)) {
        output_node = find_data_node(data_nodes, data_node_count, graph_cfg.dataout_index[0]);
        if (output_node == NULL) {
            xil_printf("[OPFLOW] output data node 0x%08lx not found\r\n",
                       (unsigned long)graph_cfg.dataout_index[0]);
            return -13;
        }
        output_addr = output_node->addr;
    }

    xil_printf("[OPFLOW] compare output node addr=0x%08lx\r\n", (unsigned long)output_addr);

    if (sizes->golden_size == 0U) {
        xil_printf("[CMP] skip: no golden file loaded\r\n");
        return 0;
    }

    if ((layernorm_cpu_count > 0U) && (output_node != NULL) && (output_node->format == 1U)) {
        if (data_node_format_1_to_normal(output_node, CPU_COMPARE_START_ADDR) != 0) {
            xil_printf("[CMP] output format1 to normal failed\r\n");
            return -17;
        }
        output_addr = CPU_COMPARE_START_ADDR;
        xil_printf("[CMP] LN output converted to normal addr=0x%08lx\r\n",
                   (unsigned long)output_addr);
    }

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
