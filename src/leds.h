/* LED engine: two software-PWM groups on separate hardware timers plus a twinkle animator. */
#ifndef LEDS_H
#define LEDS_H

#include <stdint.h>
#include "stm8l101.h"

/* Pin masks on port B (all push-pull, active LOW) */
#define STAR_R      (1 << 3)
#define STAR_G      (1 << 4)
#define STAR_B      (1 << 5)
#define BR_R        (1 << 0)
#define BR_G        (1 << 1)
#define BR_B        (1 << 2)
#define LED_MASK    (STAR_R | STAR_G | STAR_B | BR_R | BR_G | BR_B)

#define PWM_STEPS   64          /* duty 0..64, 64 = fully on */

#define PAT_SEQUENTIAL  0x01    /* walk the palette in order instead of random picks */

typedef struct {
    const uint8_t *palette;     /* RGB triples, 0..255 */
    uint8_t ncolors;
    uint8_t bri_min, bri_max;   /* brightness scale 0..255 applied to the palette colour */
    uint8_t spd_min, spd_max;   /* fade speed 1..255; cur moves spd/8 levels per 10 ms tick */
    uint8_t hold_min, hold_max; /* ticks (10 ms) to hold at the target before a new pick */
    uint8_t flags;
} pattern_t;

typedef struct {
    uint16_t cur[3];            /* current level, 8.8 fixed point */
    uint16_t tgt[3];            /* target level, 8.8 */
    uint16_t step[3];           /* per-tick step, 8.8 */
    uint8_t  hold;
    uint8_t  idx;               /* last palette index */
    const pattern_t *pat;
} group_t;

extern group_t star, branches;

/* Duty cycles consumed by the timer ISRs (0..PWM_STEPS) */
extern volatile uint8_t star_duty_r, star_duty_g, star_duty_b;
extern volatile uint8_t br_duty_r, br_duty_g, br_duty_b;

void leds_init(void);                     /* GPIO + TIM2/TIM3 setup, timers stopped */
void leds_start(void);                    /* enable the two PWM timers */
void leds_stop(void);                     /* stop timers, all LEDs off */
void group_set_pattern(group_t *g, const pattern_t *p);
void group_tick(group_t *g);              /* call every 10 ms */
void leds_push_duties(void);              /* convert group levels to PWM duties */

uint8_t rnd8(void);

/* ISRs live in leds.c; prototypes must be visible from main.c for the vector table */
void tim2_isr(void) __interrupt(IRQ_TIM2_UPD);
void tim3_isr(void) __interrupt(IRQ_TIM3_UPD);

#endif
