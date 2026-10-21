#ifndef __M261_BRIDGE_I2C_H__
#define __M261_BRIDGE_I2C_H__

#include <stdint.h>

#define M261_BRIDGE_I2C_PORT_COUNT      2u
#define M261_BRIDGE_I2C_STAGE_BUF_SIZE  512u
#define M261_BRIDGE_I2C_SLAVE_BUF_SIZE  256u
#define M261_BRIDGE_I2C_ERROR_NONE      0u
#define M261_BRIDGE_I2C_ERROR_IO        1u
#define M261_BRIDGE_I2C_ERROR_TIMEOUT   2u

uint8_t M261BridgeI2c_InitMaster(uint8_t port, uint8_t sda_pin, uint8_t scl_pin, uint32_t baudrate);
uint8_t M261BridgeI2c_InitSlave(uint8_t port, uint8_t sda_pin, uint8_t scl_pin, uint8_t addr7, uint32_t baudrate);
void M261BridgeI2c_Deinit(uint8_t port);
uint8_t M261BridgeI2c_IsMasterReady(uint8_t port);
uint8_t M261BridgeI2c_IsSlaveReady(uint8_t port);
uint8_t M261BridgeI2c_LastError(void);
uint8_t M261BridgeI2c_MasterWrite(uint8_t port, uint8_t addr7, const uint8_t *data, uint16_t len, uint16_t *written);
uint8_t M261BridgeI2c_MasterRead(uint8_t port, uint8_t addr7, uint8_t *out, uint16_t len, uint16_t *read_len);
uint8_t M261BridgeI2c_MasterWriteRead(uint8_t port, uint8_t addr7, const uint8_t *tx, uint16_t tx_len,
                                      uint8_t *rx, uint16_t rx_len, uint16_t *read_len, uint8_t repeated_start);
uint8_t M261BridgeI2c_PmbusBlockRead(uint8_t port, uint8_t addr7, const uint8_t *tx, uint16_t tx_len,
                                     uint8_t expect_pec, uint8_t max_block_len, uint8_t *out, uint16_t *out_len);
uint8_t M261BridgeI2c_GroupWrite(uint8_t port, const uint8_t *segment_blob, uint16_t blob_len,
                                 uint8_t segment_count, uint8_t *completed_segments);
uint8_t M261BridgeI2c_BusStatus(uint8_t port, uint8_t recover_if_needed,
                                uint8_t *idle_before, uint8_t *recovered, uint8_t *idle_after);
uint8_t M261BridgeI2c_SmbusQuick(uint8_t port, uint8_t addr7, uint8_t read_bit, uint8_t *ack);
uint8_t M261BridgeI2c_SlaveSetTx(uint8_t port, const uint8_t *data, uint16_t len, uint16_t *written);
uint8_t M261BridgeI2c_SlaveGetRx(uint8_t port, uint8_t *out, uint16_t max_len, uint16_t *read_len);
void M261BridgeI2c_IrqHandler(uint8_t port);

#endif /* __M261_BRIDGE_I2C_H__ */
