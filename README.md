# Astro Sweep (Mode 4)

A fast-paced retro arcade shooter built for the **Game Boy Advance** using **Mode 4**, page flipping, DMA rendering, object pooling, and a complete state machine.

This version upgrades the original Mode 3 game to Mode 4 with:

* Page flipping for smoother rendering
* Palette-based graphics
* Custom designed Start, Pause, Win, and Lose screens
* Improved flicker handling

## Game Overview

You pilot a ship in deep space, blasting incoming asteroids to survive and score points. Avoid collisions, manage your lives, and use special abilities strategically to win.

## State Machine

The game is driven by a full state machine:

### **START**

* Custom retro title screen
* Press **START** to begin

### **GAME**

* Main gameplay loop
* Player movement, shooting, collisions, HUD updates

### **PAUSE**

* Press **START** to pause/resume
* Press **SELECT** to return to START

### **WIN**

* Triggered at 25 points
* Custom win screen
* Press **START** to return to START

### **LOSE**

* Triggered when lives reach 0
* Custom lose screen
* Press **START** to return to START

## Controls

## Normal Gameplay

* **D-Pad** → Move player
* **A** → Shoot
* **B** → Dash (short burst with cooldown)
* **L** (A on keyboard) → Use Nova Bomb (if available)
* **START** → Pause / Resume / Start game

## HUD (Top-Left Display)

* `L:` Lives
* `P:` Points
* `B:` Nova Bomb available (0 or 1)

HUD redraws dynamically and is rendered last to prevent flicker artifacts.

## Win / Lose Conditions

### Win

* Reach **25 points**

### Lose

* Run out of lives (start with 3)

## Above-and-Beyond Mechanic: Nova Bomb Power-Up

A rare **magenta bomb asteroid** spawns roughly **1 in every ~15 asteroids**.

### If you shoot it:

* You gain a **Nova Bomb**
* HUD shows `B:1`
* Only 1 bomb can be held at a time

### When activated:

* Clears all active asteroids
* Grants bonus points based on number cleared
* Bomb is consumed immediately

This mechanic adds strategic depth without altering the core gameplay loop.

# 🧪 Debug / Cheat Controls

Hold these combinations during gameplay to unlock cheats:

* **SELECT + START** → Force WIN
* **SELECT + LEFT** → Force LOSE
* **SELECT + A** → Reset score to 0 and restore lives to 3
* **SELECT + UP** → Restore lives (keep score)
* **SELECT + B** → Clear all asteroids
* **SELECT + RIGHT** → Grant Nova Bomb (`B:1`)

## Technical Implementation Highlights

## Mode 4 Rendering

* Uses **palette-based graphics**
* Draws to a **back buffer**
* Flips pages each frame for smooth animation
* Reduces tearing and flicker

## DMA Usage
* DMA used for:
  * `fillScreen`
  * `drawRectangle`
  * buffer copying
* Minimizes CPU overhead

## Object Pooling

* Bullet pool
* Asteroid pool
* Objects are reused instead of reallocated
* Prevents memory waste and improves performance

## Collision System

* Bullet ↔ Asteroid
* Player ↔ Asteroid
* Bomb clears all active asteroids
* Meaningful collisions affect score and lives

## Flicker Reduction

* Page flipping
* Dirty redraw logic
* HUD drawn last
* Off-screen cleanup handling

## Core Systems

* Player struct
* Bullet struct array (object pool)
* Asteroid struct array (object pool)
* Star background system
* Score tracking
* Bomb inventory system
* Cooldowns (dash, shooting)
* Invulnerability frames
* Screen shake effect during bomb use

## Build & Run

Compile using the provided GBA toolchain and run in:

* mGBA
* VisualBoyAdvance
* Docker-based GBA compiler
