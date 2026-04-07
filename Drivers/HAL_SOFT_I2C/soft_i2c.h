#ifndef __SOFT_I2C_H
#define __SOFT_I2C_H

#include "main.h"
#include <stdint.h>

void SoftI2C_Start(void);
void SoftI2C_Stop(void);
void SoftI2C_SendAck(uint8_t ack);
uint8_t SoftI2C_WaitAck(void);
void SoftI2C_WriteByteRaw(uint8_t byte);
uint8_t SoftI2C_ReadByteRaw(void);
uint8_t SoftI2C_WriteByte(uint8_t byte);

HAL_StatusTypeDef SoftI2C_WriteBytes(uint8_t dev_addr_w, const uint8_t *data, uint16_t len);
HAL_StatusTypeDef SoftI2C_WriteRegBytes(uint8_t dev_addr_w, uint8_t reg, const uint8_t *data, uint16_t len);
HAL_StatusTypeDef SoftI2C_ReadRegBytes(uint8_t dev_addr_w, uint8_t dev_addr_r, uint8_t reg, uint8_t *data, uint16_t len);

#endif
