/* Steem PicoGUS shim: hardware/gpio.h - only the IRQ pin matters */
#pragma once
#include "../pico/platform.h"
#ifndef PICO_DEFAULT_LED_PIN
#define PICO_DEFAULT_LED_PIN 25
#endif
#define PG_SHIM_IRQ_PIN 21
static inline void gpio_put(uint pin, bool value) {
    if (pin == PG_SHIM_IRQ_PIN) pg_shim_irq_line = value;
}
static inline bool gpio_get(uint pin) {
    return (pin == PG_SHIM_IRQ_PIN) ? pg_shim_irq_line : false;
}
static inline void gpio_xor_mask(uint32_t mask) { (void)mask; } /* activity LED */
