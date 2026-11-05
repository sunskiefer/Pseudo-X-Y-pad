/* XYZ (L'Cronx): a free X/Y pad for the Akai Force / MPC OS. X and Y (-5 .. +5) are automatable and mappable to Q-Links; the pad
 * itself is drawn and read by common/xyz_pad.c. Test sound: a sine through a resonant low-pass filter.
 *   x (pad horizontal, vertical-drag knob)   = filter cutoff, 20 Hz .. 20 kHz (exponential)
 *   y (pad vertical, horizontal-drag knob)   = filter resonance, Q 0.71 .. 5
 *   circ (circular-drag knob, 0 .. 1)        = sine pitch, 55 Hz .. 880 Hz (4 octaves, exponential)
 *   vol  (vertical-drag knob, 0 .. 1)        = output volume (gain 0 .. 2; 0.5 is the level of the first test build) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "xyz_pad.h"

typedef struct {
    volatile float x, y, circ, vol;
    float phase, fc, res;        /* sine phase (0..1), smoothed cutoff (Hz) and resonance (0..1) */
    float ic1, ic2;              /* filter state */
    volatile unsigned changed;   /* set by the pad's thread when a finger moved X (bit 0) or Y (bit 1) */
    unsigned reported;           /* the bits the wrapper still has to be told about (engine thread only) */
    overlay_t pad;
} xyz_t;

static void *xyz_create(const char *data_dir) {
    xyz_t *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->circ = 0.5f;
    s->vol = 0.5f;
    s->fc = 632.0f; s->res = 0.5f;
    pad_start(&s->pad, data_dir, &s->x, &s->y, &s->changed);
    return s;
}
static void xyz_destroy(void *p) {
    xyz_t *s = p;
    if (!s) return;
    pad_stop(&s->pad);
    free(s);
}
static void xyz_midi(void *p, const uint8_t *msg, int len) { (void)p; (void)msg; (void)len; }

static float clampv(float v) { return v < -5.0f ? -5.0f : v > 5.0f ? 5.0f : v; }

static void xyz_set_param(void *p, const char *key, const char *val) {
    xyz_t *s = p;
    if (!strcmp(key, "x")) s->x = clampv((float)atof(val));
    else if (!strcmp(key, "y")) s->y = clampv((float)atof(val));
    else if (!strcmp(key, "circ")) { float v = (float)atof(val); s->circ = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
    else if (!strcmp(key, "vol")) { float v = (float)atof(val); s->vol = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
}

/* 2^x without libm's powf/exp2f: their 2.27+ symbol versions are newer than the Force's glibc (max 2.17 allowed) */
static float my_exp2(float x) {
    if (x < -30.0f) x = -30.0f; else if (x > 30.0f) x = 30.0f;
    float fl = floorf(x), f = (x - fl) * 0.69314718056f;
    float p = 1.0f + f * (1.0f + f * (0.5f + f * (1.0f / 6 + f * (1.0f / 24 + f * (1.0f / 120 + f * (1.0f / 720 + f * (1.0f / 5040)))))));
    return ldexpf(p, (int)fl);
}
static float cutoff_hz(float x) { return 20.0f * my_exp2((x + 5.0f) * 0.99657843f); }   /* 20 Hz * 1000^((x+5)/10) */
static float pitch_hz(float c) { return 55.0f * my_exp2(c * 4.0f); }

static int xyz_get_param(void *p, const char *key, char *buf, int len) {
    xyz_t *s = p;
    if (!strcmp(key, "x")) return snprintf(buf, (size_t)len, "%g", (double)s->x);
    if (!strcmp(key, "y")) return snprintf(buf, (size_t)len, "%g", (double)s->y);
    if (!strcmp(key, "circ")) return snprintf(buf, (size_t)len, "%g", (double)s->circ);
    if (!strcmp(key, "vol")) return snprintf(buf, (size_t)len, "%g", (double)s->vol);
    if (!strcmp(key, "vol_display")) return snprintf(buf, (size_t)len, "%.0f %%", (double)(s->vol * 100.0f));
    if (!strcmp(key, "x_display")) {
        float f = cutoff_hz(s->x);
        return f >= 1000.0f ? snprintf(buf, (size_t)len, "%.1f kHz", (double)(f / 1000.0f)) : snprintf(buf, (size_t)len, "%.0f Hz", (double)f);
    }
    if (!strcmp(key, "y_display")) return snprintf(buf, (size_t)len, "%.0f %%", (double)((s->y + 5.0f) * 10.0f));
    if (!strcmp(key, "circ_display")) return snprintf(buf, (size_t)len, "%.0f Hz", (double)pitch_hz(s->circ));
    return 0;
}

/* a sine (level 0.2 times the volume knob's gain; at full volume and high resonance the peak can reach the clip) through a zero-delay-feedback state variable low-pass;
 * cutoff and resonance are smoothed and the filter coefficients are set every 16 samples */
static void xyz_render(void *p, int16_t *out, int frames) {
    xyz_t *s = p;
    const float fs = 44100.0f, TWO_PI = 6.28318530718f;
    float gain = s->vol * 2.0f * 32767.0f;
    float fc_t = cutoff_hz(s->x), res_t = (s->y + 5.0f) * 0.1f, inc = pitch_hz(s->circ) / fs;
    int i = 0;
    while (i < frames) {
        int n = frames - i < 16 ? frames - i : 16;
        s->fc += (fc_t - s->fc) * 0.15f;
        s->res += (res_t - s->res) * 0.15f;
        float fc = s->fc > 19000.0f ? 19000.0f : s->fc < 20.0f ? 20.0f : s->fc;
        float k = 1.4f - 1.2f * s->res;                      /* damping 1/Q: Q 0.71 .. 5 */
        float g = tanf(3.14159265359f * fc / fs);
        float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        for (int j = 0; j < n; j++, i++) {
            float v0 = 0.2f * sinf(TWO_PI * s->phase);
            s->phase += inc; if (s->phase >= 1.0f) s->phase -= 1.0f;
            float v3 = v0 - s->ic2;
            float v1 = a1 * s->ic1 + a2 * v3;
            float v2 = s->ic2 + a2 * s->ic1 + a3 * v3;
            s->ic1 = 2.0f * v1 - s->ic1;
            s->ic2 = 2.0f * v2 - s->ic2;
            float o = v2 * gain;
            int16_t q = o > 32767.0f ? 32767 : o < -32768.0f ? -32768 : (int16_t)o;
            out[2 * i] = q; out[2 * i + 1] = q;
        }
    }
}

/* the wrapper asks once per host block which parameters the engine changed by itself: the ones a finger moved on the pad */
int mpc_engine_changed(void *p, char *key, int len) {
    xyz_t *s = p;
    if (!s) return 0;
    s->reported |= __atomic_exchange_n(&s->changed, 0u, __ATOMIC_ACQUIRE);
    if (s->reported & 1u) { s->reported &= ~1u; snprintf(key, (size_t)len, "x"); return 1; }
    if (s->reported & 2u) { s->reported &= ~2u; snprintf(key, (size_t)len, "y"); return 1; }
    return 0;
}

static const mpc_engine_t ENGINE = {xyz_create, xyz_destroy, xyz_midi, xyz_set_param, xyz_get_param, xyz_render, NULL, NULL};
const mpc_engine_t *mpc_engine(void) { return &ENGINE; }
