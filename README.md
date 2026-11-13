# ChristmasTreeFW

Firmware for a Christmas tree LED ornament built around the STM8L101F3P6 (TSSOP-20, 8 KB flash, 1.5 KB RAM).
Two tri-colour LED groups, a star on top and the branches below, are driven by software PWM from two
hardware timers. A single push button cycles through the light programs and a low-power sleep mode.

## Hardware

All LED outputs are push-pull and active low: logic 0 turns a segment on, logic 1 turns it off.

| Pin | Function |
|-----|----------|
| PB5 | Star, blue |
| PB4 | Star, green |
| PB3 | Star, red |
| PB2 | Branches, blue |
| PB1 | Branches, green |
| PB0 | Branches, red |
| PA2 | Push button to ground, active low, internal pull-up enabled |
| PA0 | SWIM programming pin |

Programming connector: SWIM, GND, NRST, VDD. The Samtec TSM-104 header library files are in `libs/`.

## Light programs

The board powers up in program 1. Each button press switches to the next program straight away.
After the last program the next press puts the tree to sleep (mode 0), and the press after that
wakes it into program 1 again. After a press is accepted, further presses are ignored for 250 ms,
which debounces the switch.

| Mode | Star | Branches |
|------|------|----------|
| 0 | Off | Off. Core in Halt, about 1 uA, wakes on button |
| 1 | Twinkles white | Twinkles green |
| 2 | Red, green, blue, 1 s each | Same, in step with the star |
| 3 | Twinkles white | Crossfades green and red |
| 4 | Rainbow drift, leads | Rainbow drift, follows |
| 5 | Gold twinkle | Candle-flame flicker |
| 6 | Steady gold | Fast red, green, blue party |

Every program change starts from the same state: both PWM timers stopped with counters, flags and
frame phase cleared, the 10 ms tick restarted, and both LED groups faded up from dark.

Programs are defined as pairs of pattern descriptors in the `MODES` table at the top of `src/main.c`.
A pattern is a palette plus brightness, fade-speed and hold ranges, so new programs are a one-line addition.

## How it works

| Timer | Role |
|-------|------|
| TIM2 | Star PWM engine, 12.8 kHz update interrupt |
| TIM3 | Branch PWM engine, 12.8 kHz update interrupt |
| TIM4 | 10 ms system tick: button debounce and animation pacing |
| EXTI2 | Button falling edge, wakes the core from Halt |

Each PWM interrupt chops its three channels with a 64-step duty cycle, giving a flicker-free 200 Hz frame.
Duty values are latched at the start of each frame so the animator can update them at any time.
The port writes compile to single `bset`/`bres` instructions, so the two engines share port B safely.

The animator runs at 100 Hz and fades each colour channel toward a random target brightness at its own
random speed. Because red, green and blue arrive at different times the colours overlap and blend on the
way, which produces the twinkle. Levels pass through a quadratic gamma so fades look even to the eye.

In mode 0 every peripheral clock is gated and the core executes HALT. A press that arrives in the few
cycles before HALT leaves the EXTI request pending, so HALT returns immediately and no press is lost.

## Source layout

```
src/main.c          mode table, button handling, sleep, TIM4 and EXTI2 interrupt handlers
src/leds.c          PWM interrupt handlers for TIM2/TIM3 and the twinkle animator
src/leds.h          LED engine interface and pattern descriptor types
src/stm8l101.h      register map and interrupt vector numbers used by this project
openocd/            OpenOCD configuration for SWIM programming
.vscode/            IntelliSense settings so the editor understands SDCC keywords
libs/               Samtec TSM-104 connector schematic symbol, footprint and STEP model
```

## Toolchain

| Tool | Purpose | Install |
|------|---------|---------|
| SDCC 4.5.0 | C compiler for STM8 | `winget install SDCC.SDCC` |
| mingw32-make | Runs the Makefile | Part of the WinLibs GCC package on winget |
| OpenOCD 0.12 | SWIM programmer via ST-Link | `winget install xpack-dev-tools.openocd-xpack` |
| Zadig | Binds the WinUSB driver to the ST-Link | `winget install akeo.ie.Zadig` |

SDCC 4.5.0 miscompiles `rnd8() % p->ncolors` in `group_pick()`: the random value is overwritten before
the divide, so the result is always 0. An earlier retry loop built on that expression hung the firmware.
`group_pick()` now does all random choices through `rnd_range()`, whose generated code is correct, and has
no retry loop. Check the `.rst` listing if you add new `%` expressions on 8-bit values.

SDCC 4.5.0 for Windows ships its preprocessor helper as `bin\cc1` without an `.exe` extension, which makes
every compile fail with `cannot execute 'cc1'`. Copy `C:\Program Files\SDCC\bin\cc1` to `cc1.exe` in the same
folder and make sure `C:\Program Files\SDCC\bin` is on `PATH`.

## Build

```
mingw32-make          # build/main.ihx
mingw32-make bin      # build/main.bin, raw 64 KB address-space image with code at 0x8000
mingw32-make clean
```

The current build uses about 1.9 KB of flash and 65 bytes of static RAM; the stack peaks at about 30 bytes.

## Flash over SWIM

You need a SWIM-capable probe: a standalone ST-LINK/V2 or clone, or an STLINK-V3SET with the SWIM adapter.
The ST-Link built into STM32 Nucleo boards only supports SWD and will not work.

1. Plug in the ST-Link, run Zadig, choose Options, List All Devices, select the ST-Link debug interface
   and install the WinUSB driver. This is a one-time step.
2. Connect SWIM, GND, NRST and VDD to the board.
3. Run:

```
mingw32-make flash
```

This calls OpenOCD with `openocd/stm8l101f3.cfg`, connects with NRST held low (a plain SWIM connect fails
while the LED firmware runs), then writes, verifies and starts the firmware. OpenOCD's `program` command does
not work for STM8 because there is no STM8 flash-bank driver; the Makefile uses `load_image` and
`verify_image` instead.
If you have `stm8flash` on your `PATH`, `mingw32-make flash-stm8flash` does the same job with that tool.

## Tuning notes

The palettes assume equal LED efficiency per colour. Most tri-colour LEDs have a weak red or a strong blue,
so the white entries in `src/main.c` may need the green and blue values reduced to look neutral.
