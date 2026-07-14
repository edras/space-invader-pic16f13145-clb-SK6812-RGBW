![Microchip Logo](pic16f13145_spi_to_ws2812_mcc.X/assets/logo.png "Microchip Technology")

# 1D Space Invaders — PIC16F13145 + SK6812 LED Strip

**A fully playable 1D Space Invaders game running on a PIC16F13145 microcontroller, displayed on a 300-LED SK6812 GRBW strip.**

![Status](https://img.shields.io/badge/status-active-success)
![Platform](https://img.shields.io/badge/platform-PIC16F13145-blue)
![Updated](https://img.shields.io/badge/updated-2026--07--10-informational)

---

## Table of Contents

- [About](#about)
- [Hardware](#hardware)
- [How the Game Works](#how-the-game-works)
- [Game Modes](#game-modes)
- [Controls](#controls)
- [Technical Design](#technical-design)
- [Architecture](#architecture)
- [Key Engineering Decisions](#key-engineering-decisions)
- [Building and Flashing](#building-and-flashing)
- [Changelog](#changelog)

---

## About

This project turns a 5-metre SK6812 GRBW LED strip into a 1D version of Space Invaders. A group of coloured invaders marches from one end of the strip toward the player. The player fires colour-matched shots to destroy individual invaders before the group reaches index 0.

The game runs entirely on a PIC16F13145 at 32 MHz. No external display, no OS, no RTOS — just bare-metal C with a 1 ms hardware timer and an SPI peripheral driving the LEDs through the device's on-chip Configurable Logic Block (CLB).

---

## Hardware

| Component | Details |
|-----------|---------|
| Microcontroller | PIC16F13145 (32 MHz, 2 KB RAM, 8 KB Flash) |
| LED strip | BTF-Lighting SK6812 GRBW, 5 V, 60 LEDs/m, 5 m (300 LEDs) |
| LED protocol | SK6812 GRBW — 4 bytes per LED (G, R, B, W) at 800 kHz |
| SPI → LED conversion | On-chip CLB converts MSSP1 SPI bitstream to WS2812/SK6812 timing |
| Supply | 5 V USB; LED brightness capped at ~35% to limit current draw |

### Button mapping

| Button | Pin | Function |
|--------|-----|----------|
| PB1 | RC4 | Game control (short / long press) |
| PB2 | RC5 | Fire **red** shot |
| PB3 | RA4 | Fire **green** shot |
| PB4 | RC3 | Fire **blue** shot |

### LED strip orientation

```
Index 0  ←  player end (shots start here)
Index 299 ←  spawn end (invaders appear here)
```

---

## How the Game Works

### Cell encoding

All game state lives in a single 300-byte array `strip[]`. Each byte encodes both what is at that position and its colour:

| Value | Meaning |
|-------|---------|
| `0x00` | Empty |
| `0x01` | Red invader |
| `0x02` | Green invader |
| `0x03` | Blue invader |
| `0x81` | Red shot |
| `0x82` | Green shot |
| `0x83` | Blue shot |

Bit 7 is the **shot flag** — set means shot, clear means invader. Bits 1–0 encode the colour (1=red, 2=green, 3=blue). This lets shots and invaders coexist naturally in the same array without any separate data structures.

### Invaders

- The group starts at indices 280–299 (20 random-colour invaders).
- Every game tick the entire group shifts one step toward the player (index 0).
- The game ends immediately when any invader reaches index 0.
- In **Endless mode** a new random invader is appended at index 299 each step so the group never shrinks.

### Shots

- Pressing a shot button places a shot cell at index 0 (player position) if that cell is empty.
- Every shot tick all shot cells advance one step toward the invaders (toward index 299).
- Multiple shots of the same or different colours can be in flight simultaneously.
- When a shot meets an invader:
  - **Colour match** — both are removed and the group compacts (shifts down), shrinking by one LED.
  - **Colour mismatch** — only the shot is removed; the invader is unharmed.

### Win condition (Classic mode only)

When all invader cells are gone, a rainbow sweep animation plays across the full 300-LED strip for 3 seconds, then the game returns to the mode-selection screen.

### Lose condition

When any invader reaches index 0, the entire strip blinks solid red three times (on-board LEDs also flash), then the game returns to the mode-selection screen.

---

## Game Modes

Two modes are available, selected before each game.

### Classic

- 20 fixed-width invaders march toward the player.
- Destroying all invaders wins the game (rainbow celebration).
- The invader group shrinks as you score hits.

### Endless

- A new random invader is added at the far end every step — the group continuously grows.
- There is no win condition; survive as long as possible.
- The game ends only when the group reaches the player.

### Mode indicator (idle blink)

The blink rate of LED 0 tells you which mode is currently selected:

| Blink rate | Mode |
|-----------|------|
| Slow (1 s period) | Classic |
| Fast (150 ms period) | Endless |

---

## Controls

### Idle / mode selection

| Action | Effect |
|--------|--------|
| Short press PB1 | Start the game in the currently selected mode |
| Long press PB1 (≥ 1 s) | Switch to the other mode |

### In game

| Action | Effect |
|--------|--------|
| PB2 | Fire a red shot |
| PB3 | Fire a green shot |
| PB4 | Fire a blue shot |
| Long press PB1 (≥ 1 s) | Abort game and return to mode selection |

### Game over / win animation

| Action | Effect |
|--------|--------|
| Short press PB1 | Skip the animation and return to mode selection immediately |

### Short vs long press

Buttons are classified **on release**:
- Released before 1 second → **short press**
- Released after 1 second → **long press**

This means pressing PB1 during the game triggers nothing until you release it, at which point the hold duration determines what happens.

---

## Technical Design

### SPI → SK6812 via CLB

The PIC16F13145 has no dedicated WS2812 / SK6812 peripheral. Instead, the on-chip **Configurable Logic Block (CLB)** is programmed to convert the MSSP1 SPI bit stream into the required pulse-width-modulated signal:

- SPI clock: 800 kHz (Fosc / (4 × (SSP1ADD+1)) = 32 MHz / 40)
- Each SPI bit becomes one SK6812 bit: short pulse = `0`, long pulse = `1`
- SK6812 byte order: **G, R, B, W** — 4 bytes per LED, 1200 bytes per frame

The CLB circuit is generated by MPLAB MCC and lives in `mcc_generated_files/` (not modified).

### Interrupt masking during render

The SK6812 protocol treats any gap > 80 µs between bytes as a **RESET**, latching the current frame and restarting the LED counter from zero. The 1 ms TMR0 interrupt would create such a gap mid-stream.

Fix: global interrupts are disabled for the entire `WriteLEDs()` call (~15 ms at 800 kHz). After re-enabling, `ms_tick` is compensated by `RENDER_MS = 16` to keep the software timer accurate.

### RAM layout

The `strip[300]` array is 300 bytes — no single normal RAM bank on PIC16F13145 is large enough (each bank is 80 bytes). The linker places it in **BIGRAM** (`0x2000–0x23EF`), a 1008-byte region of SRAM that is addressed through the program memory space. Despite the address range, **BIGRAM is true SRAM — writes do not touch flash**.

All other game variables fit in Bank 0 and the call stack fits in the remaining space without overlap.

### ms_tick compensation

`ms_tick` is incremented by the TMR0 ISR (1 count per ms). During `WriteLEDs()` interrupts are off so `ms_tick` freezes. After each render, `ms_tick += RENDER_MS` compensates for the frozen time. Without this, game tick rates, shot speed, and button debounce timing would all run slower than intended.

### Debounce and long-press

Each button has a `button_t` struct tracking:
- `change_ms` — timestamp of last raw-level change (debounce filter)
- `press_ms` — timestamp when stable press was confirmed
- `long_fired` — set when the hold exceeds `LONG_PRESS_MS` (1 s)

Events fire **on release** only:
- `pressed_event` — if `long_fired` was not set (short press)
- `long_press_event` — if `long_fired` was set (long press)

---

## Architecture

```
main.c
│
├── Timer_1ms_Callback()   ISR — increments ms_tick
├── Ms_Now()               Atomic 16-bit timer read
├── Debounce_Update()      Per-button debounce + short/long press
│
├── WriteLEDs()            SPI frame render (GI disabled, ms_tick compensated)
├── WriteRainbowFrame()    Win animation — hue sweep across full strip
├── WriteAllRed()          Lose animation — solid red flash
│
├── Game_Init()            Reset strip, seed invaders, start game
├── Fire_Shot()            Place shot cell at index 0
├── Shots_Run()            Advance all shot cells, handle collisions
├── Invaders_Advance()     Shift group one step, append new LED in Endless mode
├── All_Invaders_Gone()    Win detection scan (Classic mode only)
└── Game_Update()          Orchestrate shot + invader ticks, speed-up schedule

State machine:
  STATE_IDLE → (short press PB1) → STATE_PLAYING
  STATE_PLAYING → (all invaders gone, Classic) → STATE_WIN → STATE_IDLE
  STATE_PLAYING → (invader reaches index 0) → STATE_GAME_OVER → STATE_IDLE
  STATE_PLAYING → (long press PB1) → STATE_IDLE
```

---

## Key Engineering Decisions

### Why a unified cell array?

The original design used a separate `shots[]` struct array alongside a `color_id_t strip[]` invader array. This caused:
- A RAM bank collision when expanding shot slots (bssBANK0 overlapping cstackBANK0)
- Complex cross-referencing between two arrays in the render loop
- Compaction bugs when removing invaders

The unified cell array eliminates all of these — shots and invaders are the same kind of data, live in the same memory, and are processed by a single forward pass.

### Why BIGRAM for strip[]?

No contiguous 300-byte region exists in the normal RAM banks. BIGRAM is the only option. It behaves identically to RAM for read/write; there is no flash wear.

### Why disable interrupts during render instead of buffering?

A 300-byte frame buffer would consume 300 additional bytes of RAM — impossible given the RAM constraints. Disabling interrupts for ~15 ms is safe: the TMR0 ISR only increments a counter, and compensating `ms_tick` afterward is sufficient to keep timing accurate.

### Why on-release event classification?

On-press classification fires immediately and cannot distinguish short from long — the long-press event would have to fire while the button is still held, which creates ambiguity (the user doesn't know if releasing now counts as short or long). On-release classification is unambiguous: the full hold duration is known at the moment of release.

---

## Building and Flashing

### Requirements

- MPLAB X IDE v6.x or later
- XC8 Compiler v3.x or later
- PIC16F1xxxx_DFP pack 1.29.444 or later

### Build

1. Open `pic16f13145_spi_to_ws2812_mcc.X` in MPLAB X
2. Select the `free` configuration
3. Build → Clean and Build Project

### Flash

Connect a MPLAB SNAP or PICkit 4 to the ICSP header and use MPLAB X to program the device.

---

## Changelog

| Commit | Description |
|--------|-------------|
| *(pending)* | Rainbow wave fix, shot speed doubled, collision fix for fast invaders |
| `e716922` | Short / long press for game selection and in-game reset |
| `8639c37` | Redesign: unified strip cell array, multiple simultaneous shots, invader compaction |
| `e217442` | Bug fixes: shot/invader collision, group shrink on hit |
| `4cd1c95` | Fix SK6812 GRBW 4-byte protocol, interrupt masking, ms_tick compensation |
| `2de8f37` | Initial 1D Space Invaders game |

---

## License

Copyright © Microchip Technology Inc. All rights reserved.

Subject to your compliance with Microchip's terms, you may use this software exclusively with Microchip products.
