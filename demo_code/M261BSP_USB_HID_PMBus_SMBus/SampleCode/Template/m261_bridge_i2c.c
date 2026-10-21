#include <string.h>

#include "NuMicro.h"
#include "bridge_le.h"
#include "m261_bridge_i2c.h"

#define M261_PIN_A(pin)        ((uint8_t)(0x00u | ((pin) & 0x0Fu)))
#define M261_PIN_B(pin)        ((uint8_t)(0x10u | ((pin) & 0x0Fu)))
#define M261_PIN_C(pin)        ((uint8_t)(0x20u | ((pin) & 0x0Fu)))
#define M261_PIN_F(pin)        ((uint8_t)(0x50u | ((pin) & 0x0Fu)))

#define M261_PIN_PORT(pin)     ((uint8_t)(((pin) >> 4) & 0x0Fu))
#define M261_PIN_INDEX(pin)    ((uint8_t)((pin) & 0x0Fu))

#define M261_BRIDGE_I2C_ROLE_NONE      0u
#define M261_BRIDGE_I2C_ROLE_MASTER    1u
#define M261_BRIDGE_I2C_ROLE_SLAVE     2u

typedef struct
{
    uint8_t initialized;
    uint8_t role;
    uint8_t sda_pin;
    uint8_t scl_pin;
    uint8_t addr7;
    uint32_t baudrate;
} M261_BRIDGE_I2C_STATE_T;

static M261_BRIDGE_I2C_STATE_T g_i2c_state[M261_BRIDGE_I2C_PORT_COUNT];
static uint8_t g_i2c_last_error = M261_BRIDGE_I2C_ERROR_NONE;
static volatile uint8_t g_i2c_slave_rx[M261_BRIDGE_I2C_PORT_COUNT][M261_BRIDGE_I2C_SLAVE_BUF_SIZE];
static volatile uint8_t g_i2c_slave_tx[M261_BRIDGE_I2C_PORT_COUNT][M261_BRIDGE_I2C_SLAVE_BUF_SIZE];
static volatile uint16_t g_i2c_slave_rx_len[M261_BRIDGE_I2C_PORT_COUNT];
static volatile uint16_t g_i2c_slave_tx_len[M261_BRIDGE_I2C_PORT_COUNT];
static volatile uint16_t g_i2c_slave_tx_index[M261_BRIDGE_I2C_PORT_COUNT];

static void BridgeI2c_ClearLastError(void)
{
    g_i2c_last_error = M261_BRIDGE_I2C_ERROR_NONE;
}

static void BridgeI2c_MarkIoError(void)
{
    if (g_i2c_last_error == M261_BRIDGE_I2C_ERROR_NONE)
    {
        g_i2c_last_error = M261_BRIDGE_I2C_ERROR_IO;
    }
}

static void BridgeI2c_MarkTimeout(void)
{
    g_i2c_last_error = M261_BRIDGE_I2C_ERROR_TIMEOUT;
}

static I2C_T *BridgeI2c_GetInst(uint8_t port)
{
    if (port == 0u)
    {
        return I2C0;
    }
    if (port == 1u)
    {
        return I2C1;
    }
    return 0;
}

static IRQn_Type BridgeI2c_GetIrq(uint8_t port)
{
    return (port == 0u) ? I2C0_IRQn : I2C1_IRQn;
}

static void BridgeI2c_ClearSlaveBuffers(uint8_t port)
{
    if (port >= M261_BRIDGE_I2C_PORT_COUNT)
    {
        return;
    }
    g_i2c_slave_rx_len[port] = 0u;
    g_i2c_slave_tx_len[port] = 0u;
    g_i2c_slave_tx_index[port] = 0u;
}

static void BridgeI2c_SlavePushRx(uint8_t port, uint8_t data)
{
    uint16_t index;

    if (port >= M261_BRIDGE_I2C_PORT_COUNT)
    {
        return;
    }
    index = g_i2c_slave_rx_len[port];
    if (index < M261_BRIDGE_I2C_SLAVE_BUF_SIZE)
    {
        g_i2c_slave_rx[port][index] = data;
        g_i2c_slave_rx_len[port] = (uint16_t)(index + 1u);
    }
}

static uint8_t BridgeI2c_SlaveNextTx(uint8_t port)
{
    uint16_t index;
    uint8_t data;

    data = 0xFFu;
    if (port >= M261_BRIDGE_I2C_PORT_COUNT)
    {
        return data;
    }
    index = g_i2c_slave_tx_index[port];
    if (index < g_i2c_slave_tx_len[port])
    {
        data = g_i2c_slave_tx[port][index];
        g_i2c_slave_tx_index[port] = (uint16_t)(index + 1u);
    }
    return data;
}

static GPIO_T *BridgeI2c_GetGpioPort(uint8_t pin_code)
{
    switch (M261_PIN_PORT(pin_code))
    {
        case 0u:
            return PA;
        case 1u:
            return PB;
        case 2u:
            return PC;
        case 5u:
            return PF;
        default:
            break;
    }
    return 0;
}

static uint32_t BridgeI2c_PinMask(uint8_t pin_code)
{
    uint8_t index;

    index = M261_PIN_INDEX(pin_code);
    if (index >= 16u)
    {
        return 0u;
    }
    return (uint32_t)(1ul << index);
}

static void BridgeI2c_SetPinOpenDrain(uint8_t pin_code)
{
    GPIO_T *gpio;
    uint32_t mask;

    gpio = BridgeI2c_GetGpioPort(pin_code);
    mask = BridgeI2c_PinMask(pin_code);
    if ((gpio != 0) && (mask != 0u))
    {
        GPIO_SetMode(gpio, mask, GPIO_MODE_OPEN_DRAIN);
    }
}

static void BridgeI2c_SetPinInput(uint8_t pin_code)
{
    GPIO_T *gpio;
    uint32_t mask;

    gpio = BridgeI2c_GetGpioPort(pin_code);
    mask = BridgeI2c_PinMask(pin_code);
    if ((gpio != 0) && (mask != 0u))
    {
        GPIO_SetMode(gpio, mask, GPIO_MODE_INPUT);
    }
}

static uint8_t BridgeI2c_ReadPin(uint8_t pin_code)
{
    GPIO_T *gpio;
    uint32_t mask;

    gpio = BridgeI2c_GetGpioPort(pin_code);
    mask = BridgeI2c_PinMask(pin_code);
    if ((gpio == 0) || (mask == 0u))
    {
        return 0u;
    }
    return ((gpio->PIN & mask) != 0u) ? 1u : 0u;
}

static void BridgeI2c_ReleasePin(uint8_t pin_code)
{
    GPIO_T *gpio;
    uint32_t mask;

    gpio = BridgeI2c_GetGpioPort(pin_code);
    mask = BridgeI2c_PinMask(pin_code);
    if ((gpio != 0) && (mask != 0u))
    {
        gpio->DOUT |= mask;
        GPIO_SetMode(gpio, mask, GPIO_MODE_OPEN_DRAIN);
    }
}

static void BridgeI2c_DrivePinLow(uint8_t pin_code)
{
    GPIO_T *gpio;
    uint32_t mask;

    gpio = BridgeI2c_GetGpioPort(pin_code);
    mask = BridgeI2c_PinMask(pin_code);
    if ((gpio != 0) && (mask != 0u))
    {
        gpio->DOUT &= ~mask;
        GPIO_SetMode(gpio, mask, GPIO_MODE_OPEN_DRAIN);
    }
}

static void BridgeI2c_DelayShort(void)
{
    volatile uint32_t index;

    for (index = 0u; index < 240u; ++index)
    {
    }
}

static uint8_t BridgeI2c_IsValidPair(uint8_t port, uint8_t sda_pin, uint8_t scl_pin)
{
    if (port == 0u)
    {
        if ((sda_pin == M261_PIN_B(4u)) && (scl_pin == M261_PIN_B(5u))) { return 1u; }
        if ((sda_pin == M261_PIN_F(2u)) && (scl_pin == M261_PIN_F(3u))) { return 1u; }
        if ((sda_pin == M261_PIN_A(4u)) && (scl_pin == M261_PIN_A(5u))) { return 1u; }
        if ((sda_pin == M261_PIN_C(0u)) && (scl_pin == M261_PIN_C(1u))) { return 1u; }
    }
    else if (port == 1u)
    {
        if ((sda_pin == M261_PIN_B(0u)) && (scl_pin == M261_PIN_B(1u))) { return 1u; }
        if ((sda_pin == M261_PIN_A(6u)) && (scl_pin == M261_PIN_A(7u))) { return 1u; }
        if ((sda_pin == M261_PIN_A(2u)) && (scl_pin == M261_PIN_A(3u))) { return 1u; }
        if ((sda_pin == M261_PIN_F(1u)) && (scl_pin == M261_PIN_F(0u))) { return 1u; }
        if ((sda_pin == M261_PIN_C(4u)) && (scl_pin == M261_PIN_C(5u))) { return 1u; }
        if ((sda_pin == M261_PIN_B(10u)) && (scl_pin == M261_PIN_B(11u))) { return 1u; }
    }
    return 0u;
}

static uint8_t BridgeI2c_ApplyMux(uint8_t port, uint8_t sda_pin, uint8_t scl_pin)
{
    if (BridgeI2c_IsValidPair(port, sda_pin, scl_pin) == 0u)
    {
        return 0u;
    }

    SYS_UnlockReg();

    if (port == 0u)
    {
        if ((sda_pin == M261_PIN_B(4u)) && (scl_pin == M261_PIN_B(5u)))
        {
            SYS->GPB_MFPL = (SYS->GPB_MFPL & ~(SYS_GPB_MFPL_PB4MFP_Msk | SYS_GPB_MFPL_PB5MFP_Msk)) |
                            (SYS_GPB_MFPL_PB4MFP_I2C0_SDA | SYS_GPB_MFPL_PB5MFP_I2C0_SCL);
        }
        else if ((sda_pin == M261_PIN_F(2u)) && (scl_pin == M261_PIN_F(3u)))
        {
            SYS->GPF_MFPL = (SYS->GPF_MFPL & ~(SYS_GPF_MFPL_PF2MFP_Msk | SYS_GPF_MFPL_PF3MFP_Msk)) |
                            (SYS_GPF_MFPL_PF2MFP_I2C0_SDA | SYS_GPF_MFPL_PF3MFP_I2C0_SCL);
        }
        else if ((sda_pin == M261_PIN_A(4u)) && (scl_pin == M261_PIN_A(5u)))
        {
            SYS->GPA_MFPL = (SYS->GPA_MFPL & ~(SYS_GPA_MFPL_PA4MFP_Msk | SYS_GPA_MFPL_PA5MFP_Msk)) |
                            (SYS_GPA_MFPL_PA4MFP_I2C0_SDA | SYS_GPA_MFPL_PA5MFP_I2C0_SCL);
        }
        else if ((sda_pin == M261_PIN_C(0u)) && (scl_pin == M261_PIN_C(1u)))
        {
            SYS->GPC_MFPL = (SYS->GPC_MFPL & ~(SYS_GPC_MFPL_PC0MFP_Msk | SYS_GPC_MFPL_PC1MFP_Msk)) |
                            (SYS_GPC_MFPL_PC0MFP_I2C0_SDA | SYS_GPC_MFPL_PC1MFP_I2C0_SCL);
        }
    }
    else
    {
        if ((sda_pin == M261_PIN_B(0u)) && (scl_pin == M261_PIN_B(1u)))
        {
            SYS->GPB_MFPL = (SYS->GPB_MFPL & ~(SYS_GPB_MFPL_PB0MFP_Msk | SYS_GPB_MFPL_PB1MFP_Msk)) |
                            (SYS_GPB_MFPL_PB0MFP_I2C1_SDA | SYS_GPB_MFPL_PB1MFP_I2C1_SCL);
        }
        else if ((sda_pin == M261_PIN_A(6u)) && (scl_pin == M261_PIN_A(7u)))
        {
            SYS->GPA_MFPL = (SYS->GPA_MFPL & ~(SYS_GPA_MFPL_PA6MFP_Msk | SYS_GPA_MFPL_PA7MFP_Msk)) |
                            (SYS_GPA_MFPL_PA6MFP_I2C1_SDA | SYS_GPA_MFPL_PA7MFP_I2C1_SCL);
        }
        else if ((sda_pin == M261_PIN_A(2u)) && (scl_pin == M261_PIN_A(3u)))
        {
            SYS->GPA_MFPL = (SYS->GPA_MFPL & ~(SYS_GPA_MFPL_PA2MFP_Msk | SYS_GPA_MFPL_PA3MFP_Msk)) |
                            (SYS_GPA_MFPL_PA2MFP_I2C1_SDA | SYS_GPA_MFPL_PA3MFP_I2C1_SCL);
        }
        else if ((sda_pin == M261_PIN_F(1u)) && (scl_pin == M261_PIN_F(0u)))
        {
            SYS->GPF_MFPL = (SYS->GPF_MFPL & ~(SYS_GPF_MFPL_PF1MFP_Msk | SYS_GPF_MFPL_PF0MFP_Msk)) |
                            (SYS_GPF_MFPL_PF1MFP_I2C1_SDA | SYS_GPF_MFPL_PF0MFP_I2C1_SCL);
        }
        else if ((sda_pin == M261_PIN_C(4u)) && (scl_pin == M261_PIN_C(5u)))
        {
            SYS->GPC_MFPL = (SYS->GPC_MFPL & ~(SYS_GPC_MFPL_PC4MFP_Msk | SYS_GPC_MFPL_PC5MFP_Msk)) |
                            (SYS_GPC_MFPL_PC4MFP_I2C1_SDA | SYS_GPC_MFPL_PC5MFP_I2C1_SCL);
        }
        else if ((sda_pin == M261_PIN_B(10u)) && (scl_pin == M261_PIN_B(11u)))
        {
            SYS->GPB_MFPH = (SYS->GPB_MFPH & ~(SYS_GPB_MFPH_PB10MFP_Msk | SYS_GPB_MFPH_PB11MFP_Msk)) |
                            (SYS_GPB_MFPH_PB10MFP_I2C1_SDA | SYS_GPB_MFPH_PB11MFP_I2C1_SCL);
        }
    }

    SYS_LockReg();

    BridgeI2c_SetPinOpenDrain(sda_pin);
    BridgeI2c_SetPinOpenDrain(scl_pin);
    BridgeI2c_ReleasePin(sda_pin);
    BridgeI2c_ReleasePin(scl_pin);

    return 1u;
}

static void BridgeI2c_ClearMuxToGpio(uint8_t port, uint8_t sda_pin, uint8_t scl_pin)
{
    SYS_UnlockReg();

    if (port == 0u)
    {
        if ((sda_pin == M261_PIN_B(4u)) && (scl_pin == M261_PIN_B(5u)))
        {
            SYS->GPB_MFPL &= ~(SYS_GPB_MFPL_PB4MFP_Msk | SYS_GPB_MFPL_PB5MFP_Msk);
        }
        else if ((sda_pin == M261_PIN_F(2u)) && (scl_pin == M261_PIN_F(3u)))
        {
            SYS->GPF_MFPL &= ~(SYS_GPF_MFPL_PF2MFP_Msk | SYS_GPF_MFPL_PF3MFP_Msk);
        }
        else if ((sda_pin == M261_PIN_A(4u)) && (scl_pin == M261_PIN_A(5u)))
        {
            SYS->GPA_MFPL &= ~(SYS_GPA_MFPL_PA4MFP_Msk | SYS_GPA_MFPL_PA5MFP_Msk);
        }
        else if ((sda_pin == M261_PIN_C(0u)) && (scl_pin == M261_PIN_C(1u)))
        {
            SYS->GPC_MFPL &= ~(SYS_GPC_MFPL_PC0MFP_Msk | SYS_GPC_MFPL_PC1MFP_Msk);
        }
    }
    else if (port == 1u)
    {
        if ((sda_pin == M261_PIN_B(0u)) && (scl_pin == M261_PIN_B(1u)))
        {
            SYS->GPB_MFPL &= ~(SYS_GPB_MFPL_PB0MFP_Msk | SYS_GPB_MFPL_PB1MFP_Msk);
        }
        else if ((sda_pin == M261_PIN_A(6u)) && (scl_pin == M261_PIN_A(7u)))
        {
            SYS->GPA_MFPL &= ~(SYS_GPA_MFPL_PA6MFP_Msk | SYS_GPA_MFPL_PA7MFP_Msk);
        }
        else if ((sda_pin == M261_PIN_A(2u)) && (scl_pin == M261_PIN_A(3u)))
        {
            SYS->GPA_MFPL &= ~(SYS_GPA_MFPL_PA2MFP_Msk | SYS_GPA_MFPL_PA3MFP_Msk);
        }
        else if ((sda_pin == M261_PIN_F(1u)) && (scl_pin == M261_PIN_F(0u)))
        {
            SYS->GPF_MFPL &= ~(SYS_GPF_MFPL_PF1MFP_Msk | SYS_GPF_MFPL_PF0MFP_Msk);
        }
        else if ((sda_pin == M261_PIN_C(4u)) && (scl_pin == M261_PIN_C(5u)))
        {
            SYS->GPC_MFPL &= ~(SYS_GPC_MFPL_PC4MFP_Msk | SYS_GPC_MFPL_PC5MFP_Msk);
        }
        else if ((sda_pin == M261_PIN_B(10u)) && (scl_pin == M261_PIN_B(11u)))
        {
            SYS->GPB_MFPH &= ~(SYS_GPB_MFPH_PB10MFP_Msk | SYS_GPB_MFPH_PB11MFP_Msk);
        }
    }

    SYS_LockReg();

    BridgeI2c_SetPinInput(sda_pin);
    BridgeI2c_SetPinInput(scl_pin);
}

static void BridgeI2c_EnableClock(uint8_t port)
{
    SYS_UnlockReg();
    if (port == 0u)
    {
        CLK_EnableModuleClock(I2C0_MODULE);
        SYS_ResetModule(I2C0_RST);
    }
    else
    {
        CLK_EnableModuleClock(I2C1_MODULE);
        SYS_ResetModule(I2C1_RST);
    }
    SYS_LockReg();
}

static void BridgeI2c_DisableClock(uint8_t port)
{
    SYS_UnlockReg();
    if (port == 0u)
    {
        CLK_DisableModuleClock(I2C0_MODULE);
    }
    else
    {
        CLK_DisableModuleClock(I2C1_MODULE);
    }
    SYS_LockReg();
}

static uint8_t BridgeI2c_WaitReady(I2C_T *i2c)
{
    uint32_t timeout;

    timeout = SystemCoreClock;
    while ((i2c->CTL0 & I2C_CTL0_SI_Msk) == 0u)
    {
        if (timeout == 0u)
        {
            BridgeI2c_MarkTimeout();
            return 0u;
        }
        --timeout;
    }
    return 1u;
}

static uint8_t BridgeI2c_WaitStopClear(I2C_T *i2c)
{
    uint32_t timeout;

    timeout = SystemCoreClock;
    while ((i2c->CTL0 & I2C_CTL0_STO_Msk) != 0u)
    {
        if (timeout == 0u)
        {
            BridgeI2c_MarkTimeout();
            return 0u;
        }
        --timeout;
    }
    return 1u;
}

static void BridgeI2c_DelayBit(void)
{
    uint8_t count;

    for (count = 0u; count < 2u; ++count)
    {
        BridgeI2c_DelayShort();
    }
}

static uint8_t BridgeI2c_WaitPinHigh(uint8_t pin_code)
{
    uint32_t timeout;

    timeout = SystemCoreClock / 1000u;
    if (timeout < 1000u)
    {
        timeout = 1000u;
    }
    while (BridgeI2c_ReadPin(pin_code) == 0u)
    {
        if (timeout == 0u)
        {
            return 0u;
        }
        --timeout;
    }
    return 1u;
}

static uint8_t BridgeI2c_BitBangStart(uint8_t sda_pin, uint8_t scl_pin)
{
    BridgeI2c_ReleasePin(sda_pin);
    BridgeI2c_ReleasePin(scl_pin);
    if (BridgeI2c_WaitPinHigh(scl_pin) == 0u)
    {
        return 0u;
    }
    if (BridgeI2c_WaitPinHigh(sda_pin) == 0u)
    {
        return 0u;
    }
    BridgeI2c_DelayBit();
    BridgeI2c_DrivePinLow(sda_pin);
    BridgeI2c_DelayBit();
    BridgeI2c_DrivePinLow(scl_pin);
    BridgeI2c_DelayBit();
    return 1u;
}

static uint8_t BridgeI2c_BitBangStop(uint8_t sda_pin, uint8_t scl_pin)
{
    uint8_t ok;

    ok = 1u;
    BridgeI2c_DrivePinLow(sda_pin);
    BridgeI2c_DelayBit();
    BridgeI2c_ReleasePin(scl_pin);
    if (BridgeI2c_WaitPinHigh(scl_pin) == 0u)
    {
        ok = 0u;
    }
    BridgeI2c_DelayBit();
    BridgeI2c_ReleasePin(sda_pin);
    BridgeI2c_DelayBit();
    return ok;
}

static uint8_t BridgeI2c_BitBangWriteByte(uint8_t sda_pin, uint8_t scl_pin, uint8_t value, uint8_t *ack)
{
    uint8_t bit;
    uint8_t sample;

    if (ack != 0)
    {
        *ack = 0u;
    }

    for (bit = 0u; bit < 8u; ++bit)
    {
        if ((value & 0x80u) != 0u)
        {
            BridgeI2c_ReleasePin(sda_pin);
        }
        else
        {
            BridgeI2c_DrivePinLow(sda_pin);
        }
        BridgeI2c_DelayBit();
        BridgeI2c_ReleasePin(scl_pin);
        if (BridgeI2c_WaitPinHigh(scl_pin) == 0u)
        {
            return 0u;
        }
        BridgeI2c_DelayBit();
        BridgeI2c_DrivePinLow(scl_pin);
        BridgeI2c_DelayBit();
        value = (uint8_t)(value << 1u);
    }

    BridgeI2c_ReleasePin(sda_pin);
    BridgeI2c_DelayBit();
    BridgeI2c_ReleasePin(scl_pin);
    if (BridgeI2c_WaitPinHigh(scl_pin) == 0u)
    {
        return 0u;
    }
    BridgeI2c_DelayBit();
    sample = BridgeI2c_ReadPin(sda_pin);
    BridgeI2c_DrivePinLow(scl_pin);
    BridgeI2c_DelayBit();

    if (ack != 0)
    {
        *ack = (sample == 0u) ? 1u : 0u;
    }
    return 1u;
}

static uint8_t BridgeI2c_SmbusQuickReadBitBang(uint8_t port, uint8_t addr7, uint8_t *ack)
{
    I2C_T *i2c;
    uint8_t sda_pin;
    uint8_t scl_pin;
    uint8_t ok;
    uint8_t stop_ok;
    uint32_t baudrate;

    if (ack != 0)
    {
        *ack = 0u;
    }
    if ((port >= M261_BRIDGE_I2C_PORT_COUNT) || (addr7 > 0x7Fu) ||
        (ack == 0) ||
        (g_i2c_state[port].initialized == 0u) ||
        (g_i2c_state[port].role != M261_BRIDGE_I2C_ROLE_MASTER))
    {
        return 0u;
    }

    i2c = BridgeI2c_GetInst(port);
    if (i2c == 0)
    {
        return 0u;
    }

    sda_pin = g_i2c_state[port].sda_pin;
    scl_pin = g_i2c_state[port].scl_pin;
    baudrate = g_i2c_state[port].baudrate;

    I2C_Close(i2c);
    BridgeI2c_ClearMuxToGpio(port, sda_pin, scl_pin);
    BridgeI2c_ReleasePin(sda_pin);
    BridgeI2c_ReleasePin(scl_pin);
    BridgeI2c_DelayBit();

    ok = BridgeI2c_BitBangStart(sda_pin, scl_pin);
    if (ok != 0u)
    {
        ok = BridgeI2c_BitBangWriteByte(sda_pin, scl_pin, (uint8_t)((addr7 << 1u) | 0x01u), ack);
    }
    stop_ok = BridgeI2c_BitBangStop(sda_pin, scl_pin);

    (void)BridgeI2c_ApplyMux(port, sda_pin, scl_pin);
    (void)I2C_Open(i2c, baudrate);
    I2C_DisableInt(i2c);
    I2C_EnableTimeout(i2c, 0u);

    if ((ok == 0u) || (stop_ok == 0u))
    {
        BridgeI2c_MarkTimeout();
        return 0u;
    }
    return 1u;
}

uint8_t M261BridgeI2c_IsMasterReady(uint8_t port)
{
    if (port >= M261_BRIDGE_I2C_PORT_COUNT)
    {
        return 0u;
    }
    return ((g_i2c_state[port].initialized != 0u) &&
            (g_i2c_state[port].role == M261_BRIDGE_I2C_ROLE_MASTER)) ? 1u : 0u;
}

uint8_t M261BridgeI2c_IsSlaveReady(uint8_t port)
{
    if (port >= M261_BRIDGE_I2C_PORT_COUNT)
    {
        return 0u;
    }
    return ((g_i2c_state[port].initialized != 0u) &&
            (g_i2c_state[port].role == M261_BRIDGE_I2C_ROLE_SLAVE)) ? 1u : 0u;
}

uint8_t M261BridgeI2c_LastError(void)
{
    return g_i2c_last_error;
}

uint8_t M261BridgeI2c_InitMaster(uint8_t port, uint8_t sda_pin, uint8_t scl_pin, uint32_t baudrate)
{
    I2C_T *i2c;
    uint8_t other_port;

    BridgeI2c_ClearLastError();

    if ((port >= M261_BRIDGE_I2C_PORT_COUNT) ||
        (BridgeI2c_IsValidPair(port, sda_pin, scl_pin) == 0u))
    {
        return 0u;
    }

    if (baudrate < 10000u)
    {
        baudrate = 10000u;
    }
    if (baudrate > 1000000u)
    {
        baudrate = 1000000u;
    }

    other_port = (port == 0u) ? 1u : 0u;
    M261BridgeI2c_Deinit(other_port);
    M261BridgeI2c_Deinit(port);

    BridgeI2c_EnableClock(port);
    if (BridgeI2c_ApplyMux(port, sda_pin, scl_pin) == 0u)
    {
        BridgeI2c_DisableClock(port);
        return 0u;
    }

    i2c = BridgeI2c_GetInst(port);
    if (i2c == 0)
    {
        BridgeI2c_DisableClock(port);
        return 0u;
    }

    (void)I2C_Open(i2c, baudrate);
    I2C_DisableInt(i2c);
    NVIC_DisableIRQ(BridgeI2c_GetIrq(port));
    NVIC_ClearPendingIRQ(BridgeI2c_GetIrq(port));
    I2C_EnableTimeout(i2c, 0u);

    g_i2c_state[port].initialized = 1u;
    g_i2c_state[port].role = M261_BRIDGE_I2C_ROLE_MASTER;
    g_i2c_state[port].sda_pin = sda_pin;
    g_i2c_state[port].scl_pin = scl_pin;
    g_i2c_state[port].addr7 = 0u;
    g_i2c_state[port].baudrate = baudrate;
    return 1u;
}

uint8_t M261BridgeI2c_InitSlave(uint8_t port, uint8_t sda_pin, uint8_t scl_pin, uint8_t addr7, uint32_t baudrate)
{
    I2C_T *i2c;
    uint8_t other_port;

    BridgeI2c_ClearLastError();

    if ((port >= M261_BRIDGE_I2C_PORT_COUNT) ||
        (addr7 > 0x7Fu) ||
        (BridgeI2c_IsValidPair(port, sda_pin, scl_pin) == 0u))
    {
        return 0u;
    }

    if (baudrate < 10000u)
    {
        baudrate = 10000u;
    }
    if (baudrate > 1000000u)
    {
        baudrate = 1000000u;
    }

    other_port = (port == 0u) ? 1u : 0u;
    M261BridgeI2c_Deinit(other_port);
    M261BridgeI2c_Deinit(port);

    BridgeI2c_EnableClock(port);
    if (BridgeI2c_ApplyMux(port, sda_pin, scl_pin) == 0u)
    {
        BridgeI2c_DisableClock(port);
        return 0u;
    }

    i2c = BridgeI2c_GetInst(port);
    if (i2c == 0)
    {
        BridgeI2c_DisableClock(port);
        return 0u;
    }

    (void)I2C_Open(i2c, baudrate);
    I2C_EnableTimeout(i2c, 0u);
    I2C_SetSlaveAddr(i2c, 0u, addr7, 0u);
    I2C_SetSlaveAddrMask(i2c, 0u, 0u);
    BridgeI2c_ClearSlaveBuffers(port);

    g_i2c_state[port].initialized = 1u;
    g_i2c_state[port].role = M261_BRIDGE_I2C_ROLE_SLAVE;
    g_i2c_state[port].sda_pin = sda_pin;
    g_i2c_state[port].scl_pin = scl_pin;
    g_i2c_state[port].addr7 = addr7;
    g_i2c_state[port].baudrate = baudrate;

    I2C_EnableInt(i2c);
    NVIC_ClearPendingIRQ(BridgeI2c_GetIrq(port));
    NVIC_EnableIRQ(BridgeI2c_GetIrq(port));
    I2C_SET_CONTROL_REG(i2c, I2C_CTL_SI_AA);
    return 1u;
}

void M261BridgeI2c_Deinit(uint8_t port)
{
    I2C_T *i2c;
    uint8_t sda_pin;
    uint8_t scl_pin;
    uint8_t was_slave;

    if (port >= M261_BRIDGE_I2C_PORT_COUNT)
    {
        return;
    }

    BridgeI2c_ClearLastError();

    i2c = BridgeI2c_GetInst(port);
    sda_pin = g_i2c_state[port].sda_pin;
    scl_pin = g_i2c_state[port].scl_pin;
    was_slave = (g_i2c_state[port].role == M261_BRIDGE_I2C_ROLE_SLAVE) ? 1u : 0u;

    if (i2c != 0)
    {
        if (was_slave != 0u)
        {
            I2C_DisableInt(i2c);
            NVIC_DisableIRQ(BridgeI2c_GetIrq(port));
            NVIC_ClearPendingIRQ(BridgeI2c_GetIrq(port));
        }
        I2C_Close(i2c);
    }

    if (g_i2c_state[port].initialized != 0u)
    {
        BridgeI2c_ClearMuxToGpio(port, sda_pin, scl_pin);
    }

    memset(&g_i2c_state[port], 0, sizeof(g_i2c_state[port]));
    BridgeI2c_ClearSlaveBuffers(port);
    BridgeI2c_DisableClock(port);
}

uint8_t M261BridgeI2c_MasterWrite(uint8_t port, uint8_t addr7, const uint8_t *data, uint16_t len, uint16_t *written)
{
    I2C_T *i2c;
    uint8_t xfering;
    uint8_t err;
    uint8_t ctrl;
    uint32_t tx_len;
    uint32_t status;

    BridgeI2c_ClearLastError();

    if (written != 0)
    {
        *written = 0u;
    }
    if ((port >= M261_BRIDGE_I2C_PORT_COUNT) || (addr7 > 0x7Fu) ||
        (M261BridgeI2c_IsMasterReady(port) == 0u) || ((data == 0) && (len != 0u)))
    {
        return 0u;
    }
    if (len == 0u)
    {
        return 1u;
    }

    i2c = BridgeI2c_GetInst(port);
    if (i2c == 0)
    {
        return 0u;
    }

    xfering = 1u;
    err = 0u;
    ctrl = I2C_CTL_SI;
    tx_len = 0u;

    I2C_START(i2c);
    while ((xfering != 0u) && (err == 0u))
    {
        if (BridgeI2c_WaitReady(i2c) == 0u)
        {
            BridgeI2c_MarkIoError();
            err = 1u;
            break;
        }

        status = I2C_GET_STATUS(i2c);
        switch (status)
        {
            case 0x08u:
                I2C_SET_DATA(i2c, (uint8_t)(addr7 << 1u));
                ctrl = I2C_CTL_SI;
                break;

            case 0x18u:
            case 0x28u:
                if (tx_len < len)
                {
                    I2C_SET_DATA(i2c, data[tx_len]);
                    ++tx_len;
                    ctrl = I2C_CTL_SI;
                }
                else
                {
                    ctrl = I2C_CTL_STO_SI;
                    xfering = 0u;
                }
                break;

            case 0x30u:
                ctrl = I2C_CTL_STO_SI;
                BridgeI2c_MarkIoError();
                err = 1u;
                break;

            case 0x20u:
            case 0x38u:
            default:
                ctrl = I2C_CTL_STO_SI;
                BridgeI2c_MarkIoError();
                err = 1u;
                break;
        }

        I2C_SET_CONTROL_REG(i2c, ctrl);
    }

    if (BridgeI2c_WaitStopClear(i2c) == 0u)
    {
        BridgeI2c_MarkIoError();
        err = 1u;
    }
    if (written != 0)
    {
        *written = (uint16_t)tx_len;
    }
    if ((err == 0u) && (tx_len == len))
    {
        return 1u;
    }
    BridgeI2c_MarkIoError();
    return 0u;
}

uint8_t M261BridgeI2c_MasterRead(uint8_t port, uint8_t addr7, uint8_t *out, uint16_t len, uint16_t *read_len)
{
    I2C_T *i2c;
    uint8_t xfering;
    uint8_t err;
    uint8_t ctrl;
    uint32_t rx_len;
    uint32_t status;

    BridgeI2c_ClearLastError();

    if (read_len != 0)
    {
        *read_len = 0u;
    }
    if ((port >= M261_BRIDGE_I2C_PORT_COUNT) || (addr7 > 0x7Fu) ||
        (M261BridgeI2c_IsMasterReady(port) == 0u) || ((out == 0) && (len != 0u)))
    {
        return 0u;
    }
    if (len == 0u)
    {
        return 1u;
    }

    i2c = BridgeI2c_GetInst(port);
    if (i2c == 0)
    {
        return 0u;
    }

    xfering = 1u;
    err = 0u;
    ctrl = I2C_CTL_SI;
    rx_len = 0u;

    I2C_START(i2c);
    while ((xfering != 0u) && (err == 0u))
    {
        if (BridgeI2c_WaitReady(i2c) == 0u)
        {
            BridgeI2c_MarkIoError();
            err = 1u;
            break;
        }

        status = I2C_GET_STATUS(i2c);
        switch (status)
        {
            case 0x08u:
                I2C_SET_DATA(i2c, (uint8_t)((addr7 << 1u) | 0x01u));
                ctrl = I2C_CTL_SI;
                break;

            case 0x40u:
                ctrl = (len == 1u) ? I2C_CTL_SI : I2C_CTL_SI_AA;
                break;

            case 0x50u:
                out[rx_len] = (uint8_t)I2C_GET_DATA(i2c);
                ++rx_len;
                ctrl = (rx_len < ((uint32_t)len - 1u)) ? I2C_CTL_SI_AA : I2C_CTL_SI;
                break;

            case 0x58u:
                out[rx_len] = (uint8_t)I2C_GET_DATA(i2c);
                ++rx_len;
                ctrl = I2C_CTL_STO_SI;
                xfering = 0u;
                break;

            case 0x48u:
            case 0x38u:
            default:
                ctrl = I2C_CTL_STO_SI;
                BridgeI2c_MarkIoError();
                err = 1u;
                break;
        }

        I2C_SET_CONTROL_REG(i2c, ctrl);
    }

    if (BridgeI2c_WaitStopClear(i2c) == 0u)
    {
        BridgeI2c_MarkIoError();
        err = 1u;
    }
    if (read_len != 0)
    {
        *read_len = (uint16_t)rx_len;
    }
    if ((err == 0u) && (rx_len == len))
    {
        return 1u;
    }
    BridgeI2c_MarkIoError();
    return 0u;
}

uint8_t M261BridgeI2c_MasterWriteRead(uint8_t port, uint8_t addr7, const uint8_t *tx, uint16_t tx_len,
                                      uint8_t *rx, uint16_t rx_len, uint16_t *read_len, uint8_t repeated_start)
{
    I2C_T *i2c;
    uint8_t xfering;
    uint8_t err;
    uint8_t ctrl;
    uint8_t phase_read;
    uint32_t tx_index;
    uint32_t rx_index;
    uint32_t status;
    uint16_t written;

    BridgeI2c_ClearLastError();

    if (read_len != 0)
    {
        *read_len = 0u;
    }
    if ((port >= M261_BRIDGE_I2C_PORT_COUNT) || (addr7 > 0x7Fu) ||
        (M261BridgeI2c_IsMasterReady(port) == 0u) ||
        ((tx == 0) && (tx_len != 0u)) ||
        ((rx == 0) && (rx_len != 0u)))
    {
        return 0u;
    }
    if (rx_len == 0u)
    {
        return M261BridgeI2c_MasterWrite(port, addr7, tx, tx_len, &written);
    }
    if (tx_len == 0u)
    {
        return M261BridgeI2c_MasterRead(port, addr7, rx, rx_len, read_len);
    }
    if (repeated_start == 0u)
    {
        if (M261BridgeI2c_MasterWrite(port, addr7, tx, tx_len, &written) == 0u)
        {
            return 0u;
        }
        return M261BridgeI2c_MasterRead(port, addr7, rx, rx_len, read_len);
    }

    i2c = BridgeI2c_GetInst(port);
    if (i2c == 0)
    {
        return 0u;
    }

    xfering = 1u;
    err = 0u;
    ctrl = I2C_CTL_SI;
    phase_read = 0u;
    tx_index = 0u;
    rx_index = 0u;

    I2C_START(i2c);
    while ((xfering != 0u) && (err == 0u))
    {
        if (BridgeI2c_WaitReady(i2c) == 0u)
        {
            BridgeI2c_MarkIoError();
            err = 1u;
            break;
        }

        status = I2C_GET_STATUS(i2c);
        switch (status)
        {
            case 0x08u:
                I2C_SET_DATA(i2c, (uint8_t)(addr7 << 1u));
                ctrl = I2C_CTL_SI;
                break;

            case 0x18u:
            case 0x28u:
                if (tx_index < tx_len)
                {
                    I2C_SET_DATA(i2c, tx[tx_index]);
                    ++tx_index;
                    ctrl = I2C_CTL_SI;
                }
                else
                {
                    phase_read = 1u;
                    ctrl = I2C_CTL_STA_SI;
                }
                break;

            case 0x10u:
                if (phase_read == 0u)
                {
                    BridgeI2c_MarkIoError();
                    err = 1u;
                    ctrl = I2C_CTL_STO_SI;
                }
                else
                {
                    I2C_SET_DATA(i2c, (uint8_t)((addr7 << 1u) | 0x01u));
                    ctrl = I2C_CTL_SI;
                }
                break;

            case 0x40u:
                ctrl = (rx_len == 1u) ? I2C_CTL_SI : I2C_CTL_SI_AA;
                break;

            case 0x50u:
                rx[rx_index] = (uint8_t)I2C_GET_DATA(i2c);
                ++rx_index;
                ctrl = (rx_index < ((uint32_t)rx_len - 1u)) ? I2C_CTL_SI_AA : I2C_CTL_SI;
                break;

            case 0x58u:
                rx[rx_index] = (uint8_t)I2C_GET_DATA(i2c);
                ++rx_index;
                ctrl = I2C_CTL_STO_SI;
                xfering = 0u;
                break;

            case 0x20u:
            case 0x30u:
            case 0x38u:
            case 0x48u:
            default:
                ctrl = I2C_CTL_STO_SI;
                BridgeI2c_MarkIoError();
                err = 1u;
                break;
        }

        I2C_SET_CONTROL_REG(i2c, ctrl);
    }

    if (BridgeI2c_WaitStopClear(i2c) == 0u)
    {
        BridgeI2c_MarkIoError();
        err = 1u;
    }
    if (read_len != 0)
    {
        *read_len = (uint16_t)rx_index;
    }
    if ((err == 0u) && (rx_index == rx_len))
    {
        return 1u;
    }
    BridgeI2c_MarkIoError();
    return 0u;
}

uint8_t M261BridgeI2c_PmbusBlockRead(uint8_t port, uint8_t addr7, const uint8_t *tx, uint16_t tx_len,
                                     uint8_t expect_pec, uint8_t max_block_len, uint8_t *out, uint16_t *out_len)
{
    I2C_T *i2c;
    uint8_t xfering;
    uint8_t err;
    uint8_t ctrl;
    uint8_t phase_read;
    uint8_t target_known;
    uint8_t block_count;
    uint16_t target_len;
    uint32_t tx_index;
    uint32_t rx_index;
    uint32_t status;

    BridgeI2c_ClearLastError();

    if (out_len != 0)
    {
        *out_len = 0u;
    }
    if ((port >= M261_BRIDGE_I2C_PORT_COUNT) || (addr7 > 0x7Fu) ||
        (M261BridgeI2c_IsMasterReady(port) == 0u) || (tx == 0) ||
        (tx_len == 0u) || (out == 0) || (out_len == 0) ||
        (max_block_len == 0u) || (max_block_len > 32u))
    {
        return 0u;
    }

    i2c = BridgeI2c_GetInst(port);
    if (i2c == 0)
    {
        return 0u;
    }

    xfering = 1u;
    err = 0u;
    ctrl = I2C_CTL_SI;
    phase_read = 0u;
    target_known = 0u;
    block_count = 0u;
    target_len = 0u;
    tx_index = 0u;
    rx_index = 0u;

    I2C_START(i2c);
    while ((xfering != 0u) && (err == 0u))
    {
        if (BridgeI2c_WaitReady(i2c) == 0u)
        {
            BridgeI2c_MarkIoError();
            err = 1u;
            break;
        }

        status = I2C_GET_STATUS(i2c);
        switch (status)
        {
            case 0x08u:
                I2C_SET_DATA(i2c, (uint8_t)(addr7 << 1u));
                ctrl = I2C_CTL_SI;
                break;

            case 0x18u:
            case 0x28u:
                if (tx_index < tx_len)
                {
                    I2C_SET_DATA(i2c, tx[tx_index]);
                    ++tx_index;
                    ctrl = I2C_CTL_SI;
                }
                else
                {
                    phase_read = 1u;
                    ctrl = I2C_CTL_STA_SI;
                }
                break;

            case 0x10u:
                if (phase_read == 0u)
                {
                    BridgeI2c_MarkIoError();
                    err = 1u;
                    ctrl = I2C_CTL_STO_SI;
                }
                else
                {
                    I2C_SET_DATA(i2c, (uint8_t)((addr7 << 1u) | 0x01u));
                    ctrl = I2C_CTL_SI;
                }
                break;

            case 0x40u:
                ctrl = I2C_CTL_SI_AA;
                break;

            case 0x50u:
                out[rx_index] = (uint8_t)I2C_GET_DATA(i2c);
                ++rx_index;
                if (target_known == 0u)
                {
                    block_count = out[0];
                    if (block_count > max_block_len)
                    {
                        BridgeI2c_MarkIoError();
                        err = 1u;
                        ctrl = I2C_CTL_STO_SI;
                        break;
                    }
                    target_len = (uint16_t)(1u + block_count + ((expect_pec != 0u) ? 1u : 0u));
                    target_known = 1u;
                }

                if (rx_index >= target_len)
                {
                    ctrl = I2C_CTL_STO_SI;
                    xfering = 0u;
                }
                else
                {
                    ctrl = (rx_index < ((uint32_t)target_len - 1u)) ? I2C_CTL_SI_AA : I2C_CTL_SI;
                }
                break;

            case 0x58u:
                out[rx_index] = (uint8_t)I2C_GET_DATA(i2c);
                ++rx_index;
                ctrl = I2C_CTL_STO_SI;
                xfering = 0u;
                break;

            case 0x20u:
            case 0x30u:
            case 0x38u:
            case 0x48u:
            default:
                ctrl = I2C_CTL_STO_SI;
                BridgeI2c_MarkIoError();
                err = 1u;
                break;
        }

        I2C_SET_CONTROL_REG(i2c, ctrl);
    }

    if (BridgeI2c_WaitStopClear(i2c) == 0u)
    {
        BridgeI2c_MarkIoError();
        err = 1u;
    }
    *out_len = (uint16_t)rx_index;
    if ((err == 0u) && (target_known != 0u) && (rx_index >= target_len))
    {
        return 1u;
    }
    BridgeI2c_MarkIoError();
    return 0u;
}

static uint8_t BridgeI2c_ValidateGroupBlob(const uint8_t *segment_blob, uint16_t blob_len, uint8_t segment_count)
{
    uint16_t offset;
    uint16_t segment_len;
    uint8_t index;

    offset = 0u;
    for (index = 0u; index < segment_count; ++index)
    {
        if ((uint16_t)(offset + 3u) > blob_len)
        {
            return 0u;
        }
        segment_len = Bridge_ReadU16Le(&segment_blob[offset + 1u]);
        if ((segment_len == 0u) || ((uint16_t)(offset + 3u + segment_len) > blob_len))
        {
            return 0u;
        }
        offset = (uint16_t)(offset + 3u + segment_len);
    }
    return (offset == blob_len) ? 1u : 0u;
}

uint8_t M261BridgeI2c_GroupWrite(uint8_t port, const uint8_t *segment_blob, uint16_t blob_len,
                                 uint8_t segment_count, uint8_t *completed_segments)
{
    I2C_T *i2c;
    uint8_t xfering;
    uint8_t err;
    uint8_t ctrl;
    uint8_t index;
    uint8_t addr7;
    uint16_t offset;
    uint16_t segment_len;
    uint16_t data_offset;
    uint16_t tx_index;
    uint32_t status;

    BridgeI2c_ClearLastError();

    if (completed_segments != 0)
    {
        *completed_segments = 0u;
    }
    if ((port >= M261_BRIDGE_I2C_PORT_COUNT) || (segment_blob == 0) ||
        (segment_count == 0u) || (M261BridgeI2c_IsMasterReady(port) == 0u) ||
        (BridgeI2c_ValidateGroupBlob(segment_blob, blob_len, segment_count) == 0u))
    {
        return 0u;
    }

    i2c = BridgeI2c_GetInst(port);
    if (i2c == 0)
    {
        return 0u;
    }

    xfering = 1u;
    err = 0u;
    ctrl = I2C_CTL_SI;
    index = 0u;
    offset = 0u;
    addr7 = segment_blob[0] & 0x7Fu;
    segment_len = Bridge_ReadU16Le(&segment_blob[1]);
    data_offset = 3u;
    tx_index = 0u;

    I2C_START(i2c);
    while ((xfering != 0u) && (err == 0u))
    {
        if (BridgeI2c_WaitReady(i2c) == 0u)
        {
            BridgeI2c_MarkIoError();
            err = 1u;
            break;
        }

        status = I2C_GET_STATUS(i2c);
        switch (status)
        {
            case 0x08u:
            case 0x10u:
                I2C_SET_DATA(i2c, (uint8_t)(addr7 << 1u));
                ctrl = I2C_CTL_SI;
                break;

            case 0x18u:
            case 0x28u:
                if (tx_index < segment_len)
                {
                    I2C_SET_DATA(i2c, segment_blob[data_offset + tx_index]);
                    ++tx_index;
                    ctrl = I2C_CTL_SI;
                }
                else
                {
                    ++index;
                    if (completed_segments != 0)
                    {
                        *completed_segments = index;
                    }
                    if (index >= segment_count)
                    {
                        ctrl = I2C_CTL_STO_SI;
                        xfering = 0u;
                    }
                    else
                    {
                        offset = (uint16_t)(data_offset + segment_len);
                        addr7 = segment_blob[offset] & 0x7Fu;
                        segment_len = Bridge_ReadU16Le(&segment_blob[offset + 1u]);
                        data_offset = (uint16_t)(offset + 3u);
                        tx_index = 0u;
                        ctrl = I2C_CTL_STA_SI;
                    }
                }
                break;

            case 0x20u:
            case 0x30u:
            case 0x38u:
            default:
                ctrl = I2C_CTL_STO_SI;
                BridgeI2c_MarkIoError();
                err = 1u;
                break;
        }

        I2C_SET_CONTROL_REG(i2c, ctrl);
    }

    if (BridgeI2c_WaitStopClear(i2c) == 0u)
    {
        BridgeI2c_MarkIoError();
        err = 1u;
    }
    if (err == 0u)
    {
        return 1u;
    }
    BridgeI2c_MarkIoError();
    return 0u;
}

uint8_t M261BridgeI2c_BusStatus(uint8_t port, uint8_t recover_if_needed,
                                uint8_t *idle_before, uint8_t *recovered, uint8_t *idle_after)
{
    uint8_t sda_pin;
    uint8_t scl_pin;
    uint8_t idle;
    uint8_t count;
    uint32_t baudrate;
    I2C_T *i2c;

    BridgeI2c_ClearLastError();

    if ((port >= M261_BRIDGE_I2C_PORT_COUNT) || (M261BridgeI2c_IsMasterReady(port) == 0u) ||
        (idle_before == 0) || (recovered == 0) || (idle_after == 0))
    {
        return 0u;
    }

    sda_pin = g_i2c_state[port].sda_pin;
    scl_pin = g_i2c_state[port].scl_pin;
    baudrate = g_i2c_state[port].baudrate;
    i2c = BridgeI2c_GetInst(port);

    idle = ((BridgeI2c_ReadPin(sda_pin) != 0u) && (BridgeI2c_ReadPin(scl_pin) != 0u)) ? 1u : 0u;
    *idle_before = idle;
    *recovered = 0u;

    if ((idle == 0u) && (recover_if_needed != 0u) && (i2c != 0))
    {
        I2C_Close(i2c);
        BridgeI2c_ReleasePin(sda_pin);
        BridgeI2c_ReleasePin(scl_pin);
        BridgeI2c_DelayShort();

        for (count = 0u; count < 9u; ++count)
        {
            BridgeI2c_DrivePinLow(scl_pin);
            BridgeI2c_DelayShort();
            BridgeI2c_ReleasePin(scl_pin);
            BridgeI2c_DelayShort();
        }

        BridgeI2c_DrivePinLow(sda_pin);
        BridgeI2c_DelayShort();
        BridgeI2c_ReleasePin(scl_pin);
        BridgeI2c_DelayShort();
        BridgeI2c_ReleasePin(sda_pin);
        BridgeI2c_DelayShort();

        (void)BridgeI2c_ApplyMux(port, sda_pin, scl_pin);
        (void)I2C_Open(i2c, baudrate);
        I2C_EnableTimeout(i2c, 0u);
        *recovered = 1u;
    }

    *idle_after = ((BridgeI2c_ReadPin(sda_pin) != 0u) && (BridgeI2c_ReadPin(scl_pin) != 0u)) ? 1u : 0u;
    return 1u;
}

uint8_t M261BridgeI2c_SmbusQuick(uint8_t port, uint8_t addr7, uint8_t read_bit, uint8_t *ack)
{
    I2C_T *i2c;
    uint8_t xfering;
    uint8_t err;
    uint8_t ctrl;
    uint32_t status;

    BridgeI2c_ClearLastError();

    if (ack != 0)
    {
        *ack = 0u;
    }
    if ((port >= M261_BRIDGE_I2C_PORT_COUNT) || (addr7 > 0x7Fu) ||
        (M261BridgeI2c_IsMasterReady(port) == 0u) || (ack == 0))
    {
        return 0u;
    }
    if (read_bit != 0u)
    {
        return BridgeI2c_SmbusQuickReadBitBang(port, addr7, ack);
    }

    i2c = BridgeI2c_GetInst(port);
    if (i2c == 0)
    {
        return 0u;
    }

    xfering = 1u;
    err = 0u;
    ctrl = I2C_CTL_SI;

    I2C_START(i2c);
    while ((xfering != 0u) && (err == 0u))
    {
        if (BridgeI2c_WaitReady(i2c) == 0u)
        {
            BridgeI2c_MarkIoError();
            err = 1u;
            break;
        }

        status = I2C_GET_STATUS(i2c);
        switch (status)
        {
            case 0x08u:
                I2C_SET_DATA(i2c, (uint8_t)((addr7 << 1u) | ((read_bit != 0u) ? 1u : 0u)));
                ctrl = I2C_CTL_SI;
                break;

            case 0x18u:
            case 0x40u:
                *ack = 1u;
                ctrl = I2C_CTL_STO_SI;
                xfering = 0u;
                break;

            case 0x20u:
            case 0x48u:
                *ack = 0u;
                ctrl = I2C_CTL_STO_SI;
                xfering = 0u;
                break;

            case 0x38u:
            default:
                ctrl = I2C_CTL_STO_SI;
                BridgeI2c_MarkIoError();
                err = 1u;
                break;
        }

        I2C_SET_CONTROL_REG(i2c, ctrl);
    }

    if (BridgeI2c_WaitStopClear(i2c) == 0u)
    {
        BridgeI2c_MarkIoError();
        err = 1u;
    }
    if (err == 0u)
    {
        return 1u;
    }
    BridgeI2c_MarkIoError();
    return 0u;
}

uint8_t M261BridgeI2c_SlaveSetTx(uint8_t port, const uint8_t *data, uint16_t len, uint16_t *written)
{
    uint16_t index;
    IRQn_Type irq;

    BridgeI2c_ClearLastError();

    if (written != 0)
    {
        *written = 0u;
    }
    if ((port >= M261_BRIDGE_I2C_PORT_COUNT) ||
        (M261BridgeI2c_IsSlaveReady(port) == 0u) ||
        ((data == 0) && (len != 0u)))
    {
        return 0u;
    }
    if (len > M261_BRIDGE_I2C_SLAVE_BUF_SIZE)
    {
        len = M261_BRIDGE_I2C_SLAVE_BUF_SIZE;
    }

    irq = BridgeI2c_GetIrq(port);
    NVIC_DisableIRQ(irq);
    for (index = 0u; index < len; ++index)
    {
        g_i2c_slave_tx[port][index] = data[index];
    }
    g_i2c_slave_tx_len[port] = len;
    g_i2c_slave_tx_index[port] = 0u;
    NVIC_ClearPendingIRQ(irq);
    NVIC_EnableIRQ(irq);

    if (written != 0)
    {
        *written = len;
    }
    return 1u;
}

uint8_t M261BridgeI2c_SlaveGetRx(uint8_t port, uint8_t *out, uint16_t max_len, uint16_t *read_len)
{
    uint16_t index;
    uint16_t count;
    uint16_t remain;
    IRQn_Type irq;

    BridgeI2c_ClearLastError();

    if (read_len != 0)
    {
        *read_len = 0u;
    }
    if ((port >= M261_BRIDGE_I2C_PORT_COUNT) ||
        (M261BridgeI2c_IsSlaveReady(port) == 0u) ||
        ((out == 0) && (max_len != 0u)))
    {
        return 0u;
    }

    irq = BridgeI2c_GetIrq(port);
    NVIC_DisableIRQ(irq);
    count = g_i2c_slave_rx_len[port];
    if (count > max_len)
    {
        count = max_len;
    }

    for (index = 0u; index < count; ++index)
    {
        out[index] = g_i2c_slave_rx[port][index];
    }

    remain = (uint16_t)(g_i2c_slave_rx_len[port] - count);
    for (index = 0u; index < remain; ++index)
    {
        g_i2c_slave_rx[port][index] = g_i2c_slave_rx[port][index + count];
    }
    g_i2c_slave_rx_len[port] = remain;
    NVIC_ClearPendingIRQ(irq);
    NVIC_EnableIRQ(irq);

    if (read_len != 0)
    {
        *read_len = count;
    }
    return 1u;
}

void M261BridgeI2c_IrqHandler(uint8_t port)
{
    I2C_T *i2c;
    uint32_t status;
    uint8_t ctrl;

    if ((port >= M261_BRIDGE_I2C_PORT_COUNT) ||
        (M261BridgeI2c_IsSlaveReady(port) == 0u))
    {
        return;
    }

    i2c = BridgeI2c_GetInst(port);
    if (i2c == 0)
    {
        return;
    }

    status = I2C_GET_STATUS(i2c);
    ctrl = I2C_CTL_SI_AA;

    switch (status)
    {
        case 0x60u:
        case 0x68u:
        case 0x70u:
        case 0x78u:
            ctrl = I2C_CTL_SI_AA;
            break;

        case 0x80u:
        case 0x90u:
            BridgeI2c_SlavePushRx(port, (uint8_t)I2C_GET_DATA(i2c));
            ctrl = I2C_CTL_SI_AA;
            break;

        case 0x88u:
        case 0x98u:
            BridgeI2c_SlavePushRx(port, (uint8_t)I2C_GET_DATA(i2c));
            ctrl = I2C_CTL_SI_AA;
            break;

        case 0xA0u:
            ctrl = I2C_CTL_SI_AA;
            break;

        case 0xA8u:
        case 0xB0u:
            g_i2c_slave_tx_index[port] = 0u;
            I2C_SET_DATA(i2c, BridgeI2c_SlaveNextTx(port));
            ctrl = I2C_CTL_SI_AA;
            break;

        case 0xB8u:
            I2C_SET_DATA(i2c, BridgeI2c_SlaveNextTx(port));
            ctrl = I2C_CTL_SI_AA;
            break;

        case 0xC0u:
        case 0xC8u:
            g_i2c_slave_tx_index[port] = 0u;
            ctrl = I2C_CTL_SI_AA;
            break;

        case 0x00u:
            ctrl = I2C_CTL_STO_SI_AA;
            break;

        default:
            ctrl = I2C_CTL_SI_AA;
            break;
    }

    I2C_SET_CONTROL_REG(i2c, ctrl);
}

void I2C0_IRQHandler(void)
{
    M261BridgeI2c_IrqHandler(0u);
}

void I2C1_IRQHandler(void)
{
    M261BridgeI2c_IrqHandler(1u);
}
