/*  Steem PicoGUS shim: the RP2040 interpolator, clamp mode only (lane 0).
    The GUS firmware builds with INTERP_CLAMP: clamp16() is an interp1
    configured by clamp_setup() to shift, sign extend and clamp. C++ only. */
#pragma once
#include <stddef.h>
#include "../pico/platform.h"

typedef struct {
    uint32_t shift, mask_lsb, mask_msb;
    bool is_signed, clamp, blend;
} interp_config;

static inline interp_config interp_default_config(void) {
    interp_config c = {0, 0, 31, false, false, false};
    return c;
}
static inline void interp_config_set_clamp(interp_config *c, bool v) { c->clamp = v; }
static inline void interp_config_set_blend(interp_config *c, bool v) { c->blend = v; }
static inline void interp_config_set_shift(interp_config *c, uint32_t v) { c->shift = v; }
static inline void interp_config_set_signed(interp_config *c, bool v) { c->is_signed = v; }
static inline void interp_config_set_mask(interp_config *c, uint32_t lsb, uint32_t msb) { c->mask_lsb = lsb; c->mask_msb = msb; }

struct pg_interp_hw {
    int32_t accum[2];
    int32_t base[3];
    interp_config ctrl[2];
    struct peek_t {
        int32_t operator[](int lane) const {
            const pg_interp_hw *p = (const pg_interp_hw *)((const char *)this - offsetof(pg_interp_hw, peek));
            const interp_config &c = p->ctrl[lane & 1];
            uint32_t v = (uint32_t)p->accum[lane & 1] >> c.shift;
            uint32_t width = c.mask_msb + 1;
            uint32_t mask = (width >= 32 ? 0xFFFFFFFFu : ((1u << width) - 1)) & ~((1u << c.mask_lsb) - 1);
            v &= mask;
            int32_t r = (int32_t)v;
            if (c.is_signed && width < 32 && (v & (1u << c.mask_msb)))
                r = (int32_t)(v | ~((1u << width) - 1));
            if (c.clamp && lane == 0)
                r = r < p->base[0] ? p->base[0] : (r > p->base[1] ? p->base[1] : r);
            return r;
        }
    } peek;
};
typedef pg_interp_hw interp_hw_t;
extern pg_interp_hw pg_interp0, pg_interp1;
#define interp0 (&pg_interp0)
#define interp1 (&pg_interp1)
static inline void interp_set_config(pg_interp_hw *i, uint lane, interp_config *c) { i->ctrl[lane & 1] = *c; }
