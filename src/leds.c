/*
 * LED engine for the STM8L101 Christmas tree.
 *
 * Two independent software-PWM engines, each driven by its own hardware timer:
 *   TIM2 update interrupt  -> star     (PB3 red, PB4 green, PB5 blue)
 *   TIM3 update interrupt  -> branches (PB0 red, PB1 green, PB2 blue)
 * Each ISR runs at 12.8 kHz and chops its three channels with a 64-step duty cycle,
 * giving a 200 Hz PWM frame per group.  Duty values are latched at the start of each
 * frame so the animator can update them at any time without tearing.
 *
 * On top of that, a 100 Hz animator (group_tick) fades each colour channel toward a
 * randomly chosen target with its own random speed.  Because the three channels of a
 * group reach their targets at different times, colours overlap and blend on the way,
 * which is what gives the twinkle / shimmer effect.
 */
#include "leds.h"

group_t star, branches;

volatile uint8_t star_duty_r, star_duty_g, star_duty_b;
volatile uint8_t br_duty_r, br_duty_g, br_duty_b;

/* ------------------------------------------------------------------ PWM ISRs */

/* Star: PB3/PB4/PB5. Logic 0 = LED on. */
void tim2_isr(void) __interrupt(IRQ_TIM2_UPD)
{
    static uint8_t t;
    static uint8_t dr, dg, db;

    TIM2_SR1 &= (uint8_t)~TIM_SR1_UIF;

    if (++t >= PWM_STEPS)
        t = 0;

    if (t == 0) {                       /* start of frame: latch and switch on */
        dr = star_duty_r;
        dg = star_duty_g;
        db = star_duty_b;
        if (dr) PB_ODR &= (uint8_t)~STAR_R;
        if (dg) PB_ODR &= (uint8_t)~STAR_G;
        if (db) PB_ODR &= (uint8_t)~STAR_B;
    } else {                            /* switch each channel off at its duty point */
        if (t == dr) PB_ODR |= STAR_R;
        if (t == dg) PB_ODR |= STAR_G;
        if (t == db) PB_ODR |= STAR_B;
    }
}

/* Branches: PB0/PB1/PB2. */
void tim3_isr(void) __interrupt(IRQ_TIM3_UPD)
{
    static uint8_t t;
    static uint8_t dr, dg, db;

    TIM3_SR1 &= (uint8_t)~TIM_SR1_UIF;

    if (++t >= PWM_STEPS)
        t = 0;

    if (t == 0) {
        dr = br_duty_r;
        dg = br_duty_g;
        db = br_duty_b;
        if (dr) PB_ODR &= (uint8_t)~BR_R;
        if (dg) PB_ODR &= (uint8_t)~BR_G;
        if (db) PB_ODR &= (uint8_t)~BR_B;
    } else {
        if (t == dr) PB_ODR |= BR_R;
        if (t == dg) PB_ODR |= BR_G;
        if (t == db) PB_ODR |= BR_B;
    }
}

/* ------------------------------------------------------------------ hardware */

/* 16 MHz / 1250 = 12.8 kHz tick; 64 ticks = 200 Hz PWM frame */
#define PWM_ARR     (1250u - 1u)

void leds_init(void)
{
    PB_ODR |= LED_MASK;                 /* all off before enabling outputs */
    PB_DDR |= LED_MASK;                 /* outputs */
    PB_CR1 |= LED_MASK;                 /* push-pull */
    PB_CR2 &= (uint8_t)~LED_MASK;       /* 2 MHz slew is plenty */

    CLK_PCKENR |= CLK_PCKEN_TIM2 | CLK_PCKEN_TIM3;

    TIM2_CR1  = 0;
    TIM2_PSCR = 0;                      /* /1 */
    TIM2_ARRH = (uint8_t)(PWM_ARR >> 8);
    TIM2_ARRL = (uint8_t)(PWM_ARR & 0xFF);
    TIM2_EGR  = TIM_EGR_UG;             /* load prescaler/ARR */
    TIM2_SR1  = 0;
    TIM2_IER  = TIM_IER_UIE;

    TIM3_CR1  = 0;
    TIM3_PSCR = 0;
    TIM3_ARRH = (uint8_t)(PWM_ARR >> 8);
    TIM3_ARRL = (uint8_t)(PWM_ARR & 0xFF);
    TIM3_EGR  = TIM_EGR_UG;
    TIM3_SR1  = 0;
    TIM3_IER  = TIM_IER_UIE;
}

void leds_start(void)
{
    CLK_PCKENR |= CLK_PCKEN_TIM2 | CLK_PCKEN_TIM3;
    TIM2_CR1 |= TIM_CR1_CEN;
    TIM3_CR1 |= TIM_CR1_CEN;
}

void leds_stop(void)
{
    TIM2_CR1 &= (uint8_t)~TIM_CR1_CEN;
    TIM3_CR1 &= (uint8_t)~TIM_CR1_CEN;
    CLK_PCKENR &= (uint8_t)~(CLK_PCKEN_TIM2 | CLK_PCKEN_TIM3);
    star_duty_r = star_duty_g = star_duty_b = 0;
    br_duty_r = br_duty_g = br_duty_b = 0;
    PB_ODR |= LED_MASK;                 /* logic 1 = off */
}

/* ------------------------------------------------------------------ animator */

static uint16_t rng = 0xACE1u;

uint8_t rnd8(void)
{
    /* 16-bit xorshift */
    rng ^= rng << 7;
    rng ^= rng >> 9;
    rng ^= rng << 8;
    return (uint8_t)rng;
}

static uint8_t rnd_range(uint8_t lo, uint8_t hi)
{
    if (hi <= lo)
        return lo;
    return lo + (uint8_t)(rnd8() % (uint8_t)(hi - lo + 1));
}

static void group_pick(group_t *g)
{
    const pattern_t *p = g->pat;
    const uint8_t *c;
    uint8_t bri, i;

    if (p->ncolors > 1) {
        if (p->flags & PAT_SEQUENTIAL) {
            if (++g->idx >= p->ncolors)
                g->idx = 0;
        } else {
            uint8_t n;
            do {
                n = rnd8() % p->ncolors;
            } while (n == g->idx);      /* always change colour */
            g->idx = n;
        }
    } else {
        g->idx = 0;
    }
    c   = p->palette + (uint8_t)(g->idx * 3);
    bri = rnd_range(p->bri_min, p->bri_max);

    for (i = 0; i < 3; i++) {
        /* target = colour * brightness, kept in 8.8 */
        g->tgt[i]  = (uint16_t)c[i] * bri;
        /* independent speed per channel so colours blend on the way */
        g->step[i] = (uint16_t)rnd_range(p->spd_min, p->spd_max) << 5;
    }
    g->hold = rnd_range(p->hold_min, p->hold_max);
}

void group_set_pattern(group_t *g, const pattern_t *p)
{
    g->pat = p;
    g->idx = 0xFF;
    group_pick(g);
}

void group_tick(group_t *g)
{
    uint8_t i, done = 1;

    for (i = 0; i < 3; i++) {
        uint16_t cur = g->cur[i], tgt = g->tgt[i], st = g->step[i];
        if (cur < tgt) {
            cur = (tgt - cur > st) ? cur + st : tgt;
            done = 0;
        } else if (cur > tgt) {
            cur = (cur - tgt > st) ? cur - st : tgt;
            done = 0;
        }
        g->cur[i] = cur;
    }

    if (done) {
        if (g->hold)
            g->hold--;
        else
            group_pick(g);
    }
}

/* 8-bit perceptual level -> 0..64 duty, quadratic gamma. 255 maps to 64 (always on). */
static uint8_t gamma_duty(uint16_t lvl88)
{
    uint16_t l = lvl88 >> 8;
    return (uint8_t)(((l * l) + 1023u) >> 10);
}

void leds_push_duties(void)
{
    star_duty_r = gamma_duty(star.cur[0]);
    star_duty_g = gamma_duty(star.cur[1]);
    star_duty_b = gamma_duty(star.cur[2]);
    br_duty_r   = gamma_duty(branches.cur[0]);
    br_duty_g   = gamma_duty(branches.cur[1]);
    br_duty_b   = gamma_duty(branches.cur[2]);
}
