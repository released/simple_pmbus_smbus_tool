#ifndef __M032_IAP_MAILBOX_H__
#define __M032_IAP_MAILBOX_H__

#include <stdint.h>

#define M032_IAP_SRAM_START             0x20000000UL
#define M032_IAP_SRAM_TOTAL_SIZE        0x00004000UL
#define M032_IAP_MAILBOX_SIZE           0x00000040UL
#define M032_IAP_SRAM_APP_SIZE          (M032_IAP_SRAM_TOTAL_SIZE - M032_IAP_MAILBOX_SIZE)
#define M032_IAP_MAILBOX_ADDR           (M032_IAP_SRAM_START + M032_IAP_SRAM_APP_SIZE)
#define M032_IAP_SRAM_APP_END           M032_IAP_MAILBOX_ADDR
#define M032_IAP_SRAM_PHYSICAL_END      (M032_IAP_SRAM_START + M032_IAP_SRAM_TOTAL_SIZE)

#define M032_IAP_MAILBOX_MAGIC          0x4D303332UL
#define M032_IAP_MAILBOX_MAGIC_INV      (~M032_IAP_MAILBOX_MAGIC)
#define M032_IAP_MAILBOX_VERSION        0x00000001UL
#define M032_IAP_MAILBOX_CMD_ENTER_IAP  0x45554E54UL
#define M032_IAP_MAILBOX_CHECK_XOR      0xA5A55A5AUL

typedef struct
{
    uint32_t magic;
    uint32_t magic_inv;
    uint32_t version;
    uint32_t command;
    uint32_t argument0;
    uint32_t argument1;
    uint32_t checksum;
    uint32_t reserved[9];
} M032_IAP_MAILBOX_T;

#endif /* __M032_IAP_MAILBOX_H__ */
