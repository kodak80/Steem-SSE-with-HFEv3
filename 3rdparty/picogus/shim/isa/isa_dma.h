/*  Steem PicoGUS shim: replaces sw/isa/isa_dma.h.
    The Raven060 ISA bus has no DMA controller: a transfer the card starts
    is never acknowledged, exactly as on the real machine. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "../pg_shim.h"
#include "../hardware/irq.h"
typedef struct { int unused; } PIO_t;
typedef PIO_t *PIO;
extern PIO pio0, pio1;
typedef struct {
    bool invertMsb;
    uint32_t push_threshold;
} dma_inst_t;
static inline dma_inst_t DMA_init(PIO pio, uint sm, irq_handler_t isr) { (void)pio; (void)sm; (void)isr; dma_inst_t d = {false, 0}; return d; }
static inline dma_inst_t DMA_multi_init(PIO pio, uint sm, irq_handler_t isr) { return DMA_init(pio, sm, isr); }
static inline void DMA_Start_Write(dma_inst_t *d) { (void)d; }
static inline uint32_t DMA_Complete_Write(dma_inst_t *d) { (void)d; return 0; }
static inline void DMA_Cancel_Write(dma_inst_t *d) { (void)d; }
static inline void DMA_Multi_Start_Write(dma_inst_t *d, uint32_t bytes) { (void)d; (void)bytes; }
static inline void DMA_Multi_Cancel(dma_inst_t *d) { (void)d; }
static inline void DMA_Multi_Set_Push_Threshold(dma_inst_t *d, uint32_t t) { d->push_threshold = t; }
