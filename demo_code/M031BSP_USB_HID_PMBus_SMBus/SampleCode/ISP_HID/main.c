/***************************************************************************//**
 * @file     main.c
 * @brief    ISP HID IAP bootloader main function
 * @version  0x34
 *
 * @note
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2017-2018 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include <stdio.h>
#include "targetdev.h"
#include "hid_transfer.h"
#include "iap_layout.h"

#define TRIM_INIT                 (SYS_BASE+0x118)
#define IAP_LED_WAIT_SHORT_MS     250UL
#define IAP_LED_WAIT_PAUSE_MS     1000UL
#define IAP_LED_WAIT_PAUSE_STEP   6u
#define IAP_LED_WAIT_STEP_COUNT   7u
#define IAP_LED_PULSE_MS          80UL
#define IAP_LED_PIN_SHIFT         28UL
#define IAP_LED_PIN_MASK          (0x3UL << IAP_LED_PIN_SHIFT)

static volatile uint32_t s_u32IapMsTicks;
static uint32_t s_u32LedLastToggleMs;
static uint32_t s_u32LedPulseStartMs;
static uint8_t s_u8LedPulseActive;
static uint8_t s_u8LedWaitStep;

typedef enum
{
    IAP_LED_STATUS_OFF = 0,
    IAP_LED_STATUS_CHECKING,
    IAP_LED_STATUS_WAIT_UPDATE,
    IAP_LED_STATUS_JUMP_APP
} IAP_LED_STATUS_T;

static IAP_LED_STATUS_T s_eLedStatus = IAP_LED_STATUS_OFF;

void SysTick_Handler(void)
{
    s_u32IapMsTicks++;
}

static uint32_t IAP_GetMsTicks(void)
{
    return s_u32IapMsTicks;
}

static void IAP_TickInit(void)
{
    (void)SysTick_Config(SystemCoreClock / 1000UL);
}

static void IAP_LED_Write(uint8_t u8On)
{
    if (u8On != 0u)
    {
        PB14 = 0;
    }
    else
    {
        PB14 = 1;
    }
}

static void IAP_LED_Init(void)
{
    SYS->GPB_MFPH = (SYS->GPB_MFPH & ~SYS_GPB_MFPH_PB14MFP_Msk) | SYS_GPB_MFPH_PB14MFP_GPIO;
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

    if (eStatus == IAP_LED_STATUS_CHECKING)
    {
        IAP_LED_Write(TRUE);
    }
    else if (eStatus == IAP_LED_STATUS_WAIT_UPDATE)
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
    uint32_t u32StepMs;

    u32Now = IAP_GetMsTicks();

    if (s_u8LedPulseActive != FALSE)
    {
        if ((u32Now - s_u32LedPulseStartMs) < IAP_LED_PULSE_MS)
        {
            IAP_LED_Write(TRUE);
            return;
        }

        s_u8LedPulseActive = FALSE;
        IAP_LED_Write(FALSE);
        s_u32LedLastToggleMs = u32Now;
        s_u8LedWaitStep = 1u;
    }

    if (s_eLedStatus == IAP_LED_STATUS_WAIT_UPDATE)
    {
        u32StepMs = (s_u8LedWaitStep == IAP_LED_WAIT_PAUSE_STEP) ? IAP_LED_WAIT_PAUSE_MS : IAP_LED_WAIT_SHORT_MS;

        if ((u32Now - s_u32LedLastToggleMs) >= u32StepMs)
        {
            s_u32LedLastToggleMs = u32Now;
            s_u8LedWaitStep++;

            if (s_u8LedWaitStep >= IAP_LED_WAIT_STEP_COUNT)
            {
                s_u8LedWaitStep = 0u;
            }

            if ((s_u8LedWaitStep < IAP_LED_WAIT_PAUSE_STEP) && ((s_u8LedWaitStep & 0x1u) == 0u))
            {
                IAP_LED_Write(TRUE);
            }
            else
            {
                IAP_LED_Write(FALSE);
            }
        }
    }
    else if (s_eLedStatus == IAP_LED_STATUS_CHECKING)
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
    return (M032_IAP_MAILBOX_MAGIC ^
            M032_IAP_MAILBOX_MAGIC_INV ^
            u32Version ^
            u32Command ^
            u32Argument0 ^
            u32Argument1 ^
            M032_IAP_MAILBOX_CHECK_XOR);
}

static void IAP_MailboxClear(void)
{
    volatile M032_IAP_MAILBOX_T *psMailbox;
    uint32_t i;

    psMailbox = (volatile M032_IAP_MAILBOX_T *)M032_IAP_MAILBOX_ADDR;
    psMailbox->magic = 0UL;
    psMailbox->magic_inv = 0UL;
    psMailbox->version = 0UL;
    psMailbox->command = 0UL;
    psMailbox->argument0 = 0UL;
    psMailbox->argument1 = 0UL;
    psMailbox->checksum = 0UL;

    for (i = 0UL; i < 9UL; i++)
    {
        psMailbox->reserved[i] = 0UL;
    }
}

static uint8_t IAP_MailboxConsumeEnterIap(void)
{
    volatile M032_IAP_MAILBOX_T *psMailbox;
    uint32_t u32Checksum;
    uint8_t u8Valid;

    psMailbox = (volatile M032_IAP_MAILBOX_T *)M032_IAP_MAILBOX_ADDR;
    u8Valid = FALSE;

    if ((psMailbox->magic == M032_IAP_MAILBOX_MAGIC) &&
        (psMailbox->magic_inv == M032_IAP_MAILBOX_MAGIC_INV) &&
        (psMailbox->version == M032_IAP_MAILBOX_VERSION) &&
        (psMailbox->command == M032_IAP_MAILBOX_CMD_ENTER_IAP))
    {
        u32Checksum = IAP_MailboxChecksum(psMailbox->version,
                                          psMailbox->command,
                                          psMailbox->argument0,
                                          psMailbox->argument1);

        if (psMailbox->checksum == u32Checksum)
        {
            u8Valid = TRUE;
        }
    }

    if (psMailbox->magic == M032_IAP_MAILBOX_MAGIC)
    {
        IAP_MailboxClear();
    }

    return u8Valid;
}

/*--------------------------------------------------------------------------*/
void SYS_Init(void)
{
    /* Unlock protected registers */
    SYS_UnlockReg();
    /* Set XT1_OUT(PF.2) and XT1_IN(PF.3) to input mode */
    PF->MODE &= ~(GPIO_MODE_MODE2_Msk | GPIO_MODE_MODE3_Msk);
    /* Enable Internal High speed RC oscillator (HIRC) */
    CLK->PWRCTL |= CLK_PWRCTL_HIRCEN_Msk;
    /* Waiting for Internal High speed RC clock ready */
    while ((CLK->STATUS & CLK_STATUS_HIRCSTB_Msk) != CLK_STATUS_HIRCSTB_Msk);

    /* Switch HCLK clock source to HIRC */
    CLK->CLKSEL0 = (CLK->CLKSEL0 & ~CLK_CLKSEL0_HCLKSEL_Msk) | CLK_CLKSEL0_HCLKSEL_HIRC;
    /* Switch USB clock source to HIRC */
    CLK->CLKSEL0 = (CLK->CLKSEL0 & ~CLK_CLKSEL0_USBDSEL_Msk) | CLK_CLKSEL0_HCLKSEL_HIRC;
    /* USB Clock = HIRC / 1 */
    CLK->CLKDIV0 = CLK->CLKDIV0 & ~CLK_CLKDIV0_USBDIV_Msk;
    /* Enable USB clock */
    CLK->APBCLK0 |= CLK_APBCLK0_USBDCKEN_Msk;
    /* Enable ISP and CRC clocks */
    CLK->AHBCLK |= (CLK_AHBCLK_ISPCKEN_Msk | CLK_AHBCLK_CRCCKEN_Msk);
    /* Update System Core Clock */
    SystemCoreClockUpdate();
    /* Lock protected registers */
    SYS_LockReg();
}

static uint32_t IAP_ReadWord(uint32_t u32Addr)
{
    uint32_t u32Data;

    u32Data = 0xFFFFFFFFUL;
    (void)FMC_Read_User(u32Addr, (unsigned int *)&u32Data);
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

    if ((u32Stack == 0xFFFFFFFFUL) || (u32Reset == 0xFFFFFFFFUL) || (u32Crc == 0xFFFFFFFFUL))
    {
        return FALSE;
    }

    if ((u32Stack < M032_IAP_SRAM_START) || (u32Stack > M032_IAP_SRAM_END))
    {
        return FALSE;
    }

    if ((u32Stack & 0x3UL) != 0UL)
    {
        return FALSE;
    }

    if ((u32Reset & 0x1UL) == 0UL)
    {
        return FALSE;
    }

    if ((u32ResetAddr < APROM_APPLICATION_START) || (u32ResetAddr >= APROM_APPLICATION_END))
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
    CRC_Open(CRC_32, (CRC_WDATA_RVS | CRC_CHECKSUM_RVS | CRC_CHECKSUM_COM), 0xFFFFFFFFUL, CRC_WDATA_32);

    for (u32Addr = APROM_APPLICATION_START; u32Addr < u32End; u32Addr += 4UL)
    {
        u32Data = IAP_ReadWord(u32Addr);
        CRC_WRITE_DATA(u32Data);
    }

    return CRC_GetChecksum();
}

static uint8_t IAP_VerifyApplication(void)
{
    uint32_t u32CrcCalc;
    uint32_t u32CrcApp;

    if (IAP_IsApplicationPresent() == FALSE)
    {
        return FALSE;
    }

    u32CrcCalc = IAP_CalculateApplicationCrc32();
    u32CrcApp = IAP_ReadWord(APROM_APPLICATION_CRC_ADDR);

    if (u32CrcCalc == u32CrcApp)
    {
        return TRUE;
    }

    return FALSE;
}

static void IAP_JumpToApplication(void)
{
    SYS_UnlockReg();
    FMC->ISPCTL |= FMC_ISPCTL_ISPEN_Msk;
    IAP_LED_SetStatus(IAP_LED_STATUS_JUMP_APP);
    __set_PRIMASK(1);
    FMC_SetVectorPageAddr(APROM_APPLICATION_START);
    SYS->IPRST0 = SYS_IPRST0_CPURST_Msk;

    while (1);
}

/*---------------------------------------------------------------------------------------------------------*/
/*  Main Function                                                                                          */
/*---------------------------------------------------------------------------------------------------------*/
int32_t main(void)
{
    uint32_t u32TrimInit;
    uint8_t u8EnterIapRequested;

    /* Init System, peripheral clock and multi-function I/O */
    SYS_Init();
    SYS_UnlockReg();
    IAP_TickInit();
    IAP_LED_Init();
    IAP_LED_SetStatus(IAP_LED_STATUS_CHECKING);
    u8EnterIapRequested = IAP_MailboxConsumeEnterIap();

    /* Checking if flash page size matches with target chip's */
    if ((GET_CHIP_SERIES_NUM == CHIP_SERIES_NUM_I) || (GET_CHIP_SERIES_NUM == CHIP_SERIES_NUM_G))
    {
        if (FMC_FLASH_PAGE_SIZE != 2048)
        {
            /* FMC_FLASH_PAGE_SIZE is different from target device */
            /* Please enable the compiler option PAGE_SIZE_2048 in fmc.h */
            while (SYS->PDID);
        }
    }
    else
    {
        if (FMC_FLASH_PAGE_SIZE != 512)
        {
            /* FMC_FLASH_PAGE_SIZE is different from target device */
            /* Please disable the compiler option PAGE_SIZE_2048 in fmc.h */
            while (SYS->PDID);
        }
    }

    /* Enable FMC ISP function. Before using FMC function, it should unlock system register first. */
    FMC->ISPCTL |= (FMC_ISPCTL_ISPEN_Msk | FMC_ISPCTL_APUEN_Msk);

    g_apromSize = GetApromSize();
    GetDataFlashInfo(&g_dataFlashAddr, &g_dataFlashSize);

    if ((u8EnterIapRequested == FALSE) && (IAP_VerifyApplication() != FALSE))
    {
        goto _APROM;
    }

    IAP_LED_SetStatus(IAP_LED_STATUS_WAIT_UPDATE);

    /* Open USB controller */
    USBD_Open(&gsInfo, HID_ClassRequest, NULL);
    /* Init Endpoint configuration for HID */
    HID_Init();
    /* Start USB device */
    USBD_Start();
    /* Enable USB device interrupt */
    NVIC_EnableIRQ(USBD_IRQn);

    /* Backup default trim */
    u32TrimInit = M32(TRIM_INIT);

    /* Clear SOF */
    USBD->INTSTS = USBD_INTSTS_SOFIF_Msk;

    while (1)
    {
        /* Start USB trim if it is not enabled. */
        if ((SYS->HIRCTRIMCTL & SYS_HIRCTRIMCTL_FREQSEL_Msk) != 1)
        {
            /* Start USB trim only when SOF */
            if (USBD->INTSTS & USBD_INTSTS_SOFIF_Msk)
            {
                /* Clear SOF */
                USBD->INTSTS = USBD_INTSTS_SOFIF_Msk;

                /* Re-enable crystal-less */
                SYS->HIRCTRIMCTL = 0x01;
                SYS->HIRCTRIMCTL |= SYS_HIRCTRIMCTL_REFCKSEL_Msk;
            }
        }

        /* Disable USB Trim when error */
        if (SYS->HIRCTRIMSTS & (SYS_HIRCTRIMSTS_CLKERIF_Msk | SYS_HIRCTRIMSTS_TFAILIF_Msk))
        {
            /* Init TRIM */
            M32(TRIM_INIT) = u32TrimInit;

            /* Disable crystal-less */
            SYS->HIRCTRIMCTL = 0;

            /* Clear error flags */
            SYS->HIRCTRIMSTS = SYS_HIRCTRIMSTS_CLKERIF_Msk | SYS_HIRCTRIMSTS_TFAILIF_Msk;

            /* Clear SOF */
            USBD->INTSTS = USBD_INTSTS_SOFIF_Msk;
        }

        if (bUsbDataReady == TRUE)
        {
            ParseCmd((uint8_t *)usb_rcvbuf, EP3_MAX_PKT_SIZE);
            EP2_Handler();
            bUsbDataReady = FALSE;
            IAP_LED_PulseActivity();
        }

        IAP_LED_Task();
    }

_APROM:
    IAP_JumpToApplication();

    /* Trap the CPU */
    while (1);
}
