/***************************************************************************//**
 * @file     isp_user.c
 * @brief    ISP Command source file
 * @version  0x32
 *
 * @note
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @copyright Copyright (C) 2019 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "isp_user.h"

__attribute__((aligned(4))) uint8_t response_buff[64];
__attribute__((aligned(4))) static uint8_t aprom_buf[FMC_FLASH_PAGE_SIZE];
uint32_t g_apromSize, g_dataFlashAddr, g_dataFlashSize;

static uint16_t Checksum(unsigned char *buf, int len)
{
    int i;
    uint16_t c;

    for(c = 0, i = 0 ; i < len; i++)
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

    for(offset = 0; offset < len; offset += FMC_FLASH_PAGE_SIZE)
    {
        chunk = len - offset;

        if(chunk > FMC_FLASH_PAGE_SIZE)
        {
            chunk = FMC_FLASH_PAGE_SIZE;
        }

        ReadData(start + offset, start + offset + chunk, (uint32_t *)aprom_buf);
        lcksum += Checksum(aprom_buf, (int)chunk);
    }

    return lcksum;
}

static void Isp_ResetToBootloaderCheck(void)
{
    SYS_UnlockReg();
    (void)BL_EnableFMC();
    __set_PRIMASK(1);
    FMC_SetVectorPageAddr(APROM_BOOTLOADER_START);
    NVIC_SystemReset();

    while(1);
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
    static uint32_t StartAddress, StartAddress_bak, TotalLen, TotalLen_bak, LastDataLen, g_packno = 1;
    uint8_t *response;
    uint16_t lcksum;
    uint32_t lcmd, srclen, i;
    unsigned char *pSrc;
    static uint32_t gcmd;
    response = response_buff;
    pSrc = buffer;
    srclen = len;
    lcmd = inpw(pSrc);
    outpw(response + 4, 0);
    pSrc += 8;
    srclen -= 8;
    ReadData(Config0, Config0 + 16, (uint32_t *)(response + 8)); //read config

    if(lcmd == CMD_SYNC_PACKNO)
    {
        g_packno = inpw(pSrc);
    }

    if((lcmd) && (lcmd != CMD_RESEND_PACKET))
    {
        gcmd = lcmd;
    }

    if(lcmd == CMD_GET_FWVER)
    {
        response[8] = FW_VERSION;
    }
    else if(lcmd == CMD_GET_DEVICEID)
    {
        outpw(response + 8, SYS->PDID);
        goto out;
    }
    else if(lcmd == CMD_RUN_APROM)
    {
        Isp_ResetToBootloaderCheck();
    }
    else if((lcmd == CMD_RUN_LDROM) || (lcmd == CMD_RESET))
    {
        Isp_ResetToBootloaderCheck();
    }
    else if(lcmd == CMD_CONNECT)
    {
        g_packno = 1;
        outpw(response + 8, g_apromSize);
        outpw(response + 12, g_dataFlashAddr);
        goto out;
    }
    else if(lcmd == CMD_ERASE_ALL)
    {
        EraseAP(APROM_APPLICATION_START, APROM_APPLICATION_SIZE);
    }

    if((lcmd == CMD_UPDATE_APROM) || (lcmd == CMD_UPDATE_DATAFLASH))
    {
        if(lcmd == CMD_UPDATE_DATAFLASH)
        {
            StartAddress = g_dataFlashAddr;

            if(g_dataFlashSize)    //g_dataFlashAddr
            {
                EraseAP(g_dataFlashAddr, g_dataFlashSize);
            }
            else
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
        }

        //StartAddress = inpw(pSrc);
        if(lcmd == CMD_UPDATE_DATAFLASH)
        {
            TotalLen = inpw(pSrc + 4);
        }
        pSrc += 8;
        srclen -= 8;
        StartAddress_bak = StartAddress;
        TotalLen_bak = TotalLen;
    }
    else if(lcmd == CMD_UPDATE_CONFIG)
    {
        UpdateConfig((uint32_t *)(pSrc), (uint32_t *)(response + 8));
        goto out;
    }
    else if(lcmd == CMD_RESEND_PACKET)      //for APROM&Data flash only
    {
        uint32_t PageAddress;
        StartAddress -= LastDataLen;
        TotalLen += LastDataLen;
        PageAddress = StartAddress & (0x100000 - FMC_FLASH_PAGE_SIZE);

        if((PageAddress >= Config0) || (PageAddress < APROM_APPLICATION_START) ||
           ((PageAddress + FMC_FLASH_PAGE_SIZE) > APROM_APPLICATION_END))
        {
            goto out;
        }

        ReadData(PageAddress, StartAddress, (uint32_t *)aprom_buf);
        FMC_Erase_User(PageAddress);
        WriteData(PageAddress, StartAddress, (uint32_t *)aprom_buf);

        if((StartAddress % FMC_FLASH_PAGE_SIZE) >= (FMC_FLASH_PAGE_SIZE - LastDataLen))
        {
            FMC_Erase_User(PageAddress + FMC_FLASH_PAGE_SIZE);
        }

        goto out;
    }

    if((gcmd == CMD_UPDATE_APROM) || (gcmd == CMD_UPDATE_DATAFLASH))
    {
        if(TotalLen < srclen)
        {
            srclen = TotalLen;//prevent last package from over writing
        }

        TotalLen -= srclen;
        if((gcmd == CMD_UPDATE_APROM) &&
           (Isp_IsApplicationRange(StartAddress, srclen) == 0u))
        {
            goto out;
        }
        WriteData(StartAddress, StartAddress + srclen, (uint32_t *)pSrc); //WriteData(StartAddress, StartAddress + srclen, (uint32_t*)pSrc);
        memset(pSrc, 0, srclen);
        ReadData(StartAddress, StartAddress + srclen, (uint32_t *)pSrc);
        StartAddress += srclen;
        LastDataLen =  srclen;

        if(TotalLen == 0u)
        {
            lcksum = CalCheckSum(StartAddress_bak, TotalLen_bak);
            outps(response + 8, lcksum);
        }
    }

out:
    lcksum = Checksum(buffer, len);
    outps(response, lcksum);
    ++g_packno;
    outpw(response + 4, g_packno);
    g_packno++;
    return 0;
}

/*** (C) COPYRIGHT 2019 Nuvoton Technology Corp. ***/
