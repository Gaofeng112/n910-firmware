#include "interrupt.h"

volatile uint32_t npu_irq_done_mask = 0U;
volatile int irq_A_done = 0;
volatile int irq_B_done = 0;
volatile int irq_C_done = 0;
volatile int irq_D_done = 0;

XIntc IntcInstance;

static int interrupt_initialized = 0;

static const u32 npu_irq_ids[] = {
    IRQ_ID_NPU_0,
    IRQ_ID_NPU_1,
    IRQ_ID_NPU_2,
    IRQ_ID_NPU_3,
    IRQ_ID_NPU_4,
    IRQ_ID_NPU_5,
    IRQ_ID_NPU_6,
    IRQ_ID_NPU_7,
    IRQ_ID_NPU_8,
    IRQ_ID_NPU_9,
    IRQ_ID_NPU_10,
    IRQ_ID_NPU_11,
};

static const uint32_t npu_irq_masks[] = {
    (1UL << IRQ_ID_NPU_0),
    (1UL << IRQ_ID_NPU_1),
    (1UL << IRQ_ID_NPU_2),
    (1UL << IRQ_ID_NPU_3),
    (1UL << IRQ_ID_NPU_4),
    (1UL << IRQ_ID_NPU_5),
    (1UL << IRQ_ID_NPU_6),
    (1UL << IRQ_ID_NPU_7),
    (1UL << IRQ_ID_NPU_8),
    (1UL << IRQ_ID_NPU_9),
    (1UL << IRQ_ID_NPU_10),
    (1UL << IRQ_ID_NPU_11),
};

static void mark_irq_done(uint32_t mask)
{
    npu_irq_done_mask |= mask;

    if ((mask & (1UL << IRQ_ID_A)) != 0U) {
        irq_A_done = 1;
    }
    if ((mask & (1UL << IRQ_ID_B)) != 0U) {
        irq_B_done = 1;
    }
    if ((mask & (1UL << IRQ_ID_C)) != 0U) {
        irq_C_done = 1;
    }
    if ((mask & (1UL << IRQ_ID_D)) != 0U) {
        irq_D_done = 1;
    }
}

static int interrupt_init_controller(void)
{
    int status;

    if (interrupt_initialized != 0) {
        return XST_SUCCESS;
    }

    status = XIntc_Initialize(&IntcInstance, INTC_DEVICE_ID);
    if (status != XST_SUCCESS) {
        xil_printf("[INTC] initialize failed: %d\r\n", status);
        return XST_FAILURE;
    }

    status = XIntc_Start(&IntcInstance, XIN_REAL_MODE);
    if (status != XST_SUCCESS) {
        xil_printf("[INTC] start failed: %d\r\n", status);
        return XST_FAILURE;
    }

    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
                                 (Xil_ExceptionHandler)XIntc_InterruptHandler,
                                 &IntcInstance);
    Xil_ExceptionEnable();

    interrupt_initialized = 1;
    return XST_SUCCESS;
}

int SetupInterruptSystem(u32 irq_id, XInterruptHandler handler)
{
    int status;

    status = interrupt_init_controller();
    if (status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    status = XIntc_Connect(&IntcInstance, irq_id, handler, (void *)&npu_irq_masks[irq_id]);
    if (status != XST_SUCCESS) {
        xil_printf("[INTC] connect irq %lu failed: %d\r\n", (unsigned long)irq_id, status);
        return XST_FAILURE;
    }

    XIntc_Acknowledge(&IntcInstance, irq_id);
    XIntc_Enable(&IntcInstance, irq_id);
    return XST_SUCCESS;
}

int InterruptInitAll(void)
{
    int status;
    unsigned int i;

    InterruptClearNpuDone();

    for (i = 0U; i < (sizeof(npu_irq_ids) / sizeof(npu_irq_ids[0])); ++i) {
        if ((npu_irq_masks[i] & NPU_IRQ_DONE_MASK) == 0U) {
            continue;
        }

        status = SetupInterruptSystem(npu_irq_ids[i], Handler_NPU);
        if (status != XST_SUCCESS) {
            return XST_FAILURE;
        }
    }

    xil_printf("[INTC] interrupt mode ready, done_mask=0x%08lx\r\n", (unsigned long)NPU_IRQ_DONE_MASK);
    return XST_SUCCESS;
}

void InterruptClearNpuDone(void)
{
    npu_irq_done_mask = 0U;
    irq_A_done = 0;
    irq_B_done = 0;
    irq_C_done = 0;
    irq_D_done = 0;
}

uint32_t InterruptGetNpuDoneMask(void)
{
    return npu_irq_done_mask;
}

int InterruptWaitNpuDone(uint32_t mask, uint64_t timeout_cycles)
{
    uint64_t i;

    for (i = 0ULL; i < timeout_cycles; ++i) {
        if ((npu_irq_done_mask & mask) != 0U) {
            return XST_SUCCESS;
        }

        if ((i & 0x03FFFFFFULL) == 0ULL) {
		xil_printf("[WAIT_IRQ] i=%lu done=0x%08lx isr=0x%08lx\r\n",
				   (unsigned long)i,
				   (unsigned long)npu_irq_done_mask,
				   (unsigned long)XIntc_GetIntrStatus(IntcInstance.BaseAddress));
		}
    }

    return XST_FAILURE;
}

void Handler_NPU(void *CallbackRef)
{
    uint32_t mask = *(uint32_t *)CallbackRef;
    u32 irq_id = 0U;

    mark_irq_done(mask);
    xil_printf("[IRQ] mask=0x%08lx\r\n", (unsigned long)mask);

    while (((mask >> irq_id) & 0x1U) == 0U) {
        irq_id++;
    }

    XIntc_Acknowledge(&IntcInstance, irq_id);
}

void Handler_A(void *CallbackRef)
{
    Handler_NPU(CallbackRef);
}

void Handler_B(void *CallbackRef)
{
    Handler_NPU(CallbackRef);
}

void Handler_C(void *CallbackRef)
{
    Handler_NPU(CallbackRef);
}

void Handler_D(void *CallbackRef)
{
    Handler_NPU(CallbackRef);
}
