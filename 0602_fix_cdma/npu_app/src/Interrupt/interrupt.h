#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <stdint.h>

#include "xparameters.h"
#include "xintc.h"
#include "xil_exception.h"
#include "xil_printf.h"

#define INTC_DEVICE_ID XPAR_AXI_INTC_0_DEVICE_ID

#define IRQ_ID_NPU_0  XPAR_AXI_INTC_0_NPU_TOP_0_INTERRUPT_O_INTR
#define IRQ_ID_NPU_1  XPAR_AXI_INTC_0_NPU_TOP_0_INTERRUPT_O_LOW_PRIORITY_INTR
#define IRQ_ID_NPU_2  XPAR_AXI_INTC_0_NPU_TOP_0_INTERRUPT_O_2_INTR
#define IRQ_ID_NPU_3  XPAR_AXI_INTC_0_NPU_TOP_0_INTERRUPT_O_3_INTR
#define IRQ_ID_NPU_4  XPAR_AXI_INTC_0_NPU_TOP_0_INTERRUPT_O_4_INTR
#define IRQ_ID_NPU_5  XPAR_AXI_INTC_0_NPU_TOP_0_INTERRUPT_O_5_INTR
#define IRQ_ID_NPU_6  XPAR_AXI_INTC_0_NPU_TOP_0_INTERRUPT_O_6_INTR
#define IRQ_ID_NPU_7  XPAR_AXI_INTC_0_NPU_TOP_0_INTERRUPT_O_7_INTR
#define IRQ_ID_NPU_8  XPAR_AXI_INTC_0_NPU_TOP_0_INTERRUPT_O_8_INTR
#define IRQ_ID_NPU_9  XPAR_AXI_INTC_0_NPU_TOP_0_INTERRUPT_O_9_INTR
#define IRQ_ID_NPU_10 XPAR_AXI_INTC_0_NPU_TOP_0_INTERRUPT_O_10_INTR
#define IRQ_ID_NPU_11 XPAR_AXI_INTC_0_NPU_TOP_0_INTERRUPT_O_11_INTR

#define IRQ_ID_A IRQ_ID_NPU_0
#define IRQ_ID_B IRQ_ID_NPU_1
#define IRQ_ID_C IRQ_ID_NPU_2
#define IRQ_ID_D IRQ_ID_NPU_4

#define NPU_IRQ_DONE_MASK 0x00000FF0UL

extern volatile uint32_t npu_irq_done_mask;
extern volatile int irq_A_done;
extern volatile int irq_B_done;
extern volatile int irq_C_done;
extern volatile int irq_D_done;
extern XIntc IntcInstance;

int InterruptInitAll(void);
int SetupInterruptSystem(u32 irq_id, XInterruptHandler handler);
void InterruptClearNpuDone(void);
uint32_t InterruptGetNpuDoneMask(void);
int InterruptWaitNpuDone(uint32_t mask, uint64_t timeout_cycles);

void Handler_A(void *CallbackRef);
void Handler_B(void *CallbackRef);
void Handler_C(void *CallbackRef);
void Handler_D(void *CallbackRef);
void Handler_NPU(void *CallbackRef);

#endif
