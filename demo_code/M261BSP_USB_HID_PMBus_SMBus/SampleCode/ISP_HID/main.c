/******************************************************************************
 * @file     main.c
 * @brief    ISP tool main function
 * @version  0x32
 *
 * @note
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @copyright Copyright (C) 2019 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "targetdev.h"
#include "hid_transfer.h"

// For M261 Ver. C API
volatile ISP_INFO_T     g_ISPInfo = {0};
volatile BL_USBD_INFO_T g_USBDInfo = {0};

#define TRIM_INIT           (SYS_BASE+0x10C)
#define TRIM_THRESHOLD      16      /* Each value is 0.125%, max 2% */
#define IAP_LED_WAIT_SHORT_MS 250UL
#define IAP_LED_WAIT_PAUSE_MS 1000UL
#define IAP_LED_WAIT_PAUSE_STEP 6u
#define IAP_LED_WAIT_STEP_COUNT 7u
#define IAP_LED_PULSE_MS    80UL
#define IAP_LED_PIN_SHIFT   20UL
#define IAP_LED_PIN_MASK    (0x3UL << IAP_LED_PIN_SHIFT)

static volatile uint32_t s_u32DefaultTrim, s_u32LastTrim;
static volatile uint32_t s_u32IapMsTicks;
static uint32_t s_u32LedLastToggleMs;
static uint32_t s_u32LedPulseStartMs;
static uint8_t s_u8LedPulseActive;
static uint8_t s_u8LedWaitStep;
int32_t g_FMC_i32ErrCode;

typedef enum
{
    IAP_LED_STATUS_OFF = 0,
    IAP_LED_STATUS_CHECKING,
    IAP_LED_STATUS_WAIT_UPDATE,
    IAP_LED_STATUS_JUMP_APP
} IAP_LED_STATUS_T;

static IAP_LED_STATUS_T s_eLedStatus = IAP_LED_STATUS_OFF;

void ProcessHardFault(void) {}
void SH_Return(void) {}
void SendChar_ToUART(int ch) {}

void SysTick_Handler(void)
{
    s_u32IapMsTicks++;
}

uint32_t CLK_GetPLLClockFreq(void)
{
    return 48000000;
}

uint32_t CLK_GetCPUFreq(void)
{
    return 48000000;
}

static uint32_t IAP_GetMsTicks(void)
{
    return s_u32IapMsTicks;
}

static void IAP_TickInit(void)
{
    (void)SysTick_Config(CLK_GetCPUFreq() / 1000UL);
}

static void IAP_LED_Write(uint8_t u8On)
{
    if(u8On != 0u)
    {
        PB10 = 0;
    }
    else
    {
        PB10 = 1;
    }
}

static void IAP_LED_Init(void)
{
    SYS->GPB_MFPH = (SYS->GPB_MFPH & ~SYS_GPB_MFPH_PB10MFP_Msk) | SYS_GPB_MFPH_PB10MFP_GPIO;
    PB->MODE = (PB->MODE & ~IAP_LED_PIN_MASK) | (GPIO_MODE_OUTPUT << IAP_LED_PIN_SHIFT);
    s_eLedStatus = IAP_LED_STATUS_OFF;
    s_u32LedLastToggleMs = IAP_GetMsTicks();
    s_u32LedPulseStartMs = 0UL;
    s_u8LedPulseActive = FALSE;
    s_u8LedWaitStep = 0u;
    IAP_LED_Write(FALSE);
}

static void IAP_LED_SetStatus(IAP_LED_STATUS_T eStatus)
{
    s_eLedStatus = eStatus;
    s_u32LedLastToggleMs = IAP_GetMsTicks();
    s_u8LedPulseActive = FALSE;
    s_u8LedWaitStep = 0u;

    if(eStatus == IAP_LED_STATUS_CHECKING)
    {
        IAP_LED_Write(TRUE);
    }
    else if(eStatus == IAP_LED_STATUS_WAIT_UPDATE)
    {
        IAP_LED_Write(TRUE);
    }
    else
    {
        IAP_LED_Write(FALSE);
    }
}

static void IAP_LED_PulseActivity(void)
{
    s_u32LedPulseStartMs = IAP_GetMsTicks();
    s_u8LedPulseActive = TRUE;
    IAP_LED_Write(TRUE);
}

static void IAP_LED_Task(void)
{
    uint32_t u32Now;

    u32Now = IAP_GetMsTicks();

    if(s_u8LedPulseActive != FALSE)
    {
        if((u32Now - s_u32LedPulseStartMs) < IAP_LED_PULSE_MS)
        {
            IAP_LED_Write(TRUE);
            return;
        }

        s_u8LedPulseActive = FALSE;
        IAP_LED_Write(FALSE);
        s_u32LedLastToggleMs = u32Now;
        s_u8LedWaitStep = 1u;
    }

    if(s_eLedStatus == IAP_LED_STATUS_WAIT_UPDATE)
    {
        uint32_t u32StepMs;

        u32StepMs = (s_u8LedWaitStep == IAP_LED_WAIT_PAUSE_STEP) ? IAP_LED_WAIT_PAUSE_MS : IAP_LED_WAIT_SHORT_MS;

        if((u32Now - s_u32LedLastToggleMs) >= u32StepMs)
        {
            s_u32LedLastToggleMs = u32Now;
            s_u8LedWaitStep++;

            if(s_u8LedWaitStep >= IAP_LED_WAIT_STEP_COUNT)
            {
                s_u8LedWaitStep = 0u;
            }

            if((s_u8LedWaitStep < IAP_LED_WAIT_PAUSE_STEP) && ((s_u8LedWaitStep & 0x1u) == 0u))
            {
                IAP_LED_Write(TRUE);
            }
            else
            {
                IAP_LED_Write(FALSE);
            }
        }
    }
    else if(s_eLedStatus == IAP_LED_STATUS_CHECKING)
    {
        IAP_LED_Write(TRUE);
    }
    else
    {
        IAP_LED_Write(FALSE);
    }
}

static uint32_t IAP_MailboxChecksum(uint32_t u32Version, uint32_t u32Command, uint32_t u32Argument0, uint32_t u32Argument1)
{
    return (M261_IAP_MAILBOX_MAGIC ^
            M261_IAP_MAILBOX_MAGIC_INV ^
            u32Version ^
            u32Command ^
            u32Argument0 ^
            u32Argument1 ^
            M261_IAP_MAILBOX_CHECK_XOR);
}

static void IAP_MailboxClear(void)
{
    volatile M261_IAP_MAILBOX_T *psMailbox;
    uint32_t i;

    psMailbox = (volatile M261_IAP_MAILBOX_T *)M261_IAP_MAILBOX_ADDR;
    psMailbox->magic = 0UL;
    psMailbox->magic_inv = 0UL;
    psMailbox->version = 0UL;
    psMailbox->command = 0UL;
    psMailbox->argument0 = 0UL;
    psMailbox->argument1 = 0UL;
    psMailbox->checksum = 0UL;

    for(i = 0UL; i < 9UL; i++)
    {
        psMailbox->reserved[i] = 0UL;
    }
}

static uint8_t IAP_MailboxConsumeEnterIap(void)
{
    volatile M261_IAP_MAILBOX_T *psMailbox;
    uint32_t u32Checksum;
    uint8_t u8Valid;

    psMailbox = (volatile M261_IAP_MAILBOX_T *)M261_IAP_MAILBOX_ADDR;
    u8Valid = FALSE;

    if((psMailbox->magic == M261_IAP_MAILBOX_MAGIC) &&
       (psMailbox->magic_inv == M261_IAP_MAILBOX_MAGIC_INV) &&
       (psMailbox->version == M261_IAP_MAILBOX_VERSION) &&
       (psMailbox->command == M261_IAP_MAILBOX_CMD_ENTER_IAP))
    {
        u32Checksum = IAP_MailboxChecksum(psMailbox->version,
                                          psMailbox->command,
                                          psMailbox->argument0,
                                          psMailbox->argument1);

        if(psMailbox->checksum == u32Checksum)
        {
            u8Valid = TRUE;
        }
    }

    if(psMailbox->magic == M261_IAP_MAILBOX_MAGIC)
    {
        IAP_MailboxClear();
    }

    return u8Valid;
}

int32_t SYS_Init(void)
{
    uint32_t u32TimeOutCnt;

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init System Clock                                                                                       */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Enable Internal RC 48MHz clock */
    CLK->PWRCTL |= CLK_PWRCTL_HIRC48EN_Msk;

    /* Waiting for Internal RC clock ready */
    u32TimeOutCnt = SystemCoreClock; /* 1 second time-out */
    while(!(CLK->STATUS & CLK_STATUS_HIRC48STB_Msk))
        if(--u32TimeOutCnt == 0) return -1;

    /* Switch HCLK clock source to Internal RC and HCLK source divide 1 */
    CLK->CLKSEL0 = (CLK->CLKSEL0 & (~CLK_CLKSEL0_HCLKSEL_Msk)) | CLK_CLKSEL0_HCLKSEL_HIRC48;
    CLK->CLKDIV0 = (CLK->CLKDIV0 & (~CLK_CLKDIV0_HCLKDIV_Msk)) | CLK_CLKDIV0_HCLK(1);
    /* Use HIRC48 as USB clock source */
    CLK->CLKSEL0 = (CLK->CLKSEL0 & (~CLK_CLKSEL0_USBSEL_Msk)) | CLK_CLKSEL0_USBSEL_HIRC48;
    CLK->CLKDIV0 = (CLK->CLKDIV0 & (~CLK_CLKDIV0_USBDIV_Msk)) | CLK_CLKDIV0_USB(1);
    /* Select USBD */
    SYS->USBPHY = (SYS->USBPHY & ~SYS_USBPHY_USBROLE_Msk) | SYS_USBPHY_OTGPHYEN_Msk | SYS_USBPHY_SBO_Msk;
    /* Enable IP clock */
    CLK->APBCLK0 |= CLK_APBCLK0_USBDCKEN_Msk;
    CLK->AHBCLK |= CLK_AHBCLK_CRCCKEN_Msk;

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init I/O Multi-function                                                                                 */
    /*---------------------------------------------------------------------------------------------------------*/
    /* USBD multi-function pins for VBUS, D+, D-, and ID pins */
    SYS->GPA_MFPH &= ~(SYS_GPA_MFPH_PA12MFP_Msk | SYS_GPA_MFPH_PA13MFP_Msk | SYS_GPA_MFPH_PA14MFP_Msk | SYS_GPA_MFPH_PA15MFP_Msk);
    SYS->GPA_MFPH |= (SYS_GPA_MFPH_PA12MFP_USB_VBUS | SYS_GPA_MFPH_PA13MFP_USB_D_N | SYS_GPA_MFPH_PA14MFP_USB_D_P | SYS_GPA_MFPH_PA15MFP_USB_OTG_ID);

    return 0;
}

static uint32_t IAP_ReadWord(uint32_t u32Addr)
{
    uint32_t u32Data;

    u32Data = 0xFFFFFFFFUL;
    (void)FMC_Read_User(u32Addr, &u32Data);
    return u32Data;
}

static uint8_t IAP_IsApplicationPresent(void)
{
    uint32_t u32Stack;
    uint32_t u32Reset;
    uint32_t u32ResetAddr;
    uint32_t u32Crc;

    u32Stack = IAP_ReadWord(APROM_APPLICATION_START);
    u32Reset = IAP_ReadWord(APROM_APPLICATION_START + 4UL);
    u32Crc = IAP_ReadWord(APROM_APPLICATION_CRC_ADDR);
    u32ResetAddr = u32Reset & 0xFFFFFFFEUL;

    if((u32Stack == 0xFFFFFFFFUL) || (u32Reset == 0xFFFFFFFFUL) || (u32Crc == 0xFFFFFFFFUL))
    {
        return FALSE;
    }

    if((u32Stack < M261_IAP_SRAM_START) || (u32Stack > M261_IAP_SRAM_END))
    {
        return FALSE;
    }

    if((u32Stack & 0x3UL) != 0UL)
    {
        return FALSE;
    }

    if((u32Reset & 0x1UL) == 0UL)
    {
        return FALSE;
    }

    if((u32ResetAddr < APROM_APPLICATION_START) || (u32ResetAddr >= APROM_APPLICATION_END))
    {
        return FALSE;
    }

    return TRUE;
}

static uint32_t IAP_CalculateApplicationCrc32(void)
{
    uint32_t u32Addr;
    uint32_t u32Data;
    uint32_t u32End;

    u32End = APROM_APPLICATION_CRC_ADDR;
    CRC_Open(CRC_32, (CRC_WDATA_RVS | CRC_CHECKSUM_RVS | CRC_CHECKSUM_COM), 0xFFFFFFFFUL, CRC_CPU_WDATA_32);

    for(u32Addr = APROM_APPLICATION_START; u32Addr < u32End; u32Addr += 4UL)
    {
        u32Data = IAP_ReadWord(u32Addr);
        CRC_WRITE_DATA(CRC, u32Data);
    }

    return CRC_GetChecksum();
}

static uint8_t IAP_VerifyApplication(void)
{
    uint32_t u32CrcCalc;
    uint32_t u32CrcApp;

    if(IAP_IsApplicationPresent() == FALSE)
    {
        return FALSE;
    }

    u32CrcCalc = IAP_CalculateApplicationCrc32();
    u32CrcApp = IAP_ReadWord(APROM_APPLICATION_CRC_ADDR);

    if(u32CrcCalc == u32CrcApp)
    {
        return TRUE;
    }

    return FALSE;
}

static void IAP_JumpToApplication(void)
{
    SYS_UnlockReg();
    (void)BL_EnableFMC();
    IAP_LED_SetStatus(IAP_LED_STATUS_JUMP_APP);
    __set_PRIMASK(1);
    FMC_SetVectorPageAddr(APROM_APPLICATION_START);
    SYS->IPRST0 = SYS_IPRST0_CPURST_Msk;

    while(1);
}

/*---------------------------------------------------------------------------------------------------------*/
/*  Main Function                                                                                          */
/*---------------------------------------------------------------------------------------------------------*/
int32_t main(void)
{
    volatile ISP_INFO_T      *pISPInfo;
    volatile BL_USBD_INFO_T  *pUSBDInfo;
    uint8_t u8EnterIapRequested;

    pISPInfo = &g_ISPInfo;
    pUSBDInfo = &g_USBDInfo;
    memset((void *)&g_ISPInfo, 0x0, sizeof(ISP_INFO_T));
    memset((void *)&g_USBDInfo, 0x0, sizeof(BL_USBD_INFO_T));

    /* Unlock protected registers */
    SYS_UnlockReg();
    /* Init System, peripheral clock and multi-function I/O */
    if( SYS_Init() < 0 )
    {
        while(1);
    }
    IAP_TickInit();
    IAP_LED_Init();
    IAP_LED_SetStatus(IAP_LED_STATUS_CHECKING);
    u8EnterIapRequested = IAP_MailboxConsumeEnterIap();

    (void)BL_EnableFMC();
    g_apromSize = APROM_APPLICATION_SIZE;
    g_dataFlashAddr = SCU->FNSADDR;

    if((g_dataFlashAddr > APROM_APPLICATION_START) && (g_dataFlashAddr < APROM_APPLICATION_END))
    {
        g_dataFlashSize = (APROM_APPLICATION_END - g_dataFlashAddr);
    }
    else
    {
        g_dataFlashAddr = APROM_APPLICATION_END;
        g_dataFlashSize = 0;
    }

    if((u8EnterIapRequested == FALSE) && (IAP_VerifyApplication() != FALSE))
    {
        goto _APROM;
    }

    IAP_LED_SetStatus(IAP_LED_STATUS_WAIT_UPDATE);

    BL_USBDOpen(&gsInfo, NULL, NULL, (uint32_t *)pUSBDInfo);
    /* Endpoint configuration */
    HID_Init();
    BL_USBDInstallEPHandler(EP3, EP3_Handler, (uint32_t *)pISPInfo->pfnUSBDEP);
    NVIC_EnableIRQ(USBD_IRQn);
    BL_USBDStart();

    /* Backup default trim */
    s_u32DefaultTrim = M32(TRIM_INIT);
    s_u32LastTrim = s_u32DefaultTrim;
    /* Clear SOF */
    USBD->INTSTS = USBD_INTSTS_SOFIF_Msk;

    while(1)
    {
        /* Start USB trim if it is not enabled. */
        if((SYS->TCTL48M & SYS_TCTL48M_FREQSEL_Msk) != 1)
        {
            /* Start USB trim only when SOF */
            if(USBD->INTSTS & USBD_INTSTS_SOFIF_Msk)
            {
                /* Clear SOF */
                USBD->INTSTS = USBD_INTSTS_SOFIF_Msk;
                /* Re-enable crystal-less */
                SYS->TCTL48M = 0x01;
                SYS->TCTL48M |= SYS_TCTL48M_REFCKSEL_Msk;
            }
        }

        /* Disable USB Trim when error */
        if(SYS->TISTS48M & (SYS_TISTS48M_CLKERRIF_Msk | SYS_TISTS48M_TFAILIF_Msk))
        {
            /* Last TRIM */
            M32(TRIM_INIT) = s_u32LastTrim;
            /* Disable crystal-less */
            SYS->TCTL48M = 0;
            /* Clear error flags */
            SYS->TISTS48M = SYS_TISTS48M_CLKERRIF_Msk | SYS_TISTS48M_TFAILIF_Msk;
            /* Clear SOF */
            USBD->INTSTS = USBD_INTSTS_SOFIF_Msk;
        }

        /* Check trim value whether it is over the threshold */
        if((M32(TRIM_INIT) > (s_u32DefaultTrim + TRIM_THRESHOLD)) || (M32(TRIM_INIT) < (s_u32DefaultTrim - TRIM_THRESHOLD)))
        {
            /* Write updated value */
            M32(TRIM_INIT) = s_u32LastTrim;
        }
        else
        {
            /* Backup trim value */
            s_u32LastTrim = M32(TRIM_INIT);
        }

        if(bUsbDataReady == TRUE)
        {
            ParseCmd((uint8_t *)usb_rcvbuf, 64);
            EP2_Handler();
            bUsbDataReady = FALSE;
            IAP_LED_PulseActivity();
        }

        IAP_LED_Task();
    }

_APROM:
    IAP_JumpToApplication();

    /* Trap the CPU */
    while(1);
}

void USBD_IRQHandler(void)
{
    BL_ProcessUSBDInterrupt((uint32_t *)g_ISPInfo.pfnUSBDEP, (uint32_t *)&g_ISPInfo, (uint32_t *)&g_USBDInfo);
}

/*** (C) COPYRIGHT 2019 Nuvoton Technology Corp. ***/
