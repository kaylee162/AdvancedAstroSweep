#include "analogSound.h"


// For TA sanity, please do not mess with the volume settings here.
void initSound(){
  // Enable sound (Master control)
  REG_SOUNDCNT_X = SND_ENABLED;

  // Master sound controls for DMG (GameBoy) Sound Generators
  REG_SOUNDCNT_L = DMG_VOL_LEFT(5) |
                   DMG_VOL_RIGHT(5) |
                   DMG_SND1_LEFT |
                   DMG_SND1_RIGHT |
                   DMG_SND2_LEFT |
                   DMG_SND2_RIGHT |
                   DMG_SND3_LEFT |
                   DMG_SND3_RIGHT |
                   DMG_SND4_LEFT |
                   DMG_SND4_RIGHT;

  REG_SOUNDCNT_H = DMG_MASTER_VOL(2);
}

void playNoteWithDuration(NoteWithDuration *n, u8 duty){
  if (n->note != REST) {
    REG_SND2CNT = DMG_ENV_VOL(15) | DMG_DUTY(duty) | (64 - n->duration*256/1000);  // duration is in ms, so back-calculate
    REG_SND2FREQ = n->note | SND_RESET | DMG_FREQ_TIMED;
  } else {
    REG_SND2CNT = 0;
    REG_SND2FREQ = 0;
  }
}

void playChannel1(u16 note, u8 length, u8 sweepShift, u8 sweepTime, u8 sweepDir,
                  u8 envStepTime, u8 envDir, u8 duty) {
  // Configure sweep BEFORE triggering the channel
  REG_SND1SWEEP = DMG_SWEEP_NUM(sweepShift)
                | DMG_SWEEP_STEPTIME(sweepTime)
                | (sweepDir && DMG_SWEEP_DOWN);

  // Configure envelope/duty/length
  REG_SND1CNT = DMG_ENV_VOL(15)
              | (envDir && DMG_DIRECTION_INCR)
              | DMG_STEP_TIME(envStepTime)
              | DMG_DUTY(duty)
              | DMG_SND_LENGTH(length);

  // Trigger LAST
  REG_SND1FREQ = note | SND_RESET | DMG_FREQ_TIMED;
}

#include "analogSound.h"

// Channel 2: simple square wave tone (no sweep)
void playChannel2(u16 note, u8 length, u8 envStepTime, u8 envDir, u8 duty) {
    REG_SND2CNT = DMG_SND_LENGTH(length)
                | DMG_DUTY(duty)
                | DMG_STEP_TIME(envStepTime)
                | (envDir ? DMG_DIRECTION_INCR : DMG_DIRECTION_DECR)
                | DMG_ENV_VOL(13); // slightly lower than max to avoid harsh clipping

    REG_SND2FREQ = note | DMG_FREQ_TIMED | SND_RESET;
}

// Safe, varied presets: minimal notes, but clear identity per event.
void playSfxPreset(SfxPreset p) {
    switch (p) {

        case SFXP_SHOOT:
            // Quick “pew”: high note, short, no sweep
            playChannel2(NOTE_E6, 10, 1, 0, 2);
            break;

        case SFXP_HIT:
            // “Thunk”: mid-low tone + tiny noise tick
            playChannel2(NOTE_G4, 14, 2, 0, 2);
            playDrumSound(1, 7, 0, 10, 2); // short noise tap
            break;

        case SFXP_BOMB:
            // “Explosion”: descending tone + heavier noise
            // (Channel 1 sweep down gives that falling pitch effect)
            playChannel1(NOTE_C5, 28,
                         2, 4, 1,     // sweepShift=2, sweepTime=4, sweepDir=DOWN
                         2, 0,        // envStepTime=2, envDir=DECR
                         2);          // duty=50%
            playDrumSound(3, 4, 1, 30, 2); // bigger noise burst
            break;

        case SFXP_POWERUP:
            // “Reward”: small two-tone blip (like a pickup)
            playChannel2(NOTE_C6, 10, 1, 0, 1);
            playChannel1(NOTE_E6, 10,
                         0, 0, 0,     // no sweep
                         1, 0,        // short decay
                         1);          // duty=25%
            break;

        case SFXP_WIN:
            // “Win jingle”: simple chord-ish sound (two channels)
            playChannel2(NOTE_C6, 16, 1, 0, 2);
            playChannel1(NOTE_E6, 16,
                         0, 0, 0,
                         1, 0,
                         2);
            break;

        case SFXP_LOSE:
            // “Lose”: low, longer tone + low companion tone
            playChannel2(NOTE_E3, 22, 3, 0, 2);
            playChannel1(NOTE_C3, 22,
                         0, 0, 0,
                         3, 0,
                         2);
            break;

        default:
            break;
    }
}

void playDrumSound(u8 r, u8 s, u8 b, u8 length, u8 steptime){
  REG_SND4CNT = DMG_ENV_VOL(15) | DMG_STEP_TIME(steptime) | DMG_SND_LENGTH(length);
  REG_SND4FREQ = SND_RESET | DMG_FREQ_TIMED | DMG_SND4_S(s) | DMG_SND4_BIT(b) | DMG_SND4_R(r);
}

void playAnalogSound(u16 sound){
  switch (sound){
    case 0:
      playDrumSound(0, 0, 0, 20, 1);  // "Hiss"
      break;
    case 1:
      playDrumSound(1, 3, 1, 32, 2);  // "Zap"
      break;
    case 2:
      playDrumSound(0, 7, 0, 30, 5);  // "Honk"
      break;
    case 3:
      playDrumSound(0, 7, 1, 30, 5);  // "Robot"
      break;
    case 4:
      playDrumSound(0, 9, 1, 0, 1);  // "Bump"
      break;
    case 5:
      playDrumSound(3, 3, 1, 30, 1);  // "Zonk"
      break;
    case 6:
      playDrumSound(7, 1, 1, 32, 0);  // "Buzzer"
      break;
    case 7:
      playDrumSound(0, 9, 0, 32, 3);  // "Blip"
      break;
    case 8:
      playChannel1(NOTE_G4, 0, 7, 3, 0, 1, 0, 0);  // "Boing"
      break;
    case 9:
      playChannel1(NOTE_A4, 0, 4, 3, 1, 1, 0, 0);  // "Chirp"
      break;
    case 10:
      playChannel1(NOTE_D4, 0, 4, 3, 1, 1, 0, 2);  // "Bleep"
      break;
    case 11:
      playChannel1(NOTE_G4, 10, 7, 1, 1, 2, 1, 2);  // "Wheep"
      break;
    case 12:
      playChannel1(NOTE_E4, 0, 7, 1, 1, 4, 1, 2);  // "Bleeep"
      break;
    case 13:
      playChannel1(NOTE_G4, 0, 7, 2, 1, 4, 1, 2);  // "Bloop"
      break;
    case 14:
      playChannel1(NOTE_E4, 1, 4, 2, 0, 2, 0, 2);  // "Whoop"
      break;
    case 15:
      playChannel1(NOTE_DS4, 5, 4, 2, 0, 2, 0, 2);  // "Whop"
      break;
    case 16:
      playChannel1(NOTE_G4, 0, 4, 7, 1, 2, 1, 2);  // "Shine"
      break;
    case 17:
      playChannel1(NOTE_A4, 0, 5, 7, 1, 4, 1, 3);  // "Panic"
      break;
    default:
      break;
  }
}