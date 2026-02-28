#include "sfx.h"
#include "analogSound.h"

void sfxInit(void) { initSound(); }

// Sound functions 
void sfxShoot(void)   { playSfxPreset(SFXP_SHOOT); }
void sfxHit(void)     { playSfxPreset(SFXP_HIT); }
void sfxBomb(void)    { playSfxPreset(SFXP_BOMB); }
void sfxPowerup(void) { playSfxPreset(SFXP_POWERUP); }
void sfxWin(void)     { playSfxPreset(SFXP_WIN); }
void sfxLose(void)    { playSfxPreset(SFXP_LOSE); }