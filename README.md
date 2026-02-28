# Astro Sweep (Mode 3)

A fast arcade shooter built in **Mode 3** using DMA rectangle drawing, object pooling, and a full state machine.

## State Machine
- **START** screen: press **START** to begin
- **GAME**: play the game
- **PAUSE**: press **START** to pause/resume, press **SELECT** to return to START
- **WIN / LOSE**: press **START** to return to START

## Controls (Normal Play)
- **D-Pad**: Move
- **A**: Shoot (bullet object pool)
- **B**: Dash (short burst, cooldown-based)
- **L (A on a normal keyboard)**: **Nova Bomb** (only if you have one)
- **START**: Pause / Resume (and Start from the title screen)

## HUD
Top-left shows:
- **L:** lives
- **P:** points
- **B:** bomb available (0/1)

## How to Win / Lose
- **Win:** reach **25 points** (destroy normal asteroids to score)
- **Lose:** run out of lives (start with 3)

## Above-and-Beyond Mechanic: Nova Bomb Power-Up
A rare **MAGENTA “bomb asteroid”** spawns about **1 in every ~15 asteroids**.  
If you **shoot** it, you earn a **Nova Bomb** (HUD shows `B:1`).
Note: You can only have 1 **Nova Bomb** at a time.

When you press **L** (or A on a normal keyboard), the Nova Bomb:
- clears all active asteroids on screen
- grants a small bonus score based on how many were cleared
- is consumed immediately (`B` goes back to 0)

This is “above and beyond” because the core game loop (movement, bullets, collisions, win/lose) is complete without it, and the power-up only adds extra strategy.

## Debug / Cheat Controls (for testing)
Hold these combinations during gameplay to unlock cool debugging features:

- **SELECT + START**: force **WIN**
- **SELECT + LEFT ARROW KEY**: force **LOSE**
- **SELECT + A**: reset **score to 0** and restore **lives to 3**
- **SELECT + UP ARROW KEY**: restore **lives to 3** but **keeps the points the same** 
- **SELECT + B**: clear **all asteroids**
- **SELECT + RIGHT ARROW KEY**: give yourself a **Nova Bomb** (`B:1`)

## Notes / Implementation Highlights
- DMA is used for `fillScreen` and `drawRectangle` (via the scaffold’s DMANow).
- Bullets and asteroids use **meaningful object pooling** (recycling inactive objects).
- Flicker is minimized by erasing and redrawing only objects each frame, and drawing the HUD last.