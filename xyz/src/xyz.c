/* XYZ (L'Cronx): a free X/Y pad for the Akai Force / MPC OS. Two parameters, X and Y (-5 .. +5), both automatable and mappable to
 * Q-Links; the pad itself is drawn and read by common/xyz_pad.c. The plugin makes no sound: it is the control test. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "xyz_pad.h"

typedef struct {
    volatile float x, y;
    volatile unsigned changed;   /* set by the pad's thread when a finger moved X (bit 0) or Y (bit 1) */
    unsigned reported;           /* the bits the wrapper still has to be told about (engine thread only) */
    overlay_t pad;
} xyz_t;

static void *xyz_create(const char *data_dir) {
    xyz_t *s = calloc(1, sizeof *s);
    if (!s) return NULL;
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
}

static int xyz_get_param(void *p, const char *key, char *buf, int len) {
    xyz_t *s = p;
    if (!strcmp(key, "x")) return snprintf(buf, (size_t)len, "%g", (double)s->x);
    if (!strcmp(key, "y")) return snprintf(buf, (size_t)len, "%g", (double)s->y);
    if (!strcmp(key, "x_display")) return snprintf(buf, (size_t)len, "%+.2f", (double)s->x);
    if (!strcmp(key, "y_display")) return snprintf(buf, (size_t)len, "%+.2f", (double)s->y);
    return 0;
}

static void xyz_render(void *p, int16_t *out, int frames) { (void)p; memset(out, 0, (size_t)frames * 2 * sizeof(int16_t)); }

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
