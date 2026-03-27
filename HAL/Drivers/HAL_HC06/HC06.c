#include "HC06.h"

#include <string.h>

static uint16_t hc06_strnlen(const char *s, uint16_t max_len)
{
    uint16_t i;

    if (s == NULL)
    {
        return 0U;
    }

    for (i = 0U; i < max_len; i++)
    {
        if (s[i] == '\0')
        {
            break;
        }
    }
    return i;
}

HAL_StatusTypeDef HC06_Init(HC06_HandleTypeDef *hc06,
                            UART_HandleTypeDef *huart,
                            GPIO_TypeDef *state_port,
                            uint16_t state_pin,
                            uint32_t timeout_ms)
{
    if ((hc06 == NULL) || (huart == NULL))
    {
        return HAL_ERROR;
    }

    hc06->huart = huart;
    hc06->state_port = state_port;
    hc06->state_pin = state_pin;
    hc06->timeout_ms = (timeout_ms == 0U) ? 100U : timeout_ms;

    return HAL_OK;
}

HAL_StatusTypeDef HC06_SendRaw(HC06_HandleTypeDef *hc06, const uint8_t *data, uint16_t len)
{
    if ((hc06 == NULL) || (hc06->huart == NULL) || (data == NULL) || (len == 0U))
    {
        return HAL_ERROR;
    }

    return HAL_UART_Transmit(hc06->huart, (uint8_t *)data, len, hc06->timeout_ms);
}

HAL_StatusTypeDef HC06_SendString(HC06_HandleTypeDef *hc06, const char *text)
{
    uint16_t len = hc06_strnlen(text, 256U);

    if (len == 0U)
    {
        return HAL_ERROR;
    }

    return HC06_SendRaw(hc06, (const uint8_t *)text, len);
}

HAL_StatusTypeDef HC06_Receive(HC06_HandleTypeDef *hc06, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    uint32_t timeout;

    if ((hc06 == NULL) || (hc06->huart == NULL) || (data == NULL) || (len == 0U))
    {
        return HAL_ERROR;
    }

    timeout = (timeout_ms == 0U) ? hc06->timeout_ms : timeout_ms;
    return HAL_UART_Receive(hc06->huart, data, len, timeout);
}

HAL_StatusTypeDef HC06_ReceiveLine(HC06_HandleTypeDef *hc06, char *line, uint16_t line_size, uint32_t timeout_ms)
{
    HAL_StatusTypeDef st;
    uint16_t idx = 0U;
    uint8_t ch;
    uint32_t start;
    uint32_t timeout;

    if ((hc06 == NULL) || (hc06->huart == NULL) || (line == NULL) || (line_size < 2U))
    {
        return HAL_ERROR;
    }

    line[0] = '\0';
    timeout = (timeout_ms == 0U) ? hc06->timeout_ms : timeout_ms;
    start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout)
    {
        st = HAL_UART_Receive(hc06->huart, &ch, 1U, 5U);
        if (st == HAL_OK)
        {
            if ((ch == '\n') || (ch == '\r'))
            {
                if (idx > 0U)
                {
                    break;
                }
            }
            else if (idx < (line_size - 1U))
            {
                line[idx++] = (char)ch;
                line[idx] = '\0';
            }
        }
    }

    return (idx > 0U) ? HAL_OK : HAL_TIMEOUT;
}

HAL_StatusTypeDef HC06_SendAT(HC06_HandleTypeDef *hc06, const char *command, char *response, uint16_t response_size, uint32_t wait_ms)
{
    HAL_StatusTypeDef st;
    uint8_t ch;
    uint16_t idx = 0U;
    uint32_t start;

    if ((hc06 == NULL) || (hc06->huart == NULL) || (command == NULL))
    {
        return HAL_ERROR;
    }

    st = HC06_SendString(hc06, command);
    if (st != HAL_OK)
    {
        return st;
    }

    if ((response == NULL) || (response_size < 2U))
    {
        return HAL_OK;
    }

    response[0] = '\0';
    start = HAL_GetTick();

    while ((HAL_GetTick() - start) < wait_ms)
    {
        st = HAL_UART_Receive(hc06->huart, &ch, 1U, 5U);
        if (st == HAL_OK)
        {
            if (idx < (response_size - 1U))
            {
                response[idx++] = (char)ch;
                response[idx] = '\0';
            }
        }
    }

    return HAL_OK;
}

uint8_t HC06_IsConnected(HC06_HandleTypeDef *hc06)
{
    if ((hc06 == NULL) || (hc06->state_port == NULL))
    {
        return 0U;
    }

    return (HAL_GPIO_ReadPin(hc06->state_port, hc06->state_pin) == GPIO_PIN_SET) ? 1U : 0U;
}
