#include "MAX9814.h"
#include <math.h>

void MAX9814_Init(MAX9814_HandleTypeDef *h, uint16_t adc_mid, uint16_t adc_full_scale)
{
    if (h == 0)
    {
        return;
    }
    h->dc_q16 = ((int32_t)adc_mid << 16);
    h->level = 0;
    h->dc_track_shift = 8;
    h->ema_alpha_q15 = 4096;
    h->adc_full_scale = (adc_full_scale == 0U) ? 4095U : adc_full_scale;
}

void MAX9814_ProcessBlock(MAX9814_HandleTypeDef *h, const uint16_t *samples, uint16_t count)
{
    uint32_t i;
    uint32_t sum_abs = 0;
    int32_t dc;
    uint16_t block_level;
    uint32_t smooth;

    if ((h == 0) || (samples == 0) || (count == 0U))
    {
        return;
    }

    dc = h->dc_q16;
    for (i = 0; i < count; i++)
    {
        int32_t x_q16 = ((int32_t)samples[i] << 16);
        int32_t err = x_q16 - dc;
        int32_t ac_q16;
        uint32_t abs_ac;

        dc += (err >> h->dc_track_shift);
        ac_q16 = x_q16 - dc;
        abs_ac = (ac_q16 < 0) ? (uint32_t)(-ac_q16) : (uint32_t)ac_q16;
        sum_abs += (abs_ac >> 16);
    }

    h->dc_q16 = dc;
    block_level = (uint16_t)(sum_abs / count);

    smooth = ((uint32_t)h->level * (32768U - h->ema_alpha_q15) + (uint32_t)block_level * h->ema_alpha_q15) >> 15;
    h->level = (uint16_t)smooth;
}

uint16_t MAX9814_GetLevel(const MAX9814_HandleTypeDef *h)
{
    if (h == 0)
    {
        return 0;
    }
    return h->level;
}

uint8_t MAX9814_GetLevelPercent(const MAX9814_HandleTypeDef *h)
{
    uint32_t p;
    if ((h == 0) || (h->adc_full_scale == 0U))
    {
        return 0;
    }
    p = ((uint32_t)h->level * 100U) / h->adc_full_scale;
    if (p > 100U)
    {
        p = 100U;
    }
    return (uint8_t)p;
}

int16_t MAX9814_GetLevelDb10(const MAX9814_HandleTypeDef *h, uint16_t ref_level)
{
    float ratio;
    float db;
    int32_t db10;

    if ((h == 0) || (h->level == 0U))
    {
        return -400;
    }

    if (ref_level == 0U)
    {
        ref_level = h->adc_full_scale;
        if (ref_level == 0U)
        {
            ref_level = 4095U;
        }
    }

    ratio = (float)h->level / (float)ref_level;
    if (ratio <= 0.0f)
    {
        return -400;
    }
    db = 20.0f * log10f(ratio);
    db10 = (int32_t)(db * 10.0f);

    if (db10 < -400)
    {
        db10 = -400;
    }
    if (db10 > 120)
    {
        db10 = 120;
    }
    return (int16_t)db10;
}
