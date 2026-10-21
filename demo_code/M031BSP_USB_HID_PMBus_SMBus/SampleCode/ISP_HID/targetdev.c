/***************************************************************************//**
 * @file     targetdev.c
 * @brief    ISP support function source file
 * @version  0x32
 * @date     14, June, 2017
 *
 * @note
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2017-2018 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include "targetdev.h"
#include "isp_user.h"

uint32_t GetApromSize()
{
    return APROM_APPLICATION_SIZE;
}

void GetDataFlashInfo(uint32_t *addr, uint32_t *size)
{
    uint32_t uData;
    *size = 0;
    FMC_Read_User(Config0, (unsigned int *)&uData);

    if ((uData & 0x01) == 0)   //DFEN enable
    {
        FMC_Read_User(Config1, (unsigned int *)&uData);
        // filter the reserved bits in CONFIG1
        uData &= 0x000FFFFF;

        if ((uData < APROM_APPLICATION_START) ||
            (uData >= APROM_APPLICATION_END) ||
            (uData & (FMC_FLASH_PAGE_SIZE - 1)))   //avoid config1 value from error
        {
            uData = APROM_APPLICATION_END;
        }

        *addr = uData;
        *size = APROM_APPLICATION_END - uData;
    }
    else
    {
        *addr = APROM_APPLICATION_END;
        *size = 0;
    }
}

