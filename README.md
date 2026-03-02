# Astro Sweep (Mode 4)

A fast-paced retro arcade shooter built for the **Game Boy Advance** using **Mode 4**, page flipping, DMA rendering, object pooling, and a complete state machine.

This version upgrades the original Mode 3 game to Mode 4 with:

* True Mode 4 palette-based rendering
* Page flipping every frame (no flicker)
* DMA rendering for both full-screen and non-full-screen graphics
* Custom bitmap-based player ship
* Alternate powered ship sprite
* Persistent high score system
* Fully redesigned Start and Pause screens

# Game Overview

You pilot a rocket in deep space, blasting incoming asteroids to survive and score points. Avoid collisions, manage your lives, and use special abilities strategically to win.

This version features two player sprites:

* Standard Rocket — normal state
* Powered Rocket — active whenever you hold a Nova Bomb

Sprites are rendered using DMA with per-pixel transparency and palette patching in Mode 4.

## State Machine

The game is driven by a full state machine:

### **START**

* Custom full-screen bitmap rendered using DMA
* Static instructional overlay text
* Press START to begin
* Press DOWN to view the Scoreboard

### **GAME**

* Main gameplay loop
* Rocket movement and shooting
* Dynamic HUD updates (Lives, Points, Bombs)
* Sprite switching based on bomb state
* No flicker rendering via page flipping

### **PAUSE**

* Full-screen DMA-rendered bitmap
* Static pause messaging
* Press START to resume
* Press SELECT to return to START
* Press DOWN to view the Scoreboard

### **SCOREBOARD**

Accessible from:
* START
* PAUSE

Displays:
* High Score (persistent across restarts)
* Current Score (only when opened from Pause)

Returns to the state it was opened from.

### **WIN**

* Triggered at 25 points
* Static win screen
* High score updated if beaten
* Press **START** to return to START

### **LOSE**

* Triggered when lives reach 0
* Static lose screen
* High score updated if beaten
* Press **START** to return to START

# Controls

## Normal Gameplay

* **D-Pad** → Move player
* **A** → Shoot
* **B** → Dash (short burst with cooldown)
* **L** (A on keyboard) → Use Nova Bomb (if available)
* **START** → Pause / Resume / Start game
* DOWN (Start/Pause only) → Open Scoreboard

## Dynamic Rocket System (Extra Credit Feature)

The rocket visually changes based on game state:
* When B:0 → Standard Rocket sprite
* When B:1 → Powered Rocket sprite

Implementation details:
* Both sprites use their own 8-color palettes
* Palettes are dynamically patched into unused Mode 4 palette slots
* Transparent background pixels are not drawn
* Rendering is done via row-based DMA with blending
* No flicker due to page flipping

## HUD (Top-Left Display)

* `L:` Lives
* `P:` Points
* `B:` Nova Bomb available (0 or 1)

HUD uses non-static text and updates in real time.
Static screens (Start, Pause, Win, Lose, Scoreboard) use pre-rendered text and overlays.

HUD is always drawn last to guarantee visual stability.

# High Score System (Extra Credit Feature)

* Tracks highest score achieved during runtime
* Persists between game restarts
* Updated automatically on Win or Lose
* Displayed in the Scoreboard state

This adds replayability and competitive depth.

# Above-and-Beyond Mechanic: Nova Bomb Power-Up

A rare **rose red bomb asteroid** spawns roughly **1 in every ~15 asteroids**.

### If you shoot it:

* You gain a **Nova Bomb**
* HUD shows `B:1`
* Rocket switches to powered sprite
* Only 1 bomb can be held at a time

### When activated:

* Clears all active asteroids
* Grants bonus points based on number cleared
* Triggers screen shake effect
* Powered rocket reverts to standard after use
* Bomb is consumed immediately

# Debug / Cheat Controls

Hold these combinations during gameplay to unlock cheats:

* **SELECT + START** → Force WIN
* **SELECT + LEFT** → Force LOSE
* **SELECT + A** → Reset score to 0 and restore lives to 3
* **SELECT + UP** → Restore lives (keep score)
* **SELECT + B** → Clear all asteroids
* **SELECT + RIGHT** → Grant Nova Bomb (`B:1`)

# Technical Implementation Highlights

## Mode 4 Rendering

* 8-bit indexed color mode
* Single active 256-color palette
* Runtime palette patching for rocket sprites
* Back buffer rendering
* Page flipping every frame
* Zero flicker

## DMA Usage

### Full-Screen DMA
* Start screen bitmap
* Pause screen bitmap
* Partial-Screen DMA

### Rocket sprite rows (transparent blending)

* Powered rocket sprite
* Rectangles and fills
* Buffer operations

All sprite rendering is done via DMA row copies for performance.

## Page Flipping

* Entire GAME frame is drawn to back buffer
* waitForVBlank() ensures safe timing
* flipPage() swaps buffers

Eliminates tearing and flicker completely

## Transparency System

Rocket sprites use:
* Per-pixel transparency via index masking
* Row buffering to preserve background pixels
* Selective DMA writes

White background pixels are not drawn, creating true sprite transparency in Mode 4.

## Object Pooling

* Bullet pool
* Asteroid pool
* No dynamic allocation
* Efficient reuse for performance stability

## Collision System

* Bullet ↔ Asteroid
* Player ↔ Asteroid
* Bomb clears all active asteroids
* Score and lives updated appropriately

## Core Systems

* Player struct
* Rocket + Powered Rocket bitmap system
* Bullet struct array
* Asteroid struct array
* Star background system
* Persistent high score tracking
* Bomb inventory system
* Dash cooldown
* Invulnerability frames
* Screen shake effect
* Static vs dynamic rendering separation
* Full state machine architecture

## Build & Run

Compile using the provided GBA toolchain and run in:
* mGBA
* VisualBoyAdvance
* Docker-based GBA compiler
