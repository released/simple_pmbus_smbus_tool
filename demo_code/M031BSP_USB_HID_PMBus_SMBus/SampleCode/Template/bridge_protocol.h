#ifndef __BRIDGE_PROTOCOL_H__
#define __BRIDGE_PROTOCOL_H__

#include <stdint.h>

#define BRIDGE_MAGIC                         0xA5u
#define BRIDGE_REPORT_SIZE                   64u
#define BRIDGE_HEADER_SIZE                   6u
#define BRIDGE_MAX_PAYLOAD                   (BRIDGE_REPORT_SIZE - BRIDGE_HEADER_SIZE)

#define BRIDGE_STATUS_OK                     0x00u
#define BRIDGE_STATUS_BAD_MAGIC              0x01u
#define BRIDGE_STATUS_BAD_COMMAND            0x02u
#define BRIDGE_STATUS_BAD_PAYLOAD            0x03u
#define BRIDGE_STATUS_IO_ERROR               0x04u
#define BRIDGE_STATUS_TIMEOUT                0x05u
#define BRIDGE_STATUS_NOT_READY              0x06u
#define BRIDGE_STATUS_BUSY                   0x07u
#define BRIDGE_STATUS_UNSUPPORTED            0x08u

#define BRIDGE_CMD_PING                      0x01u
#define BRIDGE_CMD_GET_INFO                  0x02u
#define BRIDGE_CMD_RESET_MCU                 0x03u

#define BRIDGE_CMD_I2C_INIT_MASTER           0x20u
#define BRIDGE_CMD_I2C_MASTER_WRITE          0x21u
#define BRIDGE_CMD_I2C_MASTER_READ           0x22u
#define BRIDGE_CMD_I2C_INIT_SLAVE            0x23u
#define BRIDGE_CMD_I2C_SLAVE_SET_TX          0x24u
#define BRIDGE_CMD_I2C_SLAVE_GET_RX          0x25u
#define BRIDGE_CMD_I2C_DEINIT                0x26u
#define BRIDGE_CMD_I2C_MASTER_WRITE_READ     0x27u
#define BRIDGE_CMD_I2C_MASTER_STAGE_CLEAR    0x28u
#define BRIDGE_CMD_I2C_MASTER_STAGE_APPEND   0x29u
#define BRIDGE_CMD_I2C_MASTER_EXEC_STAGE_WRITE       0x2Au
#define BRIDGE_CMD_I2C_MASTER_EXEC_STAGE_WRITE_READ  0x2Bu
#define BRIDGE_CMD_I2C_MASTER_STAGE_FETCH_RX         0x2Cu
#define BRIDGE_CMD_I2C_MASTER_PMBUS_BLOCK_READ       0x2Du
#define BRIDGE_CMD_I2C_MASTER_GROUP_WRITE            0x2Eu
#define BRIDGE_CMD_I2C_MASTER_BUS_STATUS             0x2Fu
#define BRIDGE_CMD_I2C_MASTER_SMBUS_QUICK            0x35u

#endif /* __BRIDGE_PROTOCOL_H__ */
