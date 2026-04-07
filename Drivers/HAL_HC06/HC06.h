#ifndef __HC06_H
#define __HC06_H

#include "main.h"
#include <stdint.h>

typedef struct
{
    UART_HandleTypeDef *huart;
    GPIO_TypeDef *state_port;
    uint16_t state_pin;
    uint32_t timeout_ms;
} HC06_HandleTypeDef;

HAL_StatusTypeDef HC06_Init(HC06_HandleTypeDef *hc06,
                            UART_HandleTypeDef *huart,
                            GPIO_TypeDef *state_port,
                            uint16_t state_pin,
                            uint32_t timeout_ms);

HAL_StatusTypeDef HC06_SendRaw(HC06_HandleTypeDef *hc06, const uint8_t *data, uint16_t len);
HAL_StatusTypeDef HC06_SendString(HC06_HandleTypeDef *hc06, const char *text);
HAL_StatusTypeDef HC06_Receive(HC06_HandleTypeDef *hc06, uint8_t *data, uint16_t len, uint32_t timeout_ms);
HAL_StatusTypeDef HC06_ReceiveLine(HC06_HandleTypeDef *hc06, char *line, uint16_t line_size, uint32_t timeout_ms);
HAL_StatusTypeDef HC06_SendAT(HC06_HandleTypeDef *hc06, const char *command, char *response, uint16_t response_size, uint32_t wait_ms);

uint8_t HC06_IsConnected(HC06_HandleTypeDef *hc06);

#endif
