#ifndef ADDRMAP_H_
#define ADDRMAP_H_

#if defined(__has_include)
#if __has_include("xparameters.h")
#include "xparameters.h"
#define HAVE_XPARAMETERS 1
#else
#define HAVE_XPARAMETERS 0
#endif
#else
#define HAVE_XPARAMETERS 0
#endif

#if (HAVE_XPARAMETERS == 0)
#define XPAR_NPU_TOP_0_BASEADDR 0x40000000UL
#define XPAR_XINTC_0_BASEADDR   0x41200000UL
#endif

/* DDR staging buffers for current testcase convention. */
#define DRAM_CMD_START_ADDR       0xE0000000UL
#define DRAM_WEIGHT_START_ADDR    0xC0000000UL
#define DRAM_DATA_START_ADDR_1    0xF0000000UL
#define DRAM_DATA_START_ADDR_2    0xF0200000UL
#define DRAM_SWAP_START_ADDR      DRAM_DATA_START_ADDR_1
#define DRAM_CONFIG_START_ADDR    0xF4000000UL
#define DRAM_CONFIG_OPFLOW_ADDR   0xF4010000UL
#define DRAM_OPFLOW_START_ADDR    0xF4100000UL
#define DRAM_NET_START_ADDR       0xF4200000UL
#define DRAM_LUT_START_ADDR       0xF5000000UL
#define DRAM_GOLDEN_START_ADDR    0xF6000000UL

/* Runtime metadata written by XSCT before releasing firmware from main. */
#define CASE_SIZE_DESC_ADDR       0xFFFF0000UL
#define CASE_STATUS_ADDR          0xFFFF1000UL

#define CASE_SIZE_MAGIC           0x4E393130UL
#define CASE_SIZE_VERSION         0x00000001UL

#define CASE_SIZE_MAGIC_IDX       0UL
#define CASE_SIZE_VERSION_IDX     1UL
#define CASE_SIZE_CONFIG_IDX      2UL
#define CASE_SIZE_CONFIG_OPFLOW_IDX 3UL
#define CASE_SIZE_OPFLOW_IDX      4UL
#define CASE_SIZE_CMD_IDX         5UL
#define CASE_SIZE_WEIGHT_IDX      6UL
#define CASE_SIZE_DATA_IDX        7UL
#define CASE_SIZE_GOLDEN_IDX      8UL
#define CASE_SIZE_NET_IDX         9UL

#define CASE_STATUS_RUNNING       0x52554E20UL
#define CASE_STATUS_PASS          0x50415353UL
#define CASE_STATUS_FAIL          0x4641494CUL

/* Fallback sizes used if runtime metadata has not been preloaded. */
#define CONFIG_BIN_SIZE_BYTES         68UL
#define CONFIG_OPFLOW_SIZE_BYTES      28UL
#define OPFLOW_BIN_SIZE_BYTES         92UL
#define CMD_BIN_SIZE_BYTES            320UL
#define WEIGHT_BIN_SIZE_BYTES         2600UL
#define DATA_BIN_SIZE_BYTES           9248UL
#define GOLDEN_BIN_SIZE_BYTES         720UL
#define NET_BIN_SIZE_BYTES            3048UL

#define DRAM_CMD_OFFSET           0x00000000UL
#define DRAM_DATA_OFFSET          0x00000000UL

#define CM_CDMA_BASE_ADDR         0x40000000UL
#define NPUREG_BASE_ADDR          0x44000000UL
#if (HAVE_XPARAMETERS == 1)
#define AXI_INTC_BASE_ADDR        XPAR_AXI_INTC_0_BASEADDR
#else
#define AXI_INTC_BASE_ADDR        0x41200000UL
#endif

#define SRAMIF_BASE_ADDR          0x44000000UL
#define WEIGHTIF_BASE_ADDR        0x48000000UL
#define SRAM_BANK_A_START_ADDR    0x45000000UL
#define SRAM_BANK_B_START_ADDR    0x46000000UL
#define LUT_SRAM_START_ADDR       0x47000000UL

#define CDMA_DAR_RESERVED_VALUE   0x00000000UL
#define WEIGHT_FIFO_START         WEIGHTIF_BASE_ADDR
#define LUT_SRAM_LENGTH           0x00020000UL

/* Hardware register addressing is register_index * 16 bytes. */
#define REG_STRIDE_BYTES          16UL

/* NPU ctrl register write values: reg bit0 en/go in current RTL. */
#define NPU_CTRL_EN_VALUE         0x00000001UL
#define NPU_CTRL_GO_VALUE         0x00000001UL

/* npu_top_0.interrupt_o[11:0] is connected to axi_intc_0.intr[11:0]. */
#define NPU_DONE_INTC_MASK        0x00000FF0UL

#define CMD_LEN_MAX_BYTES         0x00040000UL
#define CDMA_TIMEOUT_CYCLES       100000000ULL
#define NPU_TIMEOUT_CYCLES        1440000000000ULL

#endif /* ADDRMAP_H_ */
