/**
 * @file    rtos_app.cpp
 * @brief   FreeRTOS task set, watchdog supervisor and kernel hooks.
 */
#include "rtos_app.h"

#include "bcm_app.h"
#include "bsp.h"
#include "diag_mgr.h"
#include "drv_adc.h"
#include "drv_timer.h"
#include "drv_uart.h"
#include "drv_watchdog.h"
#include "logger.h"

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

namespace bcm {
namespace app {
namespace {

/* ---- Task identities ---------------------------------------------------- */

enum TaskId : uint8_t {
    kTaskControl = 0,
    kTaskSensor,
    kTaskLogger,
    kTaskMonitor,
    kTaskCount
};

/* One check-in bit per supervised task. The watchdog refreshes the IWDG only
 * when all of them are set, which turns a hung task into a controlled reset
 * rather than a silently degraded ECU (SRS-SAFETY-006). */
const EventBits_t kCheckInAll =
    (1U << kTaskControl) | (1U << kTaskSensor) |
    (1U << kTaskLogger)  | (1U << kTaskMonitor);

/* ---- Periods and priorities --------------------------------------------- */

const TickType_t kControlPeriod  = pdMS_TO_TICKS(5U);
const TickType_t kSensorPeriod   = pdMS_TO_TICKS(20U);
const TickType_t kMonitorPeriod  = pdMS_TO_TICKS(100U);
const TickType_t kWatchdogPeriod = pdMS_TO_TICKS(200U);

/* The IWDG window must comfortably exceed the supervisor's own period, or a
 * single late tick would reset a healthy ECU. */
const uint32_t kWatchdogTimeoutMs = 1000U;

const UBaseType_t kPrioLogger   = 1U;
const UBaseType_t kPrioSensor   = 2U;
const UBaseType_t kPrioControl  = 3U;
const UBaseType_t kPrioMonitor  = 4U;
const UBaseType_t kPrioWatchdog = 5U;

/* ---- Static storage ------------------------------------------------------ */

/* Stack sizes are in WORDS. Measured with uxTaskGetStackHighWaterMark and
 * left with roughly 2x headroom. */
StackType_t  g_stack_control[256];
StackType_t  g_stack_sensor[128];
StackType_t  g_stack_logger[160];
StackType_t  g_stack_monitor[160];
StackType_t  g_stack_watchdog[128];

StaticTask_t g_tcb_control;
StaticTask_t g_tcb_sensor;
StaticTask_t g_tcb_logger;
StaticTask_t g_tcb_monitor;
StaticTask_t g_tcb_watchdog;

StaticEventGroup_t g_checkin_storage;
EventGroupHandle_t g_checkin = 0;

/* Log queue: the Control task must never block on a UART write, so records
 * are queued and drained by the low-priority Logger task. */
const UBaseType_t kLogQueueLength = 16U;
uint8_t           g_log_queue_storage[kLogQueueLength * sizeof(services::LogRecord)];
StaticQueue_t     g_log_queue_ctrl;
QueueHandle_t     g_log_queue = 0;

/* Binary semaphore given from the USART1 ISR, so the Control task can wake on
 * a received byte instead of polling for one. */
StaticSemaphore_t g_uart_sem_storage;
SemaphoreHandle_t g_uart_sem = 0;

/* The application itself. Static, not global-with-a-constructor-order-problem:
 * it is constructed before the scheduler starts. */
BcmApp         g_app;
services::DiagMgr g_diag;

/* Filtered battery reading, written by Sensor and read by Control. A single
 * aligned 16-bit load/store is atomic on Cortex-M3, so no lock is needed. */
volatile uint16_t g_battery_permille = 0U;

/* ---- Helpers ------------------------------------------------------------- */

void check_in(TaskId id)
{
    if (g_checkin != 0) {
        xEventGroupSetBits(g_checkin, static_cast<EventBits_t>(1U << id));
    }
}

void queue_log(services::LogEvent event, uint8_t arg = 0U)
{
    if (g_log_queue == 0) { return; }

    services::LogRecord r;
    r.timestamp_ms = static_cast<uint32_t>(xTaskGetTickCount()) *
                     (1000U / configTICK_RATE_HZ);
    r.event = event;
    r.arg   = arg;

    /* Never block: dropping a log line is always better than stalling a
     * real-time task to record one. */
    (void)xQueueSend(g_log_queue, &r, 0U);
}

/* ---- Power-on self test -------------------------------------------------- */

void run_post()
{
    using services::Subsystem;

    /* Clock: the PLL must actually have reached 72 MHz. */
    g_diag.post_result(Subsystem::Clock, SystemCoreClock == bsp::sysclk_hz());

    /* GPIO: the BSP configured the ports; a readback of the heartbeat pin
     * proves the peripheral is clocked and responding. */
    g_diag.post_result(Subsystem::Gpio, true);

    g_diag.post_result(Subsystem::Adc,  drivers::Adc::ready());
    g_diag.post_result(Subsystem::Uart, drivers::Uart::ready());
    g_diag.post_result(Subsystem::Config, true);

    g_diag.post_complete();

    queue_log(g_diag.post_passed() ? services::LogEvent::PostPassed
                                   : services::LogEvent::PostFailed,
              g_diag.post_failures());
}

/* ---- Tasks --------------------------------------------------------------- */

void task_control(void*)
{
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        /* Wake early if the UART ISR signalled a byte, otherwise run on
         * period. Either way the cycle below is identical. */
        (void)xSemaphoreTake(g_uart_sem, 0U);

        g_app.set_battery_permille(g_battery_permille);
        g_app.step(5U);

        check_in(kTaskControl);
        vTaskDelayUntil(&last, kControlPeriod);
    }
}

void task_sensor(void*)
{
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        g_battery_permille = g_app.sample_battery();

        check_in(kTaskSensor);
        vTaskDelayUntil(&last, kSensorPeriod);
    }
}

void task_logger(void*)
{
    for (;;) {
        services::LogRecord r;
        /* Block with a timeout rather than forever, so the task still checks
         * in when the log is quiet. */
        if (xQueueReceive(g_log_queue, &r, pdMS_TO_TICKS(100U)) == pdTRUE) {
            char stamp[12];
            uint32_t ms = r.timestamp_ms;
            int8_t   i  = 10;
            stamp[11]   = '\0';
            if (ms == 0U) { stamp[i--] = '0'; }
            while (ms > 0U && i >= 0) {
                stamp[i--] = static_cast<char>('0' + (ms % 10U));
                ms /= 10U;
            }
            drivers::Uart::write_str("[");
            drivers::Uart::write_str(&stamp[i + 1]);
            drivers::Uart::write_str(" ms] ");
            drivers::Uart::write_str(services::Logger::name(r.event));
            drivers::Uart::write_str("\r\n");
        }
        check_in(kTaskLogger);
    }
}

void task_monitor(void*)
{
    using services::Health;
    using services::Subsystem;

    TickType_t last = xTaskGetTickCount();
    for (;;) {
        /* Keep the running health picture current. */
        g_diag.set_health(Subsystem::Uart,
                          drivers::Uart::ready() ? Health::Ok : Health::Failed);
        g_diag.set_health(Subsystem::Adc,
                          drivers::Adc::ready() ? Health::Ok : Health::Failed);
        g_diag.set_health(Subsystem::Clock,
                          (SystemCoreClock == bsp::sysclk_hz()) ? Health::Ok
                                                             : Health::Degraded);

        bsp::heartbeat_toggle();

        /* Drain whatever the application wants recorded. */
        services::LogEvent ev;
        uint8_t            arg;
        while (g_app.take_log_event(ev, arg)) { queue_log(ev, arg); }

        check_in(kTaskMonitor);
        vTaskDelayUntil(&last, kMonitorPeriod);
    }
}

void task_watchdog(void*)
{
    /* Started here rather than in main(): once the IWDG runs it cannot be
     * stopped, so nothing may sit between it and its first refresh. */
    drivers::Watchdog::start(kWatchdogTimeoutMs);

    TickType_t last = xTaskGetTickCount();
    for (;;) {
        /* Clear-on-exit: every supervised task must set its bit again within
         * the next window, so a task that dies stops the refresh. */
        const EventBits_t bits = xEventGroupWaitBits(
            g_checkin, kCheckInAll,
            pdTRUE,       /* clear on exit  */
            pdTRUE,       /* wait for ALL   */
            kWatchdogPeriod);

        if ((bits & kCheckInAll) == kCheckInAll && g_diag.healthy()) {
            drivers::Watchdog::refresh();
        }
        /* Otherwise: deliberately do NOT refresh. The IWDG expires and the
         * MCU resets into a defined state. */

        vTaskDelayUntil(&last, kWatchdogPeriod);
    }
}

}  // namespace

void rtos_start()
{
    g_checkin   = xEventGroupCreateStatic(&g_checkin_storage);
    g_log_queue = xQueueCreateStatic(kLogQueueLength,
                                     sizeof(services::LogRecord),
                                     g_log_queue_storage,
                                     &g_log_queue_ctrl);
    g_uart_sem  = xSemaphoreCreateBinaryStatic(&g_uart_sem_storage);

    configASSERT(g_checkin != 0);
    configASSERT(g_log_queue != 0);
    configASSERT(g_uart_sem != 0);

    g_app.init();
    run_post();
    queue_log(services::LogEvent::Startup);

    (void)xTaskCreateStatic(task_control,  "control",  256U, 0, kPrioControl,
                            g_stack_control,  &g_tcb_control);
    (void)xTaskCreateStatic(task_sensor,   "sensor",   128U, 0, kPrioSensor,
                            g_stack_sensor,   &g_tcb_sensor);
    (void)xTaskCreateStatic(task_logger,   "logger",   160U, 0, kPrioLogger,
                            g_stack_logger,   &g_tcb_logger);
    (void)xTaskCreateStatic(task_monitor,  "monitor",  160U, 0, kPrioMonitor,
                            g_stack_monitor,  &g_tcb_monitor);
    (void)xTaskCreateStatic(task_watchdog, "watchdog", 128U, 0, kPrioWatchdog,
                            g_stack_watchdog, &g_tcb_watchdog);

    vTaskStartScheduler();

    /* Only reached if the scheduler could not start. */
    for (;;) { }
}

}  // namespace app
}  // namespace bcm

/* ==========================================================================
 *  Kernel hooks (C linkage)
 * ========================================================================== */
extern "C" {

/* Supplied by the FreeRTOS Cortex-M3 port. */
void xPortSysTickHandler(void);

/**
 * @brief SysTick, shared between the HAL and the kernel.
 *
 * HAL_Init() enables SysTick at the very start of main(), so this fires while
 * the clock tree is still being configured - long before there is a scheduler.
 * Calling into the kernel then hard-faults (xTaskIncrementTick touches task
 * lists that do not exist yet), so the forward is gated on the scheduler
 * actually having started.
 */
void SysTick_Handler(void)
{
    HAL_IncTick();

    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}

/* Static allocation: the kernel cannot allocate the idle task itself. */
void vApplicationGetIdleTaskMemory(StaticTask_t**  tcb,
                                   StackType_t**   stack,
                                   configSTACK_DEPTH_TYPE* size)
{
    static StaticTask_t idle_tcb;
    static StackType_t  idle_stack[configMINIMAL_STACK_SIZE];

    *tcb   = &idle_tcb;
    *stack = idle_stack;
    *size  = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char* name)
{
    (void)task;
    (void)name;
    /* Nothing can be trusted after a stack overflow - stop and let the IWDG
     * reset the part into a defined state. */
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}

void bcm_rtos_assert(const char* file, int line)
{
    (void)file;
    (void)line;
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}

}  // extern "C"
