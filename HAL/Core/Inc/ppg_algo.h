#ifndef __PPG_ALGO_H
#define __PPG_ALGO_H

#include <stdint.h>

typedef enum
{
    PPG_SQI_BAD = 0,
    PPG_SQI_MID,
    PPG_SQI_GOOD
} PPG_Sqi_t;

typedef struct
{
    uint8_t hr_valid;
    uint8_t spo2_valid;
    uint16_t hr_bpm;
    uint8_t spo2;
    PPG_Sqi_t sqi;
} PPG_Result_t;

void PPG_Init(void);
void PPG_PushSample(uint32_t red, uint32_t ir);
void PPG_Update1s(void);
PPG_Result_t PPG_GetResult(void);

#endif
