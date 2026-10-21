/***************************************************************************//**
 * @file     isp_user.c
 * @brief    ISP Command source file
 * @version  0x34
 *
 * @note
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2017-2018 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include <stdio.h>
#include "isp_user.h"
#include "fmc_user.h"

#if 0
#define RSTSTS      RSTSRC
#define ISPCTL      ISPCON
#endif

volatile uint8_t bISPDataReady;
#ifdef __ICCARM__
#pragma data_alignment=4
uint8_t response_buff[64];
static uint8_t aprom_buf[FMC_FLASH_PAGE_SIZE];
#else
uint8_t response_buff[64] __attribute__((aligned(4)));
static uint8_t aprom_buf[FMC_FLASH_PAGE_SIZE] __attribute__((aligned(4)));
#endif
uint32_t bUpdateApromCmd;
uint32_t g_apromSize, g_dataFlashAddr, g_dataFlashSize;

__STATIC_INLINE uint16_t Checksum(unsigned char *buf, int len)
{
    int i;
    uint16_t c;

    for (c = 0, i = 0 ; i < len; i++)
    {
        c += buf[i];
    }

    return (c);
}

static uint16_t CalCheckSum(uint32_t start, uint32_t len)
{
    uint32_t offset;
    uint32_t chunk;
    uint16_t lcksum;

    lcksum = 0;

    for (offset = 0UL; offset < len; offset += FMC_FLASH_PAGE_SIZE)
    {
        chunk = len - offset;

        if (chunk > FMC_FLASH_PAGE_SIZE)
        {
            chunk = FMC_FLASH_PAGE_SIZE;
        }

        ReadData(start + offset, start + offset + chunk, (unsigned int *)aprom_buf);
        lcksum += Checksum(aprom_buf, (int)chunk);
    }

    return lcksum;
}

static void Isp_ResetToBootloaderCheck(void)
{
    SYS_UnlockReg();
    FMC->ISPCTL |= FMC_ISPCTL_ISPEN_Msk;
    __set_PRIMASK(1);
    FMC_SetVectorPageAddr(APROM_BOOTLOADER_START);
    SYS->IPRST0 = SYS_IPRST0_CPURST_Msk;

    while (1);
}

static uint8_t Isp_IsApplicationRange(uint32_t start, uint32_t size)
{
    if (size == 0u)
    {
        return 0u;
    }
    if (start < APROM_APPLICATION_START)
    {
        return 0u;
    }
    if (start >= APROM_APPLICATION_END)
    {
        return 0u;
    }
    if (size > (APROM_APPLICATION_END - start))
    {
        return 0u;
    }
    return 1u;
}

static uint32_t Isp_ClampApplicationLength(uint32_t start, uint32_t size)
{
    if (start < APROM_APPLICATION_START)
    {
        start = APROM_APPLICATION_START;
    }
    if (start >= APROM_APPLICATION_END)
    {
        return 0u;
    }
    if (size > (APROM_APPLICATION_END - start))
    {
        return (APROM_APPLICATION_END - start);
    }
    return size;
}

int ParseCmd(unsigned char *buffer, uint8_t len)
{
    static uint32_t StartAddress;
    static uint32_t StartAddress_bak;
    static uint32_t TotalLen;
    static uint32_t TotalLen_bak;
    static uint32_t LastDataLen;
    static uint32_t g_packno = 1;
    static uint32_t gcmd;
    uint8_t *response;
    uint16_t lcksum;
    uint32_t lcmd;
    uint32_t srclen;
    uint32_t regcnf0;
    uint32_t security;
    uint32_t PageAddress;
    unsigned char *pSrc;

    response = response_buff;
    pSrc = buffer;
    srclen = len;
    lcmd = inpw(pSrc);
    outpw(response + 4, 0);
    pSrc += 8;
    srclen -= 8;
    ReadData(Config0, Config0 + 16, (unsigned int *)(response + 8));
    regcnf0 = *(uint32_t *)(response + 8);
    security = regcnf0 & 0x2UL;

    if (lcmd == CMD_SYNC_PACKNO)
    {
        g_packno = inpw(pSrc);
    }

    if ((lcmd) && (lcmd != CMD_RESEND_PACKET))
    {
        gcmd = lcmd;
    }

    if (lcmd == CMD_GET_FWVER)
    {
        response[8] = FW_VERSION;
    }
    else if (lcmd == CMD_GET_DEVICEID)
    {
        outpw(response + 8, SYS->PDID);
        goto out;
    }
    else if ((lcmd == CMD_RUN_APROM) || (lcmd == CMD_RUN_LDROM) || (lcmd == CMD_RESET))
    {
        Isp_ResetToBootloaderCheck();
    }
    else if (lcmd == CMD_CONNECT)
    {
        g_packno = 1;
        outpw(response + 8, g_apromSize);
        outpw(response + 12, g_dataFlashAddr);
        goto out;
    }
    else if (lcmd == CMD_ERASE_ALL)
    {
        EraseAP(APROM_APPLICATION_START, APROM_APPLICATION_SIZE);
        bUpdateApromCmd = TRUE;
    }

    if ((lcmd == CMD_UPDATE_APROM) || (lcmd == CMD_UPDATE_DATAFLASH))
    {
        if (lcmd == CMD_UPDATE_DATAFLASH)
        {
            StartAddress = g_dataFlashAddr;

            if (g_dataFlashSize)
            {
                EraseAP(g_dataFlashAddr, g_dataFlashSize);
            }
            else
            {
                goto out;
            }

            TotalLen = inpw(pSrc + 4);
            TotalLen = Isp_ClampApplicationLength(StartAddress, TotalLen);
            if (Isp_IsApplicationRange(StartAddress, TotalLen) == 0u)
            {
                goto out;
            }
        }
        else
        {
            StartAddress = inpw(pSrc);
            TotalLen = inpw(pSrc + 4);

            if (StartAddress < APROM_APPLICATION_START)
            {
                StartAddress = APROM_APPLICATION_START + StartAddress;
            }

            TotalLen = Isp_ClampApplicationLength(StartAddress, TotalLen);
            if (Isp_IsApplicationRange(StartAddress, TotalLen) == 0u)
            {
                goto out;
            }

            EraseAP(StartAddress, TotalLen);
            bUpdateApromCmd = TRUE;
        }

        pSrc += 8;
        srclen -= 8;
        StartAddress_bak = StartAddress;
        TotalLen_bak = TotalLen;
    }
    else if (lcmd == CMD_UPDATE_CONFIG)
    {
        if ((security == 0u) && (!bUpdateApromCmd))
        {
            goto out;
        }

        UpdateConfig((unsigned int *)(pSrc), (unsigned int *)(response + 8));
        GetDataFlashInfo(&g_dataFlashAddr, &g_dataFlashSize);
        goto out;
    }
    else if (lcmd == CMD_RESEND_PACKET)
    {
        StartAddress -= LastDataLen;
        TotalLen += LastDataLen;
        PageAddress = StartAddress & (0x100000UL - FMC_FLASH_PAGE_SIZE);

        if ((PageAddress >= Config0) || (PageAddress < APROM_APPLICATION_START) ||
            ((PageAddress + FMC_FLASH_PAGE_SIZE) > APROM_APPLICATION_END))
        {
            goto out;
        }

        ReadData(PageAddress, StartAddress, (unsigned int *)aprom_buf);
        FMC_Erase_User(PageAddress);
        WriteData(PageAddress, StartAddress, (unsigned int *)aprom_buf);

        if ((StartAddress % FMC_FLASH_PAGE_SIZE) >= (FMC_FLASH_PAGE_SIZE - LastDataLen))
        {
            FMC_Erase_User(PageAddress + FMC_FLASH_PAGE_SIZE);
        }

        goto out;
    }

    if ((gcmd == CMD_UPDATE_APROM) || (gcmd == CMD_UPDATE_DATAFLASH))
    {
        if (TotalLen < srclen)
        {
            srclen = TotalLen;
        }

        TotalLen -= srclen;
        if (Isp_IsApplicationRange(StartAddress, srclen) == 0u)
        {
            goto out;
        }

        WriteData(StartAddress, StartAddress + srclen, (unsigned int *)pSrc);
        memset(pSrc, 0, srclen);
        ReadData(StartAddress, StartAddress + srclen, (unsigned int *)pSrc);
        StartAddress += srclen;
        LastDataLen =  srclen;

        if (TotalLen == 0u)
        {
            lcksum = CalCheckSum(StartAddress_bak, TotalLen_bak);
            outps(response + 8, lcksum);
        }
    }

out:
    lcksum = Checksum(buffer, len);
    outps(response, lcksum);
    outpw(response + 4, g_packno);
    g_packno++;
    return 0;
}
