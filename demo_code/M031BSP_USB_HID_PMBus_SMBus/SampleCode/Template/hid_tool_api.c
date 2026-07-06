/******************************************************************************
 * @file     hid_tool_api.c
 * @brief    HID bridge dispatcher for PMBus/SMBus host testing.
 *****************************************************************************/

#include <stdio.h>
#include <string.h>

#include "NuMicro.h"
#include "bridge_le.h"
#include "bridge_protocol.h"
#include "bridge_version.h"
#include "hid_transfer.h"
#include "hid_tool_api.h"
#include "m031_bridge_i2c.h"
#include "misc_config.h"

#define BRIDGE_INFO_NAME             "m032-pmbus-smbus-bridge/" M032_BRIDGE_FW_VERSION_STR
#define BRIDGE_RESET_REASON_NONE     0u
#define BRIDGE_RESET_REASON_CMD      2u
#define BRIDGE_RESET_DELAY_LOOPS     20000u

static volatile uint8_t g_u8CmdProcessReady = 0u;
static volatile uint8_t g_u8EP3Ready = 0u;
static volatile uint8_t g_u8ResetRequested = 0u;
static volatile uint32_t g_u32ResetCountdown = 0u;
static uint8_t g_u8LastResetReason = BRIDGE_RESET_REASON_NONE;

static uint8_t hid_buffer_to_pc[EP2_MAX_PKT_SIZE] = {0};
static uint8_t hid_buffer_from_pc[EP3_MAX_PKT_SIZE] = {0};

static uint8_t g_i2c_stage_tx[M031_BRIDGE_I2C_PORT_COUNT][M031_BRIDGE_I2C_STAGE_BUF_SIZE];
static uint8_t g_i2c_stage_rx[M031_BRIDGE_I2C_PORT_COUNT][M031_BRIDGE_I2C_STAGE_BUF_SIZE];
static uint16_t g_i2c_stage_tx_len[M031_BRIDGE_I2C_PORT_COUNT];
static uint16_t g_i2c_stage_rx_len[M031_BRIDGE_I2C_PORT_COUNT];

static void Bridge_BuildResponseHeader(uint8_t *resp, uint8_t cmd, uint8_t seq, uint8_t status, uint16_t payload_len)
{
    reset_buffer(resp, 0x00, EP2_MAX_PKT_SIZE);
    resp[0] = BRIDGE_MAGIC;
    resp[1] = cmd;
    resp[2] = seq;
    resp[3] = status;
    Bridge_WriteU16Le(&resp[4], payload_len);
}

static void Bridge_FinishResponse(uint8_t *resp, uint8_t cmd, uint8_t seq, uint8_t status,
                                  const uint8_t *payload, uint16_t payload_len)
{
    if (payload_len > BRIDGE_MAX_PAYLOAD)
    {
        status = BRIDGE_STATUS_BAD_PAYLOAD;
        payload_len = 0u;
    }

    Bridge_BuildResponseHeader(resp, cmd, seq, status, payload_len);
    if ((payload != 0) && (payload_len > 0u))
    {
        copy_buffer(&resp[BRIDGE_HEADER_SIZE], (void *)payload, payload_len);
    }
}

static uint8_t Bridge_HandleCore(uint8_t cmd, const uint8_t *payload, uint16_t payload_len,
                                 uint8_t *out, uint16_t *out_len, uint8_t *status)
{
    const char *name;
    uint16_t name_len;

    switch (cmd)
    {
        case BRIDGE_CMD_PING:
            if (payload_len > BRIDGE_MAX_PAYLOAD)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            if (payload_len > 0u)
            {
                copy_buffer(out, (void *)payload, payload_len);
            }
            *out_len = payload_len;
            *status = BRIDGE_STATUS_OK;
            return 1u;

        case BRIDGE_CMD_GET_INFO:
            if (payload_len != 0u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            name = BRIDGE_INFO_NAME;
            name_len = (uint16_t)strlen(name);
            if (name_len > (BRIDGE_MAX_PAYLOAD - 2u))
            {
                name_len = BRIDGE_MAX_PAYLOAD - 2u;
            }
            copy_buffer(out, (void *)name, name_len);
            out[name_len] = 0u;
            out[name_len + 1u] = g_u8LastResetReason;
            *out_len = (uint16_t)(name_len + 2u);
            *status = BRIDGE_STATUS_OK;
            return 1u;

        case BRIDGE_CMD_RESET_MCU:
            if (payload_len != 0u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            g_u8LastResetReason = BRIDGE_RESET_REASON_CMD;
            g_u8ResetRequested = 1u;
            *out_len = 0u;
            *status = BRIDGE_STATUS_OK;
            return 1u;

        default:
            break;
    }

    return 0u;
}

static void Bridge_I2cStageReset(uint8_t port)
{
    if (port >= M031_BRIDGE_I2C_PORT_COUNT)
    {
        return;
    }
    g_i2c_stage_tx_len[port] = 0u;
    g_i2c_stage_rx_len[port] = 0u;
}

static uint8_t Bridge_I2cValidateGroupBlob(uint8_t port, uint8_t segment_count)
{
    uint16_t offset;
    uint16_t segment_len;
    uint8_t index;

    if ((port >= M031_BRIDGE_I2C_PORT_COUNT) || (segment_count == 0u))
    {
        return 0u;
    }

    offset = 0u;
    for (index = 0u; index < segment_count; ++index)
    {
        if ((uint16_t)(offset + 3u) > g_i2c_stage_tx_len[port])
        {
            return 0u;
        }
        segment_len = Bridge_ReadU16Le(&g_i2c_stage_tx[port][offset + 1u]);
        if ((segment_len == 0u) ||
            ((uint16_t)(offset + 3u + segment_len) > g_i2c_stage_tx_len[port]))
        {
            return 0u;
        }
        offset = (uint16_t)(offset + 3u + segment_len);
    }

    return (offset == g_i2c_stage_tx_len[port]) ? 1u : 0u;
}

static uint8_t Bridge_I2cFailureStatus(void)
{
    if (M031BridgeI2c_LastError() == M031_BRIDGE_I2C_ERROR_TIMEOUT)
    {
        return BRIDGE_STATUS_TIMEOUT;
    }
    return BRIDGE_STATUS_IO_ERROR;
}

static uint8_t Bridge_HandleI2c(uint8_t cmd, const uint8_t *payload, uint16_t payload_len,
                                uint8_t *out, uint16_t *out_len, uint8_t *status)
{
    uint8_t port;
    uint8_t addr;
    uint8_t len8;
    uint16_t len16;
    uint16_t count16;
    uint16_t written;
    uint16_t read_len;
    uint16_t offset;
    uint16_t remaining;
    uint32_t baudrate;
    uint8_t ok;
    uint8_t completed;
    uint8_t idle_before;
    uint8_t recovered;
    uint8_t idle_after;
    uint8_t ack;

    switch (cmd)
    {
        case BRIDGE_CMD_I2C_INIT_MASTER:
            if (payload_len != 7u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            port = payload[0];
            baudrate = Bridge_ReadU32Le(&payload[3]);
            ok = M031BridgeI2c_InitMaster(port, payload[1], payload[2], baudrate);
            if (ok == 0u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            Bridge_I2cStageReset(port);
            out[0] = port;
            out[1] = 1u;
            *status = BRIDGE_STATUS_OK;
            *out_len = 2u;
            return 1u;

        case BRIDGE_CMD_I2C_INIT_SLAVE:
            if (payload_len != 8u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            port = payload[0];
            addr = (uint8_t)(payload[3] & 0x7Fu);
            baudrate = Bridge_ReadU32Le(&payload[4]);
            ok = M031BridgeI2c_InitSlave(port, payload[1], payload[2], addr, baudrate);
            if (ok == 0u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            Bridge_I2cStageReset(port);
            out[0] = port;
            out[1] = addr;
            out[2] = 1u;
            *status = BRIDGE_STATUS_OK;
            *out_len = 3u;
            return 1u;

        case BRIDGE_CMD_I2C_DEINIT:
            if (payload_len != 1u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            port = payload[0];
            M031BridgeI2c_Deinit(port);
            Bridge_I2cStageReset(port);
            *status = BRIDGE_STATUS_OK;
            *out_len = 0u;
            return 1u;

        case BRIDGE_CMD_I2C_MASTER_WRITE:
            if (payload_len < 3u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            port = payload[0];
            addr = (uint8_t)(payload[1] & 0x7Fu);
            len8 = payload[2];
            if ((uint16_t)(3u + len8) != payload_len)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            if (M031BridgeI2c_IsMasterReady(port) == 0u)
            {
                *status = BRIDGE_STATUS_NOT_READY;
                *out_len = 0u;
                return 1u;
            }
            written = 0u;
            ok = M031BridgeI2c_MasterWrite(port, addr, &payload[3], len8, &written);
            if (ok == 0u)
            {
                *status = Bridge_I2cFailureStatus();
                *out_len = 0u;
                return 1u;
            }
            out[0] = (uint8_t)written;
            *status = BRIDGE_STATUS_OK;
            *out_len = 1u;
            return 1u;

        case BRIDGE_CMD_I2C_SLAVE_SET_TX:
            if (payload_len < 2u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            port = payload[0];
            len8 = payload[1];
            if ((uint16_t)(2u + len8) != payload_len)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            if (M031BridgeI2c_IsSlaveReady(port) == 0u)
            {
                *status = BRIDGE_STATUS_NOT_READY;
                *out_len = 0u;
                return 1u;
            }
            written = 0u;
            ok = M031BridgeI2c_SlaveSetTx(port, &payload[2], len8, &written);
            if (ok == 0u)
            {
                *status = Bridge_I2cFailureStatus();
                *out_len = 0u;
                return 1u;
            }
            out[0] = (uint8_t)written;
            *status = BRIDGE_STATUS_OK;
            *out_len = 1u;
            return 1u;

        case BRIDGE_CMD_I2C_SLAVE_GET_RX:
            if (payload_len != 2u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            port = payload[0];
            len8 = payload[1];
            if (len8 > (BRIDGE_MAX_PAYLOAD - 1u))
            {
                len8 = BRIDGE_MAX_PAYLOAD - 1u;
            }
            if (M031BridgeI2c_IsSlaveReady(port) == 0u)
            {
                *status = BRIDGE_STATUS_NOT_READY;
                *out_len = 0u;
                return 1u;
            }
            read_len = 0u;
            ok = M031BridgeI2c_SlaveGetRx(port, &out[1], len8, &read_len);
            if (ok == 0u)
            {
                *status = Bridge_I2cFailureStatus();
                *out_len = 0u;
                return 1u;
            }
            out[0] = (uint8_t)read_len;
            *status = BRIDGE_STATUS_OK;
            *out_len = (uint16_t)(1u + read_len);
            return 1u;

        case BRIDGE_CMD_I2C_MASTER_READ:
            if (payload_len != 3u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            port = payload[0];
            addr = (uint8_t)(payload[1] & 0x7Fu);
            len8 = payload[2];
            if (len8 > (BRIDGE_MAX_PAYLOAD - 1u))
            {
                len8 = BRIDGE_MAX_PAYLOAD - 1u;
            }
            if (M031BridgeI2c_IsMasterReady(port) == 0u)
            {
                *status = BRIDGE_STATUS_NOT_READY;
                *out_len = 0u;
                return 1u;
            }
            read_len = 0u;
            ok = M031BridgeI2c_MasterRead(port, addr, &out[1], len8, &read_len);
            if (ok == 0u)
            {
                *status = Bridge_I2cFailureStatus();
                *out_len = 0u;
                return 1u;
            }
            out[0] = (uint8_t)read_len;
            *status = BRIDGE_STATUS_OK;
            *out_len = (uint16_t)(1u + read_len);
            return 1u;

        case BRIDGE_CMD_I2C_MASTER_WRITE_READ:
            if (payload_len < 5u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            port = payload[0];
            addr = (uint8_t)(payload[1] & 0x7Fu);
            len8 = payload[3];
            if ((uint16_t)(5u + len8) != payload_len)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            read_len = payload[4];
            if (read_len > (BRIDGE_MAX_PAYLOAD - 1u))
            {
                read_len = BRIDGE_MAX_PAYLOAD - 1u;
            }
            if (M031BridgeI2c_IsMasterReady(port) == 0u)
            {
                *status = BRIDGE_STATUS_NOT_READY;
                *out_len = 0u;
                return 1u;
            }
            count16 = 0u;
            ok = M031BridgeI2c_MasterWriteRead(port, addr, &payload[5], len8, &out[1], read_len, &count16, payload[2]);
            if (ok == 0u)
            {
                *status = Bridge_I2cFailureStatus();
                *out_len = 0u;
                return 1u;
            }
            out[0] = (uint8_t)count16;
            *status = BRIDGE_STATUS_OK;
            *out_len = (uint16_t)(1u + count16);
            return 1u;

        case BRIDGE_CMD_I2C_MASTER_PMBUS_BLOCK_READ:
            if (payload_len < 5u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            port = payload[0];
            addr = (uint8_t)(payload[1] & 0x7Fu);
            len8 = payload[2];
            if ((uint16_t)(5u + len8) != payload_len)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            if (M031BridgeI2c_IsMasterReady(port) == 0u)
            {
                *status = BRIDGE_STATUS_NOT_READY;
                *out_len = 0u;
                return 1u;
            }
            count16 = 0u;
            ok = M031BridgeI2c_PmbusBlockRead(port, addr, &payload[5], len8, payload[3], payload[4], out, &count16);
            if (ok == 0u)
            {
                *status = Bridge_I2cFailureStatus();
                *out_len = 0u;
                return 1u;
            }
            *status = BRIDGE_STATUS_OK;
            *out_len = count16;
            return 1u;

        case BRIDGE_CMD_I2C_MASTER_BUS_STATUS:
            if (payload_len != 2u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            port = payload[0];
            if (M031BridgeI2c_IsMasterReady(port) == 0u)
            {
                *status = BRIDGE_STATUS_NOT_READY;
                *out_len = 0u;
                return 1u;
            }
            idle_before = 0u;
            recovered = 0u;
            idle_after = 0u;
            ok = M031BridgeI2c_BusStatus(port, payload[1], &idle_before, &recovered, &idle_after);
            if (ok == 0u)
            {
                *status = Bridge_I2cFailureStatus();
                *out_len = 0u;
                return 1u;
            }
            out[0] = port;
            out[1] = idle_before;
            out[2] = recovered;
            out[3] = idle_after;
            *status = BRIDGE_STATUS_OK;
            *out_len = 4u;
            return 1u;

        case BRIDGE_CMD_I2C_MASTER_SMBUS_QUICK:
            if (payload_len != 3u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            port = payload[0];
            addr = (uint8_t)(payload[1] & 0x7Fu);
            if (M031BridgeI2c_IsMasterReady(port) == 0u)
            {
                *status = BRIDGE_STATUS_NOT_READY;
                *out_len = 0u;
                return 1u;
            }
            ack = 0u;
            ok = M031BridgeI2c_SmbusQuick(port, addr, payload[2], &ack);
            if (ok == 0u)
            {
                *status = Bridge_I2cFailureStatus();
                *out_len = 0u;
                return 1u;
            }
            out[0] = port;
            out[1] = addr;
            out[2] = (payload[2] != 0u) ? 1u : 0u;
            out[3] = ack;
            *status = BRIDGE_STATUS_OK;
            *out_len = 4u;
            return 1u;

        case BRIDGE_CMD_I2C_MASTER_STAGE_CLEAR:
            if (payload_len != 1u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            port = payload[0];
            if (port >= M031_BRIDGE_I2C_PORT_COUNT)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            Bridge_I2cStageReset(port);
            *status = BRIDGE_STATUS_OK;
            *out_len = 0u;
            return 1u;

        case BRIDGE_CMD_I2C_MASTER_STAGE_APPEND:
            if (payload_len < 2u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            port = payload[0];
            len8 = payload[1];
            if ((port >= M031_BRIDGE_I2C_PORT_COUNT) || ((uint16_t)(2u + len8) != payload_len))
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            remaining = (uint16_t)(M031_BRIDGE_I2C_STAGE_BUF_SIZE - g_i2c_stage_tx_len[port]);
            if (len8 > remaining)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            copy_buffer(&g_i2c_stage_tx[port][g_i2c_stage_tx_len[port]], (void *)&payload[2], len8);
            g_i2c_stage_tx_len[port] = (uint16_t)(g_i2c_stage_tx_len[port] + len8);
            Bridge_WriteU16Le(out, g_i2c_stage_tx_len[port]);
            *status = BRIDGE_STATUS_OK;
            *out_len = 2u;
            return 1u;

        case BRIDGE_CMD_I2C_MASTER_EXEC_STAGE_WRITE:
        case BRIDGE_CMD_I2C_MASTER_EXEC_STAGE_WRITE_READ:
            if (((cmd == BRIDGE_CMD_I2C_MASTER_EXEC_STAGE_WRITE) && (payload_len != 2u)) ||
                ((cmd == BRIDGE_CMD_I2C_MASTER_EXEC_STAGE_WRITE_READ) && (payload_len != 5u)))
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            port = payload[0];
            addr = (uint8_t)(payload[1] & 0x7Fu);
            if ((port >= M031_BRIDGE_I2C_PORT_COUNT) || (M031BridgeI2c_IsMasterReady(port) == 0u))
            {
                *status = BRIDGE_STATUS_NOT_READY;
                *out_len = 0u;
                return 1u;
            }
            len16 = 0u;
            if (cmd == BRIDGE_CMD_I2C_MASTER_EXEC_STAGE_WRITE_READ)
            {
                len16 = Bridge_ReadU16Le(&payload[3]);
                if (len16 > M031_BRIDGE_I2C_STAGE_BUF_SIZE)
                {
                    *status = BRIDGE_STATUS_BAD_PAYLOAD;
                    *out_len = 0u;
                    return 1u;
                }
            }
            g_i2c_stage_rx_len[port] = 0u;
            count16 = 0u;
            if (cmd == BRIDGE_CMD_I2C_MASTER_EXEC_STAGE_WRITE)
            {
                ok = M031BridgeI2c_MasterWrite(port, addr, g_i2c_stage_tx[port],
                                               g_i2c_stage_tx_len[port], &written);
                count16 = written;
            }
            else
            {
                ok = M031BridgeI2c_MasterWriteRead(port, addr, g_i2c_stage_tx[port], g_i2c_stage_tx_len[port],
                                                   g_i2c_stage_rx[port], len16, &count16, payload[2]);
                g_i2c_stage_rx_len[port] = count16;
            }
            if (ok == 0u)
            {
                *status = Bridge_I2cFailureStatus();
                *out_len = 0u;
                return 1u;
            }
            Bridge_WriteU16Le(out, count16);
            *status = BRIDGE_STATUS_OK;
            *out_len = 2u;
            return 1u;

        case BRIDGE_CMD_I2C_MASTER_STAGE_FETCH_RX:
            if (payload_len != 4u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            port = payload[0];
            offset = Bridge_ReadU16Le(&payload[1]);
            len8 = payload[3];
            if ((port >= M031_BRIDGE_I2C_PORT_COUNT) || (offset > g_i2c_stage_rx_len[port]))
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            if (len8 > (BRIDGE_MAX_PAYLOAD - 3u))
            {
                len8 = BRIDGE_MAX_PAYLOAD - 3u;
            }
            count16 = (uint16_t)(g_i2c_stage_rx_len[port] - offset);
            if (count16 > len8)
            {
                count16 = len8;
            }
            Bridge_WriteU16Le(&out[0], g_i2c_stage_rx_len[port]);
            out[2] = (uint8_t)count16;
            if (count16 > 0u)
            {
                copy_buffer(&out[3], &g_i2c_stage_rx[port][offset], count16);
            }
            *status = BRIDGE_STATUS_OK;
            *out_len = (uint16_t)(3u + count16);
            return 1u;

        case BRIDGE_CMD_I2C_MASTER_GROUP_WRITE:
            if (payload_len != 2u)
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            port = payload[0];
            completed = 0u;
            if ((M031BridgeI2c_IsMasterReady(port) == 0u) ||
                (Bridge_I2cValidateGroupBlob(port, payload[1]) == 0u))
            {
                *status = BRIDGE_STATUS_BAD_PAYLOAD;
                *out_len = 0u;
                return 1u;
            }
            ok = M031BridgeI2c_GroupWrite(port, g_i2c_stage_tx[port], g_i2c_stage_tx_len[port],
                                          payload[1], &completed);
            if (ok == 0u)
            {
                *status = Bridge_I2cFailureStatus();
                *out_len = 0u;
                return 1u;
            }
            out[0] = completed;
            *status = BRIDGE_STATUS_OK;
            *out_len = 1u;
            return 1u;

        default:
            break;
    }

    return 0u;
}

static void Bridge_ProcessCommand(const uint8_t *req, uint8_t *resp)
{
    uint8_t cmd;
    uint8_t seq;
    uint16_t payload_len;
    const uint8_t *payload;
    uint8_t temp[BRIDGE_MAX_PAYLOAD];
    uint16_t out_len;
    uint8_t status;
    uint8_t handled;

    cmd = req[1];
    seq = req[2];
    payload_len = Bridge_ReadU16Le(&req[4]);

    if (req[0] != BRIDGE_MAGIC)
    {
        Bridge_FinishResponse(resp, cmd, seq, BRIDGE_STATUS_BAD_MAGIC, 0, 0u);
        return;
    }
    if (payload_len > BRIDGE_MAX_PAYLOAD)
    {
        Bridge_FinishResponse(resp, cmd, seq, BRIDGE_STATUS_BAD_PAYLOAD, 0, 0u);
        return;
    }

    payload = &req[BRIDGE_HEADER_SIZE];
    reset_buffer(temp, 0x00, sizeof(temp));
    out_len = 0u;
    status = BRIDGE_STATUS_OK;
    handled = 0u;

    if (handled == 0u)
    {
        handled = Bridge_HandleCore(cmd, payload, payload_len, temp, &out_len, &status);
    }
    if (handled == 0u)
    {
        handled = Bridge_HandleI2c(cmd, payload, payload_len, temp, &out_len, &status);
    }

    if (handled == 0u)
    {
        status = BRIDGE_STATUS_BAD_COMMAND;
        out_len = 0u;
    }

    Bridge_FinishResponse(resp, cmd, seq, status, temp, out_len);
}

void HidTool_ResetState(void)
{
    g_u8CmdProcessReady = 0u;
    g_u8EP3Ready = 0u;
    g_u8ResetRequested = 0u;
    g_u32ResetCountdown = 0u;
    reset_buffer(hid_buffer_to_pc, 0x00, sizeof(hid_buffer_to_pc));
    reset_buffer(hid_buffer_from_pc, 0x00, sizeof(hid_buffer_from_pc));
}

void HidTool_OnOutReady(void)
{
    g_u8EP3Ready = 1u;
}

void HidTool_GetOutReport(uint8_t *pu8EpBuf, uint32_t u32Size)
{
    uint32_t copy_len;

    copy_len = u32Size;
    if (copy_len > EP3_MAX_PKT_SIZE)
    {
        copy_len = EP3_MAX_PKT_SIZE;
    }

    reset_buffer(hid_buffer_from_pc, 0x00, sizeof(hid_buffer_from_pc));
    USBD_MemCopy((uint8_t *)&hid_buffer_from_pc, pu8EpBuf, copy_len);

    reset_buffer(hid_buffer_to_pc, 0x00, sizeof(hid_buffer_to_pc));
    Bridge_ProcessCommand(hid_buffer_from_pc, hid_buffer_to_pc);
    g_u8CmdProcessReady = 1u;
}

void HidTool_SetInReport(void)
{
    if (g_u8CmdProcessReady == 0u)
    {
        return;
    }

    USBD_MemCopy((uint8_t *)(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP2)),
                 (void *)&hid_buffer_to_pc,
                 EP2_MAX_PKT_SIZE);
    USBD_SET_PAYLOAD_LEN(EP2, EP2_MAX_PKT_SIZE);

    g_u8CmdProcessReady = 0u;
    reset_buffer(hid_buffer_to_pc, 0x00, sizeof(hid_buffer_to_pc));

    if (g_u8ResetRequested != 0u)
    {
        g_u32ResetCountdown = BRIDGE_RESET_DELAY_LOOPS;
    }
}

void HidTool_Process(void)
{
    uint8_t *ptr;

    if (g_u8EP3Ready != 0u)
    {
        g_u8EP3Ready = 0u;
        g_u8CmdProcessReady = 0u;

        ptr = (uint8_t *)(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP3));
        HidTool_GetOutReport(ptr, USBD_GET_PAYLOAD_LEN(EP3));
        USBD_SET_PAYLOAD_LEN(EP3, EP3_MAX_PKT_SIZE);
    }

    HidTool_SetInReport();

    if (g_u32ResetCountdown > 0u)
    {
        --g_u32ResetCountdown;
        if (g_u32ResetCountdown == 0u)
        {
            SYS_UnlockReg();
            SYS_ResetChip();
        }
    }
}
