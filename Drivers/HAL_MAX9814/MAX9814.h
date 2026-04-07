#ifndef __MAX9814_H
#define __MAX9814_H

#include <stdint.h>

typedef struct
{
    int32_t dc_q16;
    uint16_t level;
    uint16_t dc_track_shift;
    uint16_t ema_alpha_q15;
    uint16_t adc_full_scale;
} MAX9814_HandleTypeDef;

void MAX9814_Init(MAX9814_HandleTypeDef *h, uint16_t adc_mid, uint16_t adc_full_scale);
void MAX9814_ProcessBlock(MAX9814_HandleTypeDef *h, const uint16_t *samples, uint16_t count);
uint16_t MAX9814_GetLevel(const MAX9814_HandleTypeDef *h);
uint8_t MAX9814_GetLevelPercent(const MAX9814_HandleTypeDef *h);
int16_t MAX9814_GetLevelDb10(const MAX9814_HandleTypeDef *h, uint16_t ref_level);

#endif
