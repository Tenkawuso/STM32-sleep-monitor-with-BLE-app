#include "BH1750.h"
#include "soft_i2c.h"

extern void HAL_Delay(uint32_t Delay);

uint8_t mcy = 0;
uint8_t BUF[3];

// I2C Bitbang BH1750
void BH1750_Start()
{
    SoftI2C_Start();
}

void BH1750_Stop()
{
    SoftI2C_Stop();
}

void Init_BH1750()
{
    BH1750_Start();
    BH1750_SendByte(BH1750_ADDR_W);
    BH1750_SendByte(0x01);
    BH1750_Stop();
}

uint16_t mread(void)
{   
    uint8_t i;
    uint16_t data = 0;
    BH1750_Start();
    BH1750_SendByte(BH1750_ADDR_R);
    for (i = 0; i < 2; i++) {
        BUF[i] = BH1750_RecvByte();
        if (i == 1) {
            BH1750_SendACK(1); // NACK after last byte
        } else {
            BH1750_SendACK(0); // ACK after first byte
        }
    }
    BH1750_Stop();
    HAL_Delay(5);
    data = (BUF[0] << 8) | BUF[1];
    return data;
}

uint32_t Value_GY30(void)
{
    uint16_t raw;
    uint32_t lux;
    Single_Write_BH1750(BH1750_ADDR_W);   // power on
    Single_Write_BH1750(BH1750_CON_H);    // high resolution mode
    HAL_Delay(180);                        // wait for measurement
    raw = mread();                            // read two-byte data
    lux = (uint32_t)((float)raw / 1.2f);
    return lux;
}

// microsecond delay assumes ~72MHz system clock
void delay_us(uint16_t us)
{
    while(us--)
    {
        __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
        __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
        __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
        __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
        __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
        __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
        __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
        __NOP();__NOP();
    }
}

/**************************************
 发送ACK/NAK
  ack: 0 -> ACK, 1 -> NAK
**************************************/
void BH1750_SendACK(int ack)
{
    if ((ack == 0) || (ack == 1))
    {
        SoftI2C_SendAck((uint8_t)ack);
    }
}

/**************************************
  ACK check
**************************************/
int BH1750_RecvACK()
{
    mcy = SoftI2C_WaitAck() ? 0 : 1;
    return mcy;
}

/**************************************
 I2C write byte (MSB first)
**************************************/
void BH1750_SendByte(uint8_t dat)
{
    SoftI2C_WriteByte(dat);
}

// 供上层调用的简单写寄存器地址（带起始条件）
void Single_Write_BH1750(uint8_t REG_Address)
{
    BH1750_Start();
    BH1750_SendByte(BH1750_ADDR_W);
    BH1750_SendByte(REG_Address);
    BH1750_Stop();
}

/**************************************
 读字节（8bit）
**************************************/
uint8_t BH1750_RecvByte()
{
    return SoftI2C_ReadByteRaw();
}
