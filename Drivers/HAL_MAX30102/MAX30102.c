#include "MAX30102.h"
#include "soft_i2c.h"

extern void HAL_Delay(uint32_t Delay);

static void max30102_start(void)
{
    SoftI2C_Start();
}

static void max30102_stop(void)
{
    SoftI2C_Stop();
}

static void max30102_send_ack(uint8_t ack)
{
    SoftI2C_SendAck(ack);
}

static uint8_t max30102_recv_ack(void)
{
    return SoftI2C_WaitAck() ? 0U : 1U;
}

static void max30102_send_byte(uint8_t data)
{
    SoftI2C_WriteByteRaw(data);
}

static uint8_t max30102_recv_byte(void)
{
    return SoftI2C_ReadByteRaw();
}

static HAL_StatusTypeDef max30102_write_multi(MAX30102_HandleTypeDef *max30102, uint8_t reg, uint8_t *data, uint16_t len)
{
    uint16_t i;
    max30102_start();
    max30102_send_byte((uint8_t)max30102->dev_addr_write);
    if (max30102_recv_ack() != 0U)
    {
        max30102_stop();
        return HAL_ERROR;
    }

    max30102_send_byte(reg);
    if (max30102_recv_ack() != 0U)
    {
        max30102_stop();
        return HAL_ERROR;
    }

    for (i = 0; i < len; i++)
    {
        max30102_send_byte(data[i]);
        if (max30102_recv_ack() != 0U)
        {
            max30102_stop();
            return HAL_ERROR;
        }
    }

    max30102_stop();
    return HAL_OK;
}

static HAL_StatusTypeDef max30102_read_multi(MAX30102_HandleTypeDef *max30102, uint8_t reg, uint8_t *data, uint16_t len)
{
    uint16_t i;
    max30102_start();
    max30102_send_byte((uint8_t)max30102->dev_addr_write);
    if (max30102_recv_ack() != 0U)
    {
        max30102_stop();
        return HAL_ERROR;
    }

    max30102_send_byte(reg);
    if (max30102_recv_ack() != 0U)
    {
        max30102_stop();
        return HAL_ERROR;
    }

    max30102_start();
    max30102_send_byte((uint8_t)max30102->dev_addr_read);
    if (max30102_recv_ack() != 0U)
    {
        max30102_stop();
        return HAL_ERROR;
    }

    for (i = 0; i < len; i++)
    {
        data[i] = max30102_recv_byte();
        max30102_send_ack((i == (len - 1U)) ? 1U : 0U);
    }

    max30102_stop();
    return HAL_OK;
}

HAL_StatusTypeDef MAX30102_WriteReg(MAX30102_HandleTypeDef *max30102, uint8_t reg, uint8_t value)
{
    return max30102_write_multi(max30102, reg, &value, 1);
}

HAL_StatusTypeDef MAX30102_ReadReg(MAX30102_HandleTypeDef *max30102, uint8_t reg, uint8_t *value)
{
    return max30102_read_multi(max30102, reg, value, 1);
}

HAL_StatusTypeDef MAX30102_Reset(MAX30102_HandleTypeDef *max30102)
{
    HAL_StatusTypeDef status;
    uint8_t value;
    uint16_t timeout = 500;

    status = MAX30102_WriteReg(max30102, MAX30102_REG_MODE_CONFIG, 0x40);
    if (status != HAL_OK)
    {
        return status;
    }

    do
    {
        status = MAX30102_ReadReg(max30102, MAX30102_REG_MODE_CONFIG, &value);
        if (status != HAL_OK)
        {
            return status;
        }
        HAL_Delay(1);
        timeout--;
    } while (((value & 0x40U) != 0U) && (timeout > 0U));

    return (timeout > 0U) ? HAL_OK : HAL_TIMEOUT;
}

HAL_StatusTypeDef MAX30102_CheckDevice(MAX30102_HandleTypeDef *max30102, uint8_t *part_id)
{
    HAL_StatusTypeDef status;
    uint8_t value;

    status = MAX30102_ReadReg(max30102, MAX30102_REG_PART_ID, &value);
    if (status != HAL_OK)
    {
        return status;
    }

    if (part_id != NULL)
    {
        *part_id = value;
    }

    return (value == MAX30102_PART_ID) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef MAX30102_SetMode(MAX30102_HandleTypeDef *max30102, uint8_t mode)
{
    return MAX30102_WriteReg(max30102, MAX30102_REG_MODE_CONFIG, mode & 0x07U);
}

HAL_StatusTypeDef MAX30102_SetLedCurrent(MAX30102_HandleTypeDef *max30102, uint8_t led1_pa, uint8_t led2_pa)
{
    HAL_StatusTypeDef status;
    status = MAX30102_WriteReg(max30102, MAX30102_REG_LED1_PA, led1_pa);
    if (status != HAL_OK)
    {
        return status;
    }
    return MAX30102_WriteReg(max30102, MAX30102_REG_LED2_PA, led2_pa);
}

HAL_StatusTypeDef MAX30102_ClearFifo(MAX30102_HandleTypeDef *max30102)
{
    HAL_StatusTypeDef status;
    status = MAX30102_WriteReg(max30102, MAX30102_REG_FIFO_WR_PTR, 0x00);
    if (status != HAL_OK)
    {
        return status;
    }
    status = MAX30102_WriteReg(max30102, MAX30102_REG_OVF_COUNTER, 0x00);
    if (status != HAL_OK)
    {
        return status;
    }
    return MAX30102_WriteReg(max30102, MAX30102_REG_FIFO_RD_PTR, 0x00);
}

HAL_StatusTypeDef MAX30102_Init(MAX30102_HandleTypeDef *max30102)
{
    HAL_StatusTypeDef status;
    uint8_t part_id;

    max30102->dev_addr_write = MAX30102_ADDR_W;
    max30102->dev_addr_read = MAX30102_ADDR_R;

    status = MAX30102_CheckDevice(max30102, &part_id);
    if (status != HAL_OK)
    {
        return status;
    }

    status = MAX30102_Reset(max30102);
    if (status != HAL_OK)
    {
        return status;
    }

    status = MAX30102_WriteReg(max30102, MAX30102_REG_INTR_ENABLE_1, 0x00);
    if (status != HAL_OK)
    {
        return status;
    }

    status = MAX30102_WriteReg(max30102, MAX30102_REG_INTR_ENABLE_2, 0x00);
    if (status != HAL_OK)
    {
        return status;
    }

    status = MAX30102_ClearFifo(max30102);
    if (status != HAL_OK)
    {
        return status;
    }

    status = MAX30102_WriteReg(max30102, MAX30102_REG_FIFO_CONFIG, 0x4F);
    if (status != HAL_OK)
    {
        return status;
    }

    status = MAX30102_WriteReg(max30102, MAX30102_REG_SPO2_CONFIG, 0x27);
    if (status != HAL_OK)
    {
        return status;
    }

    status = MAX30102_SetLedCurrent(max30102, 0x24, 0x24);
    if (status != HAL_OK)
    {
        return status;
    }

    return MAX30102_SetMode(max30102, MAX30102_MODE_SPO2);
}

HAL_StatusTypeDef MAX30102_ReadSample(MAX30102_HandleTypeDef *max30102, uint32_t *red, uint32_t *ir)
{
    HAL_StatusTypeDef status;
    uint8_t data[6];

    status = max30102_read_multi(max30102, MAX30102_REG_FIFO_DATA, data, 6);
    if (status != HAL_OK)
    {
        return status;
    }

    if (red != NULL)
    {
        *red = (((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2]) & 0x03FFFFU;
    }
    if (ir != NULL)
    {
        *ir = (((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 8) | data[5]) & 0x03FFFFU;
    }

    return HAL_OK;
}

HAL_StatusTypeDef MAX30102_GetFifoPtrs(MAX30102_HandleTypeDef *max30102, uint8_t *wr_ptr, uint8_t *rd_ptr)
{
    HAL_StatusTypeDef status;
    uint8_t wr;
    uint8_t rd;

    status = MAX30102_ReadReg(max30102, MAX30102_REG_FIFO_WR_PTR, &wr);
    if (status != HAL_OK)
    {
        return status;
    }

    status = MAX30102_ReadReg(max30102, MAX30102_REG_FIFO_RD_PTR, &rd);
    if (status != HAL_OK)
    {
        return status;
    }

    if (wr_ptr != NULL)
    {
        *wr_ptr = (uint8_t)(wr & 0x1FU);
    }

    if (rd_ptr != NULL)
    {
        *rd_ptr = (uint8_t)(rd & 0x1FU);
    }

    return HAL_OK;
}

uint8_t MAX30102_GetFifoSamplesAvailable(uint8_t wr_ptr, uint8_t rd_ptr)
{
    return (uint8_t)((wr_ptr - rd_ptr) & 0x1FU);
}

HAL_StatusTypeDef MAX30102_ReadSamples(MAX30102_HandleTypeDef *max30102, uint32_t *red, uint32_t *ir, uint8_t sample_count)
{
    HAL_StatusTypeDef status;
    uint16_t i;
    uint8_t raw[6 * 8];
    uint16_t bytes;

    if ((sample_count == 0U) || (sample_count > 8U) || (red == NULL) || (ir == NULL))
    {
        return HAL_ERROR;
    }

    bytes = (uint16_t)sample_count * 6U;
    status = max30102_read_multi(max30102, MAX30102_REG_FIFO_DATA, raw, bytes);
    if (status != HAL_OK)
    {
        return status;
    }

    for (i = 0U; i < sample_count; i++)
    {
        uint16_t idx = (uint16_t)i * 6U;
        red[i] = (((uint32_t)raw[idx] << 16) | ((uint32_t)raw[idx + 1U] << 8) | raw[idx + 2U]) & 0x03FFFFU;
        ir[i] = (((uint32_t)raw[idx + 3U] << 16) | ((uint32_t)raw[idx + 4U] << 8) | raw[idx + 5U]) & 0x03FFFFU;
    }

    return HAL_OK;
}

HAL_StatusTypeDef MAX30102_ReadTemperature(MAX30102_HandleTypeDef *max30102, float *temperature)
{
    HAL_StatusTypeDef status;
    uint8_t temp_int;
    uint8_t temp_frac;
    uint8_t mode_cfg;
    uint16_t timeout = 200;

    status = MAX30102_ReadReg(max30102, MAX30102_REG_MODE_CONFIG, &mode_cfg);
    if (status != HAL_OK)
    {
        return status;
    }

    status = MAX30102_WriteReg(max30102, MAX30102_REG_MODE_CONFIG, mode_cfg | 0x08U);
    if (status != HAL_OK)
    {
        return status;
    }

    do
    {
        status = MAX30102_ReadReg(max30102, MAX30102_REG_MODE_CONFIG, &mode_cfg);
        if (status != HAL_OK)
        {
            return status;
        }
        HAL_Delay(1);
        timeout--;
    } while (((mode_cfg & 0x08U) != 0U) && (timeout > 0U));

    if (timeout == 0U)
    {
        return HAL_TIMEOUT;
    }

    status = MAX30102_ReadReg(max30102, MAX30102_REG_TEMP_INT, &temp_int);
    if (status != HAL_OK)
    {
        return status;
    }
    status = MAX30102_ReadReg(max30102, MAX30102_REG_TEMP_FRAC, &temp_frac);
    if (status != HAL_OK)
    {
        return status;
    }

    if (temperature != NULL)
    {
        *temperature = (float)((int8_t)temp_int) + ((float)(temp_frac & 0x0FU) * 0.0625f);
    }

    return HAL_OK;
}
