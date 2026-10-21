/*_____ I N C L U D E S ____________________________________________________*/
#include <stdio.h>
#include <string.h>
#include "NuMicro.h"

#include "misc_config.h"
#include "timer_service.h"
#include "hid_transfer.h"
#include "hid_tool_api.h"

/*_____ D E C L A R A T I O N S ____________________________________________*/

volatile struct flag_32bit flag_PROJ_CTL;
#define FLAG_PROJ_TIMER_PERIOD_1000MS                  (flag_PROJ_CTL.bit0)
#define FLAG_PROJ_REVERSE1                             (flag_PROJ_CTL.bit1)
#define FLAG_PROJ_REVERSE2                             (flag_PROJ_CTL.bit2)
#define FLAG_PROJ_REVERSE3                             (flag_PROJ_CTL.bit3)
#define FLAG_PROJ_REVERSE4                             (flag_PROJ_CTL.bit4)
#define FLAG_PROJ_REVERSE5                             (flag_PROJ_CTL.bit5)
#define FLAG_PROJ_REVERSE6                             (flag_PROJ_CTL.bit6)
#define FLAG_PROJ_REVERSE7                             (flag_PROJ_CTL.bit7)

/*_____ D E F I N I T I O N S ______________________________________________*/

volatile unsigned long counter_systick = 0;
volatile uint32_t counter_tick = 0;

static int g_timer_id_task1 = -1;
static int g_timer_id_task2 = -1;
static int g_timer_id_task3 = -1;

#define CRYSTAL_LESS                                    1u
#define TRIM_INIT                                       (SYS_BASE + 0x10C)
#define TRIM_THRESHOLD                                  16u

/*_____ M A C R O S ________________________________________________________*/

#define LED_TOGGLE()                                   (PB10 ^= 1)

#if CRYSTAL_LESS
static volatile uint32_t s_u32DefaultTrim = 0u;
static volatile uint32_t s_u32LastTrim = 0u;
#endif

/*_____ F U N C T I O N S __________________________________________________*/

unsigned long get_systick(void)
{
    return (counter_systick);
}

void set_systick(unsigned long t)
{
    counter_systick = t;
}

void systick_counter(void)
{
    counter_systick++;
}

void SysTick_Handler(void)
{
    systick_counter();

    #if defined (ENABLE_TICK_EVENT)
    TickCheckTickEvent();
    #endif
}

void SysTick_delay(unsigned long delay)
{
    unsigned long tickstart;
    unsigned long wait;

    tickstart = get_systick();
    wait = delay;

    while ((get_systick() - tickstart) < wait)
    {
    }
}

void SysTick_enable(unsigned long ticks_per_second)
{
    set_systick(0);

    if (SysTick_Config(SystemCoreClock / ticks_per_second))
    {
        printf("Set system tick error!!\n");
        while (1);
    }

    #if defined (ENABLE_TICK_EVENT)
    TickInitTickEvent();
    #endif
}

uint32_t get_tick(void)
{
    return (counter_tick);
}

void set_tick(uint32_t t)
{
    counter_tick = t;
}

void tick_counter(void)
{
    counter_tick++;
}

void delay_ms(uint16_t ms)
{
    #if 1
    uint32_t start;

    start = get_tick();
    while ((uint32_t)(get_tick() - start) < (uint32_t)ms)
    {
    }
    #else
    TIMER_Delay(TIMER0, 1000 * ms);
    #endif
}

void USB_trim_process(void)
{
#if CRYSTAL_LESS
    if ((SYS->TCTL48M & SYS_TCTL48M_FREQSEL_Msk) != 1u)
    {
        if (USBD->INTSTS & USBD_INTSTS_SOFIF_Msk)
        {
            USBD->INTSTS = USBD_INTSTS_SOFIF_Msk;
            SYS->TCTL48M = 0x01u;
            SYS->TCTL48M |= SYS_TCTL48M_REFCKSEL_Msk;
        }
    }

    if (SYS->TISTS48M & (SYS_TISTS48M_CLKERRIF_Msk | SYS_TISTS48M_TFAILIF_Msk))
    {
        M32(TRIM_INIT) = s_u32LastTrim;
        SYS->TCTL48M = 0u;
        SYS->TISTS48M = SYS_TISTS48M_CLKERRIF_Msk | SYS_TISTS48M_TFAILIF_Msk;
        USBD->INTSTS = USBD_INTSTS_SOFIF_Msk;
    }

    if ((M32(TRIM_INIT) > (s_u32DefaultTrim + TRIM_THRESHOLD)) ||
        (M32(TRIM_INIT) < (s_u32DefaultTrim - TRIM_THRESHOLD)))
    {
        M32(TRIM_INIT) = s_u32LastTrim;
    }
    else
    {
        s_u32LastTrim = M32(TRIM_INIT);
    }
#endif
}

void USB_HID_Init(void)
{
    USBD_Open(&gsInfo, HID_ClassRequest, NULL);
    HID_Init();
    USBD_Start();
    NVIC_EnableIRQ(USBD_IRQn);

#if CRYSTAL_LESS
    s_u32DefaultTrim = M32(TRIM_INIT);
    s_u32LastTrim = s_u32DefaultTrim;
#endif

    USBD->INTSTS = USBD_INTSTS_SOFIF_Msk;
}

void Task_1000ms_Callback(void *user_data)
{
    static uint32_t LOG1 = 0;

    UNUSED(user_data);

    printf("%s(timer) : %4d\r\n", __FUNCTION__, LOG1++);
    LED_TOGGLE();
}

void Task_100ms_Callback(void *user_data)
{
    UNUSED(user_data);
}

void Task_10ms_Callback(void *user_data)
{
    UNUSED(user_data);
}

void TimerService_CreateTask(void)
{
    g_timer_id_task1 = TimerService_CreateTimer(10U, Task_10ms_Callback, (void *)0);
    if (g_timer_id_task1 >= 0)
    {
        TimerService_StartTimer((unsigned int)g_timer_id_task1);
        printf("task1 id = %d\r\n", g_timer_id_task1);
    }

    g_timer_id_task2 = TimerService_CreateTimer(100U, Task_100ms_Callback, (void *)0);
    if (g_timer_id_task2 >= 0)
    {
        TimerService_StartTimer((unsigned int)g_timer_id_task2);
        printf("task2 id = %d\r\n", g_timer_id_task2);
    }

    g_timer_id_task3 = TimerService_CreateTimer(1000U, Task_1000ms_Callback, (void *)0);
    if (g_timer_id_task3 >= 0)
    {
        TimerService_StartTimer((unsigned int)g_timer_id_task3);
        printf("task3 id = %d\r\n", g_timer_id_task3);
    }
}

uint8_t check_reset_source(void)
{
    uint32_t src;

    src = SYS_GetResetSrc();
    SYS->RSTSTS |= 0x1FF;
    printf("Reset Source <0x%08X>\r\n", src);

    #if 1
    if (src & BIT0)
    {
        printf("0)POR Reset Flag\r\n");
    }
    if (src & BIT1)
    {
        printf("1)NRESET Pin Reset Flag\r\n");
    }
    if (src & BIT2)
    {
        printf("2)WDT Reset Flag\r\n");
    }
    if (src & BIT3)
    {
        printf("3)LVR Reset Flag\r\n");
    }
    if (src & BIT4)
    {
        printf("4)BOD Reset Flag\r\n");
    }
    if (src & BIT5)
    {
        printf("5)System Reset Flag \r\n");
    }
    if (src & BIT6)
    {
        printf("6)Reserved.\r\n");
    }
    if (src & BIT7)
    {
        printf("7)CPU Reset Flag\r\n");
    }
    if (src & BIT8)
    {
        printf("8)CPU Lockup Reset Flag\r\n");
    }
    #endif

    if (src & SYS_RSTSTS_PORF_Msk)
    {
        SYS_ClearResetSrc(SYS_RSTSTS_PORF_Msk);
        printf("power on from POR\r\n");
        return FALSE;
    }
    else if (src & SYS_RSTSTS_PINRF_Msk)
    {
        SYS_ClearResetSrc(SYS_RSTSTS_PINRF_Msk);
        printf("power on from nRESET pin\r\n");
        return FALSE;
    }
    else if (src & SYS_RSTSTS_WDTRF_Msk)
    {
        SYS_ClearResetSrc(SYS_RSTSTS_WDTRF_Msk);
        printf("power on from WDT Reset\r\n");
        return FALSE;
    }
    else if (src & SYS_RSTSTS_LVRF_Msk)
    {
        SYS_ClearResetSrc(SYS_RSTSTS_LVRF_Msk);
        printf("power on from LVR Reset\r\n");
        return FALSE;
    }
    else if (src & SYS_RSTSTS_BODRF_Msk)
    {
        SYS_ClearResetSrc(SYS_RSTSTS_BODRF_Msk);
        printf("power on from BOD Reset\r\n");
        return FALSE;
    }
    else if (src & SYS_RSTSTS_SYSRF_Msk)
    {
        SYS_ClearResetSrc(SYS_RSTSTS_SYSRF_Msk);
        printf("power on from System Reset\r\n");
        return FALSE;
    }
    else if (src & SYS_RSTSTS_CPURF_Msk)
    {
        SYS_ClearResetSrc(SYS_RSTSTS_CPURF_Msk);
        printf("power on from CPU reset\r\n");
        return FALSE;
    }
    else if (src & SYS_RSTSTS_CPULKRF_Msk)
    {
        SYS_ClearResetSrc(SYS_RSTSTS_CPULKRF_Msk);
        printf("power on from CPU Lockup Reset\r\n");
        return FALSE;
    }

    printf("power on from unhandle reset source\r\n");
    return FALSE;
}

void TMR1_IRQHandler(void)
{
    if (TIMER_GetIntFlag(TIMER1) == 1)
    {
        TIMER_ClearIntFlag(TIMER1);
        tick_counter();
        TimerService_Tick1ms();
    }
}

void TIMER1_Init(void)
{
    TIMER_Open(TIMER1, TIMER_PERIODIC_MODE, 1000);
    TIMER_EnableInt(TIMER1);
    NVIC_EnableIRQ(TMR1_IRQn);
    TIMER_Start(TIMER1);
}

void loop(void)
{
    TimerService_Dispatch();
    USB_trim_process();
    HidTool_Process();

    if ((get_systick() % 1000) == 0)
    {
    }
}

void UARTx_Process(void)
{
    uint8_t res;

    res = UART_READ(UART0);

    if (res > 0x7F)
    {
        printf("invalid command\r\n");
    }
    else
    {
        printf("press : %c\r\n", res);

        switch (res)
        {
            case '1':
                break;

            case 'X':
            case 'x':
            case 'Z':
            case 'z':
                SYS_UnlockReg();
                SYS_ResetChip();
                break;
        }
    }
}

void UART0_IRQHandler(void)
{
    if (UART_GET_INT_FLAG(UART0, UART_INTSTS_RDAINT_Msk | UART_INTSTS_RXTOINT_Msk))
    {
        while (UART_GET_RX_EMPTY(UART0) == 0)
        {
            UARTx_Process();
        }
    }

    if (UART0->FIFOSTS & (UART_FIFOSTS_BIF_Msk |
                          UART_FIFOSTS_FEF_Msk |
                          UART_FIFOSTS_PEF_Msk |
                          UART_FIFOSTS_RXOVIF_Msk))
    {
        UART_ClearIntFlag(UART0, (UART_INTSTS_RLSINT_Msk | UART_INTSTS_BUFERRINT_Msk));
    }
}

void UART0_Init(void)
{
    SYS_ResetModule(UART0_RST);

    UART_Open(UART0, 115200);
    UART_EnableInt(UART0, UART_INTEN_RDAIEN_Msk | UART_INTEN_RXTOIEN_Msk);
    NVIC_EnableIRQ(UART0_IRQn);

    #if (_debug_log_UART_ == 1)
    dbg_printf("\r\nCLK_GetCPUFreq : %8d\r\n", CLK_GetCPUFreq());
    dbg_printf("CLK_GetHCLKFreq : %8d\r\n", CLK_GetHCLKFreq());
    dbg_printf("CLK_GetHXTFreq : %8d\r\n", CLK_GetHXTFreq());
    dbg_printf("CLK_GetLXTFreq : %8d\r\n", CLK_GetLXTFreq());
    dbg_printf("CLK_GetPCLK0Freq : %8d\r\n", CLK_GetPCLK0Freq());
    dbg_printf("CLK_GetPCLK1Freq : %8d\r\n", CLK_GetPCLK1Freq());
    #endif
}

void GPIO_Init(void)
{
    SYS->GPB_MFPH = (SYS->GPB_MFPH & ~SYS_GPB_MFPH_PB10MFP_Msk) |
                    SYS_GPB_MFPH_PB10MFP_GPIO;

    GPIO_SetMode(PB, BIT10, GPIO_MODE_OUTPUT);
    PB10 = 0;
}

void SYS_Init(void)
{
    SYS_UnlockReg();

    CLK_EnableXtalRC(CLK_PWRCTL_HIRCEN_Msk);
    CLK_WaitClockReady(CLK_STATUS_HIRCSTB_Msk);

    CLK_EnableXtalRC(CLK_PWRCTL_HIRC48EN_Msk);
    CLK_WaitClockReady(CLK_STATUS_HIRC48STB_Msk);

    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HIRC48, CLK_CLKDIV0_HCLK(1));

    CLK_SetModuleClock(USBD_MODULE, CLK_CLKSEL0_USBSEL_HIRC48, CLK_CLKDIV0_USB(1));
    CLK_EnableModuleClock(USBD_MODULE);

    SYS->USBPHY = (SYS->USBPHY & ~SYS_USBPHY_USBROLE_Msk) |
                  SYS_USBPHY_OTGPHYEN_Msk |
                  SYS_USBPHY_SBO_Msk;

    CLK_EnableModuleClock(TMR1_MODULE);
    CLK_SetModuleClock(TMR1_MODULE, CLK_CLKSEL1_TMR1SEL_HIRC, 0);

    CLK_EnableModuleClock(UART0_MODULE);
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART0SEL_HIRC, CLK_CLKDIV0_UART0(1));

    SYS->GPA_MFPH = (SYS->GPA_MFPH & ~(SYS_GPA_MFPH_PA12MFP_Msk |
                                       SYS_GPA_MFPH_PA13MFP_Msk |
                                       SYS_GPA_MFPH_PA14MFP_Msk |
                                       SYS_GPA_MFPH_PA15MFP_Msk)) |
                    (SYS_GPA_MFPH_PA12MFP_USB_VBUS |
                     SYS_GPA_MFPH_PA13MFP_USB_D_N |
                     SYS_GPA_MFPH_PA14MFP_USB_D_P |
                     SYS_GPA_MFPH_PA15MFP_USB_OTG_ID);

    SYS->GPB_MFPH = (SYS->GPB_MFPH & ~(SYS_GPB_MFPH_PB12MFP_Msk |
                                       SYS_GPB_MFPH_PB13MFP_Msk)) |
                    (SYS_GPB_MFPH_PB12MFP_UART0_RXD |
                     SYS_GPB_MFPH_PB13MFP_UART0_TXD);

    SystemCoreClockUpdate();

    SYS_LockReg();
}

/*
 * This is a template project for M261 series MCU. Users could based on this project to create their
 * own application without worry about the IAR/Keil project settings.
 */

int main(void)
{
    SYS_Init();

    GPIO_Init();
    UART0_Init();
    TIMER1_Init();
    check_reset_source();

    SysTick_enable(1000);
    #if defined (ENABLE_TICK_EVENT)
    TickSetTickEvent(1000, TickCallback_processA);
    TickSetTickEvent(5000, TickCallback_processB);
    #endif

    TimerService_Init();
    TimerService_CreateTask();

    USB_HID_Init();
    printf("M261 PMBus/SMBus HID bridge ready\r\n");

    while (1)
    {
        loop();
    }
}

/*** (C) COPYRIGHT 2017 Nuvoton Technology Corp. ***/
