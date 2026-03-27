#ifndef __MAX30102_H
#define __MAX30102_H

#include <stdint.h>
#include "main.h"

#define MAX30102_SCL_PORT software_sdl_GPIO_Port
#define MAX30102_SCL_PIN  software_sdl_Pin
#define MAX30102_SDA_PORT software_sda_GPIO_Port
#define MAX30102_SDA_PIN  software_sda_Pin

#define MAX30102_ADDR_W            0xAE
#define MAX30102_ADDR_R            0xAF

#define MAX30102_REG_INTR_STATUS_1   0x00
#define MAX30102_REG_INTR_STATUS_2   0x01
#define MAX30102_REG_INTR_ENABLE_1   0x02
#define MAX30102_REG_INTR_ENABLE_2   0x03
#define MAX30102_REG_FIFO_WR_PTR     0x04
#define MAX30102_REG_OVF_COUNTER     0x05
#define MAX30102_REG_FIFO_RD_PTR     0x06
#define MAX30102_REG_FIFO_DATA       0x07
#define MAX30102_REG_FIFO_CONFIG     0x08
#define MAX30102_REG_MODE_CONFIG     0x09
#define MAX30102_REG_SPO2_CONFIG     0x0A
#define MAX30102_REG_LED1_PA         0x0C
#define MAX30102_REG_LED2_PA         0x0D
#define MAX30102_REG_MULTI_LED_CTRL1 0x11
#define MAX30102_REG_MULTI_LED_CTRL2 0x12
#define MAX30102_REG_TEMP_INT        0x1F
#define MAX30102_REG_TEMP_FRAC       0x20
#define MAX30102_REG_REV_ID          0xFE
#define MAX30102_REG_PART_ID         0xFF

#define MAX30102_MODE_HR             0x02
#define MAX30102_MODE_SPO2           0x03
#define MAX30102_MODE_MULTI_LED      0x07

#define MAX30102_PART_ID             0x15

typedef struct
{
    uint16_t dev_addr_write;
    uint16_t dev_addr_read;
} MAX30102_HandleTypeDef;

HAL_StatusTypeDef MAX30102_Init(MAX30102_HandleTypeDef *max30102);
HAL_StatusTypeDef MAX30102_Reset(MAX30102_HandleTypeDef *max30102);
HAL_StatusTypeDef MAX30102_CheckDevice(MAX30102_HandleTypeDef *max30102, uint8_t *part_id);
HAL_StatusTypeDef MAX30102_SetMode(MAX30102_HandleTypeDef *max30102, uint8_t mode);
HAL_StatusTypeDef MAX30102_SetLedCurrent(MAX30102_HandleTypeDef *max30102, uint8_t led1_pa, uint8_t led2_pa);
HAL_StatusTypeDef MAX30102_ClearFifo(MAX30102_HandleTypeDef *max30102);
HAL_StatusTypeDef MAX30102_ReadSample(MAX30102_HandleTypeDef *max30102, uint32_t *red, uint32_t *ir);
HAL_StatusTypeDef MAX30102_GetFifoPtrs(MAX30102_HandleTypeDef *max30102, uint8_t *wr_ptr, uint8_t *rd_ptr);
uint8_t MAX30102_GetFifoSamplesAvailable(uint8_t wr_ptr, uint8_t rd_ptr);
HAL_StatusTypeDef MAX30102_ReadSamples(MAX30102_HandleTypeDef *max30102, uint32_t *red, uint32_t *ir, uint8_t sample_count);
HAL_StatusTypeDef MAX30102_ReadTemperature(MAX30102_HandleTypeDef *max30102, float *temperature);
HAL_StatusTypeDef MAX30102_ReadReg(MAX30102_HandleTypeDef *max30102, uint8_t reg, uint8_t *value);
HAL_StatusTypeDef MAX30102_WriteReg(MAX30102_HandleTypeDef *max30102, uint8_t reg, uint8_t value);

#endif
