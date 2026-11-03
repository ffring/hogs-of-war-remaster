#ifndef HOGS_LINEAR_AIM_POLICY_H
#define HOGS_LINEAR_AIM_POLICY_H

#include <stdint.h>

enum { HML_AXIS_MAX = 120, HML_AIM_UNITS_MAX = 60 };

typedef struct {
    int32_t frac_x100_x;
    int32_t frac_x100_y;
    int32_t sensitivity_percent;
} HMLAimScale;

typedef struct { int8_t x, y; } HMLAxisPacket;

static inline int32_t hml_clamp_percent(int32_t percent) {
    if (percent < 25) return 25;
    if (percent > 1000) return 1000;
    return percent;
}

static inline int32_t hml_axis_angle(int32_t pixels, int32_t *fraction,
                                     int32_t percent) {
    if (!fraction) return 0;
    if ((pixels > 0 && *fraction < 0) || (pixels < 0 && *fraction > 0))
        *fraction = 0;
    int64_t total = (int64_t)pixels * hml_clamp_percent(percent) + *fraction;
    int64_t whole = total / 100;
    int64_t remainder = total % 100;
    if (whole > HML_AIM_UNITS_MAX) { *fraction = 0; return HML_AIM_UNITS_MAX; }
    if (whole < -HML_AIM_UNITS_MAX) { *fraction = 0; return -HML_AIM_UNITS_MAX; }
    *fraction = (int32_t)remainder;
    return (int32_t)whole;
}

static inline HMLAxisPacket hml_scale_mouse_delta(HMLAimScale *scale,
                                                   int32_t dx, int32_t dy,
                                                   int invert_x, int invert_y) {
    HMLAxisPacket out = {0, 0};
    if (!scale) return out;
    scale->sensitivity_percent = hml_clamp_percent(scale->sensitivity_percent);
    int32_t x = hml_axis_angle(dx, &scale->frac_x100_x, scale->sensitivity_percent);
    int32_t y = hml_axis_angle(dy, &scale->frac_x100_y, scale->sensitivity_percent);
    if (invert_x) x = -x;
    if (invert_y) y = -y;
    if (x > HML_AIM_UNITS_MAX) x = HML_AIM_UNITS_MAX;
    if (x < -HML_AIM_UNITS_MAX) x = -HML_AIM_UNITS_MAX;
    if (y > HML_AIM_UNITS_MAX) y = HML_AIM_UNITS_MAX;
    if (y < -HML_AIM_UNITS_MAX) y = -HML_AIM_UNITS_MAX;
    /* Retail divides each synchronized signed axis byte by two. Encode two
       wire units per requested integer angle unit, up to +/-60 per call. */
    out.x = (int8_t)(x * 2);
    out.y = (int8_t)(y * 2);
    return out;
}

static inline uint32_t hml_pack(HMLAxisPacket p) {
    return (uint32_t)(uint8_t)p.x | ((uint32_t)(uint8_t)p.y << 8);
}

static inline int32_t hml_retail_half(int8_t value) {
    return value >= 0 ? value / 2 : -((-((int32_t)value)) / 2);
}

#endif
