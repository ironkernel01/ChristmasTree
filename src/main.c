/*
 * STM8L101F3P6 Christmas tree LED flasher.
 *
 *   PB5/PB4/PB3  star     blue/green/red   (push-pull, active low)
 *   PB2/PB1/PB0  branches blue/green/red   (push-pull, active low)
 *   PA2          button, active low, internal pull-up, EXTI2 wakes from Halt
 *
 * Timers:
 *   TIM2  star PWM engine        (leds.c)
 *   TIM3  branch PWM engine      (leds.c)
 *   TIM4  10 ms system tick: button debounce + animation pacing
 *
 * Button cycles through the modes below. A press is accepted on the falling edge
 * and then ignored for 1 s (debounce / repeat lockout).
 */
#include "stm8l101.h"
#include "leds.h"

#define BUTTON_PIN      (1 << 2)
#define DEBOUNCE_TICKS  100         /* 100 x 10 ms = 1 s */

/* -------------------------------------------------------------- colours */
static const uint8_t PAL_WHITE[]      = { 255, 255, 255 };
static const uint8_t PAL_GREEN[]      = {   0, 255,   0 };
static const uint8_t PAL_GREEN_RED[]  = {   0, 255,   0,  255,   0,   0 };
static const uint8_t PAL_WHITE_BLUE[] = { 255, 255, 255,    0,   0, 255,  100, 140, 255 };
static const uint8_t PAL_RAINBOW[]    = { 255,   0,   0,  255, 160,   0,    0, 255,   0,
                                            0, 255, 255,    0,   0, 255,  255,   0, 255 };
static const uint8_t PAL_CANDLE[]     = { 255,  70,   0,  255, 130,   0,  255,  30,   0 };
static const uint8_t PAL_GOLD[]       = { 255, 150,   0 };
static const uint8_t PAL_RGB[]        = { 255,   0,   0,    0, 255,   0,    0,   0, 255 };

/* -------------------------------------------------------------- patterns */
/*                       palette, n, bri lo/hi, speed lo/hi, hold lo/hi, flags */
static const pattern_t P_STAR_WHITE    = { PAL_WHITE,      1,  60, 255,  6, 40,   0,  20, 0 };
static const pattern_t P_BR_GREEN      = { PAL_GREEN,      1, 170, 255,  1,  3,  50, 200, 0 };
static const pattern_t P_BR_GREEN_RED  = { PAL_GREEN_RED,  2, 120, 255,  2, 10,  20, 120, 0 };
static const pattern_t P_WHITE_BLUE    = { PAL_WHITE_BLUE, 3,  90, 255,  3, 24,  10,  80, 0 };
static const pattern_t P_STAR_RAINBOW  = { PAL_RAINBOW,    6, 220, 255,  3,  3,   0,  10, PAT_SEQUENTIAL };
static const pattern_t P_BR_RAINBOW    = { PAL_RAINBOW,    6, 220, 255,  1,  1,  20,  20, PAT_SEQUENTIAL };
static const pattern_t P_STAR_GOLD     = { PAL_GOLD,       1, 150, 255,  2,  8,   0,  30, 0 };
static const pattern_t P_BR_CANDLE     = { PAL_CANDLE,     3,  60, 255, 16, 63,   0,   6, 0 };
static const pattern_t P_STAR_GOLD_STD = { PAL_GOLD,       1, 255, 255,  4,  4, 255, 255, 0 };
static const pattern_t P_BR_PARTY      = { PAL_RGB,        3, 200, 255, 40, 63,  10,  40, 0 };

typedef struct { const pattern_t *star, *branches; } mode_t;

/* Mode 0 is sleep; the table below holds modes 1..N */
static const mode_t MODES[] = {
    { &P_STAR_WHITE,    &P_BR_GREEN     },  /* 1: star twinkles white, branches green          */
    { &P_STAR_WHITE,    &P_BR_GREEN_RED },  /* 2: star white, branches twinkle green/red       */
    { &P_WHITE_BLUE,    &P_WHITE_BLUE   },  /* 3: star and branches white/blue                 */
    { &P_STAR_RAINBOW,  &P_BR_RAINBOW   },  /* 4: slow rainbow drift, star leads               */
    { &P_STAR_GOLD,     &P_BR_CANDLE    },  /* 5: gold star, candle-flame branches             */
    { &P_STAR_GOLD_STD, &P_BR_PARTY     },  /* 6: steady gold star, fast red/green/blue party  */
};
#define NUM_MODES   (1 + (uint8_t)(sizeof(MODES) / sizeof(MODES[0])))

/* -------------------------------------------------------------- shared state */
static volatile uint8_t tick_flag;      /* set every 10 ms by TIM4 */
static volatile uint8_t button_event;   /* set when a debounced press is accepted */
static volatile uint8_t button_lock;    /* lockout countdown in ticks */
static uint8_t mode;                    /* 0 = sleep */

/* -------------------------------------------------------------- ISRs */

/* 10 ms tick: debounce button, pace animation */
void tim4_isr(void) __interrupt(IRQ_TIM4_UPD)
{
    static uint8_t was_pressed;
    uint8_t pressed;

    TIM4_SR1 &= (uint8_t)~TIM_SR1_UIF;

    pressed = !(PA_IDR & BUTTON_PIN);

    if (button_lock)
        button_lock--;

    if (pressed && !was_pressed && button_lock == 0) {
        button_event = 1;
        button_lock  = DEBOUNCE_TICKS;
    }
    was_pressed = pressed;
    tick_flag = 1;
}

/* Button falling edge: only purpose is to wake the core from Halt */
void exti2_isr(void) __interrupt(IRQ_EXTI2)
{
    EXTI_SR1 = BUTTON_PIN;              /* write 1 clears P2F */
}

/* -------------------------------------------------------------- setup */

static void clock_init(void)
{
    CLK_CKDIVR = 0;                     /* HSI /1 = 16 MHz */
    CLK_PCKENR = 0;                     /* peripherals gated until needed */
}

static void gpio_init(void)
{
    /* Button: input, pull-up, external interrupt enabled, falling edge */
    PA_DDR &= (uint8_t)~BUTTON_PIN;
    PA_CR1 |= BUTTON_PIN;
    PA_CR2 |= BUTTON_PIN;
    EXTI_CR1 = (uint8_t)((EXTI_CR1 & (uint8_t)~0x30) | (uint8_t)(EXTI_SENS_FALL << 4));  /* P2IS */

    /* Unused pins: inputs with pull-up so nothing floats in Halt. PA0 is SWIM, left alone. */
    PA_CR1 |= 0xF8;
    PB_CR1 |= 0xC0;
    PC_DDR  = 0x00; PC_CR1 = 0xFF;
    PD_DDR  = 0x00; PD_CR1 = 0xFF;
}

static void tick_init(void)
{
    CLK_PCKENR |= CLK_PCKEN_TIM4;
    TIM4_CR1  = 0;
    TIM4_PSCR = 10;                     /* 16 MHz / 1024 = 15.625 kHz */
    TIM4_ARR  = (uint8_t)155;           /* /156 = 100.16 Hz */
    TIM4_EGR  = TIM_EGR_UG;
    TIM4_SR1  = 0;
    TIM4_IER  = TIM_IER_UIE;
    TIM4_CR1  = TIM_CR1_CEN;
}

static void apply_mode(uint8_t m)
{
    if (m == 0) {
        leds_stop();
        return;
    }
    group_set_pattern(&star,     MODES[m - 1].star);
    group_set_pattern(&branches, MODES[m - 1].branches);
    leds_push_duties();
    leds_start();
}

/* -------------------------------------------------------------- sleep */

/*
 * Mode 0: everything off, core in Halt (about 1 uA). The button's falling edge on
 * EXTI2 wakes the core; TIM4 then resumes sampling and the normal debounce logic
 * turns the press into a mode change. If a press lands between the idle check and
 * HALT, the pending EXTI request makes HALT return immediately, so no press is lost.
 */
static void sleep_until_button(void)
{
    disable_irq();
    if (button_lock == 0 && !button_event && (PA_IDR & BUTTON_PIN)) {
        PB_ODR |= LED_MASK;
        CLK_PCKENR = 0;                 /* stop TIM4 (and anything else) */
        halt();                         /* interrupts are re-enabled by HALT */
        CLK_PCKENR = CLK_PCKEN_TIM4;    /* resume the 10 ms tick for debouncing */
    }
    enable_irq();
}

/* -------------------------------------------------------------- main */

void main(void)
{
    clock_init();
    gpio_init();
    leds_init();
    tick_init();

    mode = 0;
    apply_mode(mode);
    enable_irq();

    for (;;) {
        if (button_event) {
            button_event = 0;
            if (++mode >= NUM_MODES)
                mode = 0;
            apply_mode(mode);
        }

        if (mode == 0) {
            sleep_until_button();
            continue;
        }

        if (tick_flag) {
            tick_flag = 0;
            group_tick(&star);
            group_tick(&branches);
            leds_push_duties();
        } else {
            wfi();                      /* idle until the next interrupt */
        }
    }
}
