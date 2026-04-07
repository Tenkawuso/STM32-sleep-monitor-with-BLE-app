#include "ppg_algo.h"

#define PPG_FS_HZ                  100U
#define PPG_BUF_LEN                800U
#define PPG_HR_MIN_BPM             45U
#define PPG_HR_MAX_BPM             180U
#define PPG_HR_MIN_INTERVAL_MS     360U
#define PPG_HR_MAX_INTERVAL_MS     1400U
#define PPG_SPO2_WINDOW            400U
#define PPG_PEAK_BASE_THRESHOLD    220U

static uint32_t s_red_buf[PPG_BUF_LEN];
static uint32_t s_ir_buf[PPG_BUF_LEN];
static uint16_t s_write_idx;
static uint16_t s_count;
static uint32_t s_sample_total;

static int32_t s_ir_dc;
static int32_t s_red_dc;
static int32_t s_ir_prev2;
static int32_t s_ir_prev1;
static int32_t s_last_peak;
static uint32_t s_ir_abs_ema;
static uint8_t s_have_prev;

static uint16_t s_peak_intervals[8];
static uint8_t s_peak_count;
static uint32_t s_last_peak_sample;

static uint8_t s_last_second_peaks;
static PPG_Result_t s_result;

static uint16_t ppg_abs_i32(int32_t x)
{
    if (x < 0)
    {
        x = -x;
    }
    if (x > 32767)
    {
        x = 32767;
    }
    return (uint16_t)x;
}

static uint16_t ppg_mid_of_array(uint16_t *v, uint8_t n)
{
    uint8_t i;
    uint8_t j;

    for (i = 0; i < n; i++)
    {
        for (j = (uint8_t)(i + 1U); j < n; j++)
        {
            if (v[j] < v[i])
            {
                uint16_t t = v[i];
                v[i] = v[j];
                v[j] = t;
            }
        }
    }

    return v[n / 2U];
}

static void ppg_update_hr(void)
{
    uint8_t n = s_peak_count;
    uint16_t tmp[8];
    uint8_t i;
    uint16_t med_ms;
    uint32_t bpm;

    if (n < 3U)
    {
        s_result.hr_valid = 0U;
        return;
    }

    for (i = 0U; i < n; i++)
    {
        tmp[i] = s_peak_intervals[i];
    }

    med_ms = ppg_mid_of_array(tmp, n);
    if ((med_ms < PPG_HR_MIN_INTERVAL_MS) || (med_ms > PPG_HR_MAX_INTERVAL_MS))
    {
        s_result.hr_valid = 0U;
        return;
    }

    bpm = 60000UL / med_ms;
    if ((bpm < PPG_HR_MIN_BPM) || (bpm > PPG_HR_MAX_BPM))
    {
        s_result.hr_valid = 0U;
        return;
    }

    s_result.hr_bpm = (uint16_t)bpm;
    s_result.hr_valid = 1U;
}

static void ppg_update_spo2(void)
{
    uint16_t n;
    uint16_t i;
    uint16_t start;
    uint64_t sum_red = 0ULL;
    uint64_t sum_ir = 0ULL;
    uint64_t sum_abs_red = 0ULL;
    uint64_t sum_abs_ir = 0ULL;
    uint32_t dc_red;
    uint32_t dc_ir;
    uint32_t ac_red;
    uint32_t ac_ir;
    uint32_t ratio_q10;
    int32_t spo2;

    if (s_count < PPG_SPO2_WINDOW)
    {
        s_result.spo2_valid = 0U;
        return;
    }

    n = PPG_SPO2_WINDOW;
    start = (uint16_t)((s_write_idx + PPG_BUF_LEN - n) % PPG_BUF_LEN);

    for (i = 0U; i < n; i++)
    {
        uint16_t idx = (uint16_t)((start + i) % PPG_BUF_LEN);
        sum_red += s_red_buf[idx];
        sum_ir += s_ir_buf[idx];
    }

    dc_red = (uint32_t)(sum_red / n);
    dc_ir = (uint32_t)(sum_ir / n);

    if ((dc_red < 1000U) || (dc_ir < 1000U))
    {
        s_result.spo2_valid = 0U;
        return;
    }

    for (i = 0U; i < n; i++)
    {
        uint16_t idx = (uint16_t)((start + i) % PPG_BUF_LEN);
        int32_t dr = (int32_t)s_red_buf[idx] - (int32_t)dc_red;
        int32_t di = (int32_t)s_ir_buf[idx] - (int32_t)dc_ir;
        if (dr < 0)
        {
            dr = -dr;
        }
        if (di < 0)
        {
            di = -di;
        }
        sum_abs_red += (uint32_t)dr;
        sum_abs_ir += (uint32_t)di;
    }

    ac_red = (uint32_t)(sum_abs_red / n);
    ac_ir = (uint32_t)(sum_abs_ir / n);
    if ((ac_red < 10U) || (ac_ir < 10U))
    {
        s_result.spo2_valid = 0U;
        return;
    }

    ratio_q10 = (uint32_t)(((uint64_t)ac_red * dc_ir * 1024ULL) / ((uint64_t)ac_ir * dc_red));
    if (ratio_q10 < 410U)
    {
        ratio_q10 = 410U;
    }
    else if (ratio_q10 > 1330U)
    {
        ratio_q10 = 1330U;
    }

    spo2 = 110 - (20 * (int32_t)ratio_q10) / 1024;

    if (spo2 < 85)
    {
        spo2 = 85;
    }
    if (spo2 > 100)
    {
        spo2 = 100;
    }

    if (s_result.spo2_valid == 0U)
    {
        s_result.spo2 = (uint8_t)spo2;
    }
    else
    {
        s_result.spo2 = (uint8_t)((3U * s_result.spo2 + (uint8_t)spo2) / 4U);
    }
    s_result.spo2_valid = 1U;
}

static void ppg_update_sqi(void)
{
    uint8_t peak_score;
    uint16_t amp = ppg_abs_i32(s_last_peak);

    if (s_last_second_peaks >= 2U)
    {
        peak_score = 2U;
    }
    else if (s_last_second_peaks >= 1U)
    {
        peak_score = 1U;
    }
    else
    {
        peak_score = 0U;
    }

    if ((amp > 800U) && (peak_score >= 1U))
    {
        s_result.sqi = PPG_SQI_GOOD;
    }
    else if ((amp > 300U) && (peak_score >= 1U))
    {
        s_result.sqi = PPG_SQI_MID;
    }
    else
    {
        s_result.sqi = PPG_SQI_BAD;
    }

    if ((s_result.sqi == PPG_SQI_BAD) && ((s_sample_total - s_last_peak_sample) > (4U * PPG_FS_HZ)))
    {
        s_result.hr_valid = 0U;
        s_result.spo2_valid = 0U;
    }
}

void PPG_Init(void)
{
    uint16_t i;
    for (i = 0U; i < PPG_BUF_LEN; i++)
    {
        s_red_buf[i] = 0U;
        s_ir_buf[i] = 0U;
    }
    s_write_idx = 0U;
    s_count = 0U;
    s_sample_total = 0U;
    s_ir_dc = 0;
    s_red_dc = 0;
    s_ir_prev2 = 0;
    s_ir_prev1 = 0;
    s_last_peak = 0;
    s_ir_abs_ema = 0U;
    s_have_prev = 0U;
    s_peak_count = 0U;
    s_last_peak_sample = 0U;
    s_last_second_peaks = 0U;
    s_result.hr_valid = 0U;
    s_result.spo2_valid = 0U;
    s_result.hr_bpm = 0U;
    s_result.spo2 = 0U;
    s_result.sqi = PPG_SQI_BAD;
}

void PPG_PushSample(uint32_t red, uint32_t ir)
{
    int32_t ir_ac;
    int32_t red_ac;
    uint32_t abs_ir;
    int32_t peak_threshold;
    uint32_t min_interval_samples;
    uint32_t delta_samples;

    s_red_buf[s_write_idx] = red;
    s_ir_buf[s_write_idx] = ir;
    s_write_idx = (uint16_t)((s_write_idx + 1U) % PPG_BUF_LEN);
    if (s_count < PPG_BUF_LEN)
    {
        s_count++;
    }
    s_sample_total++;

    if (s_have_prev == 0U)
    {
        s_ir_dc = (int32_t)ir;
        s_red_dc = (int32_t)red;
        s_have_prev = 1U;
        return;
    }

    s_ir_dc += (((int32_t)ir - s_ir_dc) >> 4);
    s_red_dc += (((int32_t)red - s_red_dc) >> 4);
    ir_ac = (int32_t)ir - s_ir_dc;
    red_ac = (int32_t)red - s_red_dc;
    (void)red_ac;

    abs_ir = (uint32_t)ppg_abs_i32(ir_ac);
    if (s_ir_abs_ema == 0U)
    {
        s_ir_abs_ema = abs_ir;
    }
    else
    {
        s_ir_abs_ema = (7U * s_ir_abs_ema + abs_ir) / 8U;
    }

    min_interval_samples = (PPG_HR_MIN_INTERVAL_MS * PPG_FS_HZ) / 1000U;
    delta_samples = s_sample_total - s_last_peak_sample;
    peak_threshold = (int32_t)(s_ir_abs_ema + 120U);
    if (peak_threshold < (int32_t)PPG_PEAK_BASE_THRESHOLD)
    {
        peak_threshold = (int32_t)PPG_PEAK_BASE_THRESHOLD;
    }

    if ((s_ir_prev1 > s_ir_prev2) &&
        (s_ir_prev1 > ir_ac) &&
        ((s_ir_prev1 - s_ir_prev2) > 18) &&
        ((s_ir_prev1 - ir_ac) > 18) &&
        (s_ir_prev1 > peak_threshold))
    {
        if (delta_samples >= min_interval_samples)
        {
            uint16_t interval_ms;
            s_last_peak = s_ir_prev1;
            if (s_last_peak_sample > 0U)
            {
                uint32_t diff = s_sample_total - s_last_peak_sample;
                interval_ms = (uint16_t)((diff * 1000U) / PPG_FS_HZ);
                if ((interval_ms >= PPG_HR_MIN_INTERVAL_MS) && (interval_ms <= PPG_HR_MAX_INTERVAL_MS))
                {
                    if (s_peak_count < 8U)
                    {
                        s_peak_intervals[s_peak_count++] = interval_ms;
                    }
                    else
                    {
                        uint8_t i;
                        for (i = 1U; i < 8U; i++)
                        {
                            s_peak_intervals[i - 1U] = s_peak_intervals[i];
                        }
                        s_peak_intervals[7] = interval_ms;
                    }
                    s_last_second_peaks++;
                }
            }
            s_last_peak_sample = s_sample_total;
        }
    }

    s_ir_prev2 = s_ir_prev1;
    s_ir_prev1 = ir_ac;
}

void PPG_Update1s(void)
{
    ppg_update_hr();
    if ((s_sample_total - s_last_peak_sample) > (4U * PPG_FS_HZ))
    {
        s_result.hr_valid = 0U;
    }
    ppg_update_spo2();
    ppg_update_sqi();
    s_last_second_peaks = 0U;
}

PPG_Result_t PPG_GetResult(void)
{
    return s_result;
}
