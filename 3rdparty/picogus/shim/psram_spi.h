/* Steem PicoGUS shim: PSRAM (GUS DRAM) as a plain 1MB array */
#pragma once
#include "pg_shim.h"
#include <string.h>
typedef struct { int unused; } psram_spi_inst_t;
#define PG_PSRAM_MASK (PG_SHIM_PSRAM_SIZE-1)
static inline uint8_t psram_read8(psram_spi_inst_t *s, uint32_t a) { (void)s; return pg_shim_psram[a & PG_PSRAM_MASK]; }
static inline uint8_t psram_read8_xcore(psram_spi_inst_t *s, uint32_t a) { return psram_read8(s, a); }
/* little endian, as on the RP2040 */
static inline uint16_t psram_read16(psram_spi_inst_t *s, uint32_t a) {
    (void)s; return (uint16_t)(pg_shim_psram[a & PG_PSRAM_MASK] | (pg_shim_psram[(a + 1) & PG_PSRAM_MASK] << 8));
}
static inline uint32_t psram_read32(psram_spi_inst_t *s, uint32_t a) {
    return (uint32_t)psram_read16(s, a) | ((uint32_t)psram_read16(s, a + 2) << 16);
}
static inline void psram_read(psram_spi_inst_t *s, uint32_t a, uint8_t *dst, size_t n) {
    for (size_t i = 0; i < n; i++) dst[i] = psram_read8(s, (uint32_t)(a + i));
}
static inline void psram_write8(psram_spi_inst_t *s, uint32_t a, uint8_t v) { (void)s; pg_shim_psram[a & PG_PSRAM_MASK] = v; }
static inline void psram_write8_async(psram_spi_inst_t *s, uint32_t a, uint8_t v) { psram_write8(s, a, v); }
static inline void psram_write8_xcore(psram_spi_inst_t *s, uint32_t a, uint8_t v) { psram_write8(s, a, v); }
static inline void psram_write32(psram_spi_inst_t *s, uint32_t a, uint32_t v) {
    for (int i = 0; i < 4; i++) psram_write8(s, a + i, (uint8_t)(v >> (8 * i)));
}
static inline void psram_write32_async(psram_spi_inst_t *s, uint32_t a, uint32_t v) { psram_write32(s, a, v); }
