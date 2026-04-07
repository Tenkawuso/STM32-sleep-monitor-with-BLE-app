// I2C bit-bang BH1750 driver: port/pin definitions follow I2C open-drain requirements
#pragma once
#include <stdint.h>
#include "main.h"
// Port/pin configuration for SCL and SDA (open-drain with pull-up)
#define BH1750_SCL_PORT GPIOB
#define BH1750_SCL_PIN  GPIO_PIN_12
#define BH1750_SDA_PORT GPIOB
#define BH1750_SDA_PIN  GPIO_PIN_13

// I2C addresses (7-bit address shifted left for write/read forms)
#define BH1750_ADDR_W 0x46 // (0x23 << 1)
#define BH1750_ADDR_R 0x47 // (0x23 << 1 | 1)

// BH1750 commands
#define BH1750_PWR_DOWN 0x00
#define BH1750_PWR_ON   0x01
#define BH1750_RST      0x07
#define BH1750_CON_H    0x10
#define BH1750_CON_H2   0x11
#define BH1750_CON_L    0x13
#define BH1750_ONE_H    0x20
#define BH1750_ONE_H2   0x21
#define BH1750_ONE_L    0x23

void BH1750_Start();
void BH1750_Stop();
void Init_BH1750();
uint16_t mread(void);
uint32_t Value_GY30(void);
void delay_us(uint16_t us);
void BH1750_SendACK(int ack);
int BH1750_RecvACK();
void BH1750_SendByte(uint8_t dat);
uint8_t BH1750_RecvByte();
int I2C_ReadData(uint8_t slaveAddr, uint8_t regAddr, uint8_t *pData, uint16_t dataLen);
void Single_Write_BH1750(uint8_t REG_Address);
