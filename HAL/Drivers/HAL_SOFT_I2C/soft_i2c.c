#include "soft_i2c.h"

#define SOFT_I2C_SCL_PORT software_sdl_GPIO_Port
#define SOFT_I2C_SCL_PIN  software_sdl_Pin
#define SOFT_I2C_SDA_PORT software_sda_GPIO_Port
#define SOFT_I2C_SDA_PIN  software_sda_Pin

#define SOFT_I2C_SCL(x) HAL_GPIO_WritePin(SOFT_I2C_SCL_PORT, SOFT_I2C_SCL_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define SOFT_I2C_SDA(x) HAL_GPIO_WritePin(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)

static void SoftI2C_DelayUs(uint16_t us)
{
    while (us--)
    {
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
    }
}

void SoftI2C_Start(void)
{
    SOFT_I2C_SDA(1);
    SOFT_I2C_SCL(1);
    SoftI2C_DelayUs(3);
    SOFT_I2C_SDA(0);
    SoftI2C_DelayUs(3);
    SOFT_I2C_SCL(0);
    SoftI2C_DelayUs(3);
}

void SoftI2C_Stop(void)
{
    SOFT_I2C_SDA(0);
    SoftI2C_DelayUs(3);
    SOFT_I2C_SCL(1);
    SoftI2C_DelayUs(3);
    SOFT_I2C_SDA(1);
    SoftI2C_DelayUs(3);
}

void SoftI2C_SendAck(uint8_t ack)
{
    SOFT_I2C_SDA(ack ? 1 : 0);
    SoftI2C_DelayUs(2);
    SOFT_I2C_SCL(1);
    SoftI2C_DelayUs(3);
    SOFT_I2C_SCL(0);
    SoftI2C_DelayUs(2);
}

uint8_t SoftI2C_WaitAck(void)
{
    uint8_t ack;
    SOFT_I2C_SDA(1); // release line
    SOFT_I2C_SCL(1);
    SoftI2C_DelayUs(3);
    ack = (HAL_GPIO_ReadPin(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN) == GPIO_PIN_RESET) ? 1U : 0U;
    SOFT_I2C_SCL(0);
    SoftI2C_DelayUs(3);
    return ack;
}

void SoftI2C_WriteByteRaw(uint8_t byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        SOFT_I2C_SDA((byte & 0x80U) ? 1 : 0);
        byte <<= 1;
        SoftI2C_DelayUs(2);
        SOFT_I2C_SCL(1);
        SoftI2C_DelayUs(3);
        SOFT_I2C_SCL(0);
        SoftI2C_DelayUs(2);
    }
}

uint8_t SoftI2C_ReadByteRaw(void)
{
    uint8_t i;
    uint8_t data = 0;
    SOFT_I2C_SDA(1); // release line
    for (i = 0; i < 8; i++)
    {
        data <<= 1;
        SOFT_I2C_SCL(1);
        SoftI2C_DelayUs(3);
        if (HAL_GPIO_ReadPin(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN) == GPIO_PIN_SET)
        {
            data |= 0x01U;
        }
        SOFT_I2C_SCL(0);
        SoftI2C_DelayUs(2);
    }
    return data;
}

uint8_t SoftI2C_WriteByte(uint8_t byte)
{
    SoftI2C_WriteByteRaw(byte);
    return SoftI2C_WaitAck();
}

HAL_StatusTypeDef SoftI2C_WriteBytes(uint8_t dev_addr_w, const uint8_t *data, uint16_t len)
{
    uint16_t i;
    SoftI2C_Start();
    if (!SoftI2C_WriteByte(dev_addr_w)) { SoftI2C_Stop(); return HAL_ERROR; }
    for (i = 0; i < len; i++)
    {
        if (!SoftI2C_WriteByte(data[i])) { SoftI2C_Stop(); return HAL_ERROR; }
    }
    SoftI2C_Stop();
    return HAL_OK;
}

HAL_StatusTypeDef SoftI2C_WriteRegBytes(uint8_t dev_addr_w, uint8_t reg, const uint8_t *data, uint16_t len)
{
    uint16_t i;
    SoftI2C_Start();
    if (!SoftI2C_WriteByte(dev_addr_w)) { SoftI2C_Stop(); return HAL_ERROR; }
    if (!SoftI2C_WriteByte(reg)) { SoftI2C_Stop(); return HAL_ERROR; }
    for (i = 0; i < len; i++)
    {
        if (!SoftI2C_WriteByte(data[i])) { SoftI2C_Stop(); return HAL_ERROR; }
    }
    SoftI2C_Stop();
    return HAL_OK;
}

HAL_StatusTypeDef SoftI2C_ReadRegBytes(uint8_t dev_addr_w, uint8_t dev_addr_r, uint8_t reg, uint8_t *data, uint16_t len)
{
    uint16_t i;
    SoftI2C_Start();
    if (!SoftI2C_WriteByte(dev_addr_w)) { SoftI2C_Stop(); return HAL_ERROR; }
    if (!SoftI2C_WriteByte(reg)) { SoftI2C_Stop(); return HAL_ERROR; }
    SoftI2C_Start();
    if (!SoftI2C_WriteByte(dev_addr_r)) { SoftI2C_Stop(); return HAL_ERROR; }
    for (i = 0; i < len; i++)
    {
        data[i] = SoftI2C_ReadByteRaw();
        SoftI2C_SendAck((i == (len - 1U)) ? 1U : 0U);
    }
    SoftI2C_Stop();
    return HAL_OK;
}
