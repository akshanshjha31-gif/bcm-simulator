/**
 * @file    FreeRTOSConfig.h
 * @brief   FreeRTOS kernel configuration for the BCM Simulator.
 *
 * Two decisions worth stating explicitly:
 *
 * 1. STATIC ALLOCATION ONLY. configSUPPORT_DYNAMIC_ALLOCATION is 0, so no
 *    heap_x.c is compiled and there is no heap at all. Every task, queue and
 *    semaphore is backed by a caller-supplied buffer. This enforces the
 *    architecture document's "no dynamic allocation on real-time paths" rule
 *    at link time rather than by convention, and makes RAM use exactly
 *    knowable on a 20 KB part.
 *
 * 2. SOFTWARE TIMERS ARE OFF. The architecture doc lists FreeRTOS timers, but
 *    the BCM's timing (blink phases, auto-lock, comm timeout) already lives
 *    in drivers::SoftTimer, which is HAL-free and covered by host unit tests.
 *    Moving it into kernel timers would cost a task plus its stack and make
 *    the logic untestable off-target - a clear regression.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/* ---- Scheduler ---------------------------------------------------------- */
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configCPU_CLOCK_HZ                      ( 72000000UL )
#define configTICK_RATE_HZ                      ( 1000U )
#define configMAX_PRIORITIES                    ( 6 )
#define configMINIMAL_STACK_SIZE                ( 96U )    /* words */
#define configMAX_TASK_NAME_LEN                 ( 12 )
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                 1

/* ---- Memory ------------------------------------------------------------- */
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        0

/* ---- Synchronisation primitives ----------------------------------------- */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_QUEUE_SETS                    0
#define configUSE_TASK_NOTIFICATIONS            1
#define configQUEUE_REGISTRY_SIZE               4

/* ---- Hooks -------------------------------------------------------------- */
#define configUSE_IDLE_HOOK                     0
/* Not needed: SysTick_Handler is written by hand in app/rtos_app.cpp so it
 * can feed HAL_IncTick() and only then forward to the kernel. */
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            0
/* 2 = paint the stack and check the watermark on every switch. Worth the
 * cost here: a silent stack overflow on a 20 KB part is unrecoverable. */
#define configCHECK_FOR_STACK_OVERFLOW          2

/* ---- Features not used -------------------------------------------------- */
#define configUSE_TIMERS                        0
#define configUSE_CO_ROUTINES                   0
#define configUSE_EVENT_GROUPS                  1
#define configUSE_STREAM_BUFFERS                0
#define configUSE_TRACE_FACILITY                0
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_APPLICATION_TASK_TAG          0
#define configUSE_NEWLIB_REENTRANT              0

/* ---- API inclusion ------------------------------------------------------ */
#define INCLUDE_vTaskPrioritySet                0
#define INCLUDE_uxTaskPriorityGet               0
#define INCLUDE_vTaskDelete                     0
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetCurrentTaskHandle       1

/* ---- Cortex-M3 interrupt priorities -------------------------------------
 *
 * ST's headers declare __NVIC_PRIO_BITS = 4 for the STM32F1, but THIS PART
 * implements only 3: writing 0xFF to an NVIC priority register reads back
 * 0xE0, not 0xF0. (The board is a Blue Pill clone - it also reports 128 KB of
 * flash on a part marked C8.) Configuring for 4 bits makes the CM3 port's
 * start-up sanity check fail, which is exactly what it is there to catch.
 *
 * 3 is also the safe choice on genuine 4-bit silicon: it simply uses coarser
 * priorities, and both of the port's assertions still hold.
 */
#define configPRIO_BITS                         3

/* Lowest possible priority (2^3 - 1) for the kernel's own PendSV/SysTick. */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         7
/* Interrupts at or below this logical priority may call ...FromISR APIs.
 * drv_uart asks the HAL for priority 5; the HAL encodes that as 5 << 4, of
 * which the hardware keeps the top 3 bits => effective priority 2. So the
 * USART1 ISR sits exactly at the threshold and may signal a task. */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    2

#define configKERNEL_INTERRUPT_PRIORITY \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* ---- Handler names ------------------------------------------------------ */
/* SVC and PendSV are mapped straight onto the vector table: they only ever
 * fire once the scheduler is running.
 *
 * SysTick is deliberately NOT mapped. HAL_Init() enables SysTick at the top
 * of main(), long before vTaskStartScheduler(), so routing it directly at
 * the kernel would call xTaskIncrementTick() with no task lists - which
 * hard-faults during clock configuration. app/rtos_app.cpp supplies a
 * SysTick_Handler that feeds the HAL time base and forwards to the kernel
 * only once the scheduler has actually started. */
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler

/* ---- Assertions --------------------------------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif
void bcm_rtos_assert(const char* file, int line);
#ifdef __cplusplus
}
#endif

#define configASSERT( x ) \
    if ( ( x ) == 0 ) { bcm_rtos_assert( __FILE__, __LINE__ ); }

#endif /* FREERTOS_CONFIG_H */
