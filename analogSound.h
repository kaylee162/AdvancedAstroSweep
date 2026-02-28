
#ifndef ANALOG_SOUND_H
#define ANALOG_SOUND_H
#include "gba.h"

// # Analog Sound

// ## Main Register Definitions:

// DMG Sound Channel L/R Volume/Enable
#define REG_SOUNDCNT_L  *(volatile u16*)0x04000080
// Digital DirectSound Sound Control/Mixing
#define REG_SOUNDCNT_H  *(volatile u16*)0x04000082
/*
Master Sound Control Register
  Use SND_ENABLED here GBA-wide setting.
  Here you can also read bits 0-3 to see which DMG channels are set on.
*/ 
#define REG_SOUNDCNT_X  *(volatile u16*)0x04000084
// This register controls the final sound output.
// The default setting is 0200h, it is normally not required to change
#define REG_SOUNDBIAS  *(volatile u16*)0x04000086


// ## Flags:
// This bit must be set in REG_SOUNDCNT_X to hear anything at all from the GBA
#define SND_ENABLED  (1<<7)

// Master volume controll for DMG Sound
// Set in REG_SOUNDCNT_H
#define DMG_MASTER_VOL(n)     ((n) & 3)

// Enable Sound Channel 1 from Left Speaker
// Set in REG_SOUNDCNT_L
#define DMG_SND1_LEFT         (1 << 8)

// Enable Sound Channel 2 from Left Speaker
// Set in REG_SOUNDCNT_L
#define DMG_SND2_LEFT         (1 << 9)
// Enable Sound Channel 3 from Left Speaker
// Set in REG_SOUNDCNT_L

#define DMG_SND3_LEFT         (1 << 10)

// Enable Sound Channel 4 from Left Speaker
// Set in REG_SOUNDCNT_L
#define DMG_SND4_LEFT         (1 << 11)

// Enable Sound Channel 1 from Right Speaker
// Set in REG_SOUNDCNT_L
#define DMG_SND1_RIGHT        (1 << 12)

// Enable Sound Channel 2 from Right Speaker
// Set in REG_SOUNDCNT_L
#define DMG_SND2_RIGHT        (1 << 13)

// Enable Sound Channel 3 from Right Speaker
// Set in REG_SOUNDCNT_L
#define DMG_SND3_RIGHT        (1 << 14)

// Enable Sound Channel 4 from Right Speaker
// Set in REG_SOUNDCNT_L
#define DMG_SND4_RIGHT        (1 << 15)

// DMG Overall Volume from Left Speaker
// Set in REG_SOUNDCNT_L
#define DMG_VOL_LEFT(n)       (((n) & 7) << 0)
// DMG Overall Volume from Left Right Speaker
// Set in REG_SOUNDCNT_L
#define DMG_VOL_RIGHT(n)      (((n) & 7) << 4)

// ## Channel-Specific Registers

/* DMG Sound Channel 1 Sweep register

  Bit        Expl.
  0-2   R/W  Number of sweep shift      (n=0-7)
  3     R/W  Sweep Frequency Direction  (0=Increase, 1=Decrease)
  4-6   R/W  Sweep Time; units of 7.8ms (0-7, min=7.8ms, max=54.7ms)
  7-15  -    Not used
Sweep is disabled by setting Sweep Time to zero, if so, the direction bit should be set.
The change of frequency (NR13,NR14) at each shift is calculated by the following formula where X(0) is initial freq & X(t-1) is last freq:
  X(t) = X(t-1) +/- X(t-1)/2^n
*/ 
#define REG_SND1SWEEP     *(volatile u16*)0x04000060
/* DMG Sound Channel 1 Control Register
  
  Bit        Expl.
  0-5   W    Sound length; units of (64-n)/256s  (0-63)
  6-7   R/W  Wave Pattern Duty                   (0-3, see below)
  8-10  R/W  Envelope Step-Time; units of n/64s  (1-7, 0=No Envelope)
  11    R/W  Envelope Direction                  (0=Decrease, 1=Increase)
  12-15 R/W  Initial Volume of envelope          (1-15, 0=No Sound)
*/
#define REG_SND1CNT       *(volatile u16*)0x04000062
/* DMG Sound Channel 1 Frequency/Control

  Bit        Expl.
  0-10  W    Frequency; 131072/(2048-n)Hz  (0-2047)
  11-13 -    Not used
  14    R/W  Length Flag  (1=Stop output when length in NR11 expires)
  15    W    Initial      (1=Restart Sound)
*/
#define REG_SND1FREQ      *(volatile u16*)0x04000064

/* DMG Sound Channel 2 Control Register
  
  Bit        Expl.
  0-5   W    Sound length; units of (64-n)/256s  (0-63)
  6-7   R/W  Wave Pattern Duty                   (0-3, see below)
  8-10  R/W  Envelope Step-Time; units of n/64s  (1-7, 0=No Envelope)
  11    R/W  Envelope Direction                  (0=Decrease, 1=Increase)
  12-15 R/W  Initial Volume of envelope          (1-15, 0=No Sound)
*/
#define REG_SND2CNT       *(volatile u16*)0x04000068
/* DMG Sound Channel 2 Frequency/Control

  Bit        Expl.
  0-10  W    Frequency; 131072/(2048-n)Hz  (0-2047)
  11-13 -    Not used
  14    R/W  Length Flag  (1=Stop output when length in NR11 expires)
  15    W    Initial      (1=Restart Sound)
*/
#define REG_SND2FREQ      *(volatile u16*)0x0400006C

/*
  Bit        Expl.
  0-4   -    Not used
  5     R/W  Wave RAM Dimension   (0=One bank/32 digits, 1=Two banks/64 digits)
  6     R/W  Wave RAM Bank Number (0-1, see below)
  7     R/W  Sound Channel 3 Off  (0=Stop, 1=Playback)
  8-15  -    Not used
The currently selected Bank Number (Bit 6) will be played back, while reading/writing to/from wave RAM will address the other (not selected) bank. When dimension is set to two banks, output will start by replaying the currently selected bank.
*/
#define REG_SND3SEL       *(volatile u16*)0x04000070
/*
  Bit        Expl.
  0-7   W    Sound length; units of (256-n)/256s  (0-255)
  8-12  -    Not used.
  13-14 R/W  Sound Volume  (0=Mute/Zero, 1=100%, 2=50%, 3=25%)
  15    R/W  Force Volume  (0=Use above, 1=Force 75% regardless of above)
The Length value is used only if Bit 6 in NR34 is set.
*/
#define REG_SND3CNT       *(volatile u16*)0x04000072
/*

  Bit        Expl.
  0-10  W    Sample Rate; 2097152/(2048-n) Hz   (0-2047)
  11-13 -    Not used
  14    R/W  Length Flag  (1=Stop output when length in NR31 expires)
  15    W    Initial      (1=Restart Sound)
  16-31 -    Not used
The above sample rate specifies the number of wave RAM digits per second, the actual tone frequency depends on the wave RAM content, for example:
  Wave RAM, single bank 32 digits   Tone Frequency
  FFFFFFFFFFFFFFFF0000000000000000  65536/(2048-n) Hz
  FFFFFFFF00000000FFFFFFFF00000000  131072/(2048-n) Hz
  FFFF0000FFFF0000FFFF0000FFFF0000  262144/(2048-n) Hz
  FF00FF00FF00FF00FF00FF00FF00FF00  524288/(2048-n) Hz
  F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0  1048576/(2048-n) Hz
*/
#define REG_SND3FREQ      *(volatile u16*)0x04000074
// Channel 3 Wave Pattern RAM (2 banks!!)
#define REG_SND3_WAV       ((volatile u16*)0x04000090)

/* DMG Sound Channel 4 Control Register
  
  Bit        Expl.
  0-5   W    Sound length; units of (64-n)/256s  (0-63)
  6-7   -    Not used
  8-10  R/W  Envelope Step-Time; units of n/64s  (1-7, 0=No Envelope)
  11    R/W  Envelope Direction                  (0=Decrease, 1=Increase)
  12-15 R/W  Initial Volume of envelope          (1-15, 0=No Sound)
*/
#define REG_SND4CNT       *(volatile u16*)0x04000078
/* DMG Sound Channel 4 Frequency/Control

  Bit        Expl.
  0-2   R/W  Dividing Ratio of Frequencies (r)
  3     R/W  Counter Step/Width (0=15 bits, 1=7 bits)
  4-7   R/W  Shift Clock Frequency (s)
  8-13  -    Not used
  14    R/W  Length Flag  (1=Stop output when length in NR41 expires)
  15    W    Initial      (1=Restart Sound)
  See: https://problemkaputt.de/gbatek.htm#gbasoundchannel4noise
*/
#define REG_SND4FREQ      *(volatile u16*)0x0400007C


// ## Channel-Specific Macros to set in their respective control registers

/* Set Sound Sample Length (channels 1,2,4)
  Set in REG_SNDXCNT; Requires DMG_FREQ_TIMED to be set in 
  the corresponding REG_SNDXFREQ
*/
#define DMG_SND_LENGTH(n)     ((n) & 63)

/* 
  Envelope step time for channels 1,2,4
  Set in corresponding REG_SNDXCNT
  0 means no envelope
*/
#define DMG_STEP_TIME(n)      (((n) & 7) << 8)

/* 
  Envelope step direction decreasing for channels 1,2,4
  Set in corresponding REG_SNDXCNT
*/
#define DMG_DIRECTION_DECR    (0 << 11)

/* 
  Envelope step direction increasing for channels 1,2,4
  Set in corresponding REG_SNDXCNT
*/
#define DMG_DIRECTION_INCR    (1 << 11)

/* 
  Envelope volume for channels 1,2,4
  Set in corresponding REG_SNDXCNT
*/
#define DMG_ENV_VOL(n)        (((n) & 15) << 12)

/* DMG Channels 1 & 2 Duty setting
  Wave Duty:
  0: 12.5% ( -_______-_______-_______ )
  1: 25%   ( --______--______--______ )
  2: 50%   ( ----____----____----____ ) (normal)
  3: 75%   ( ------__------__------__ )
*/
#define DMG_DUTY(n)           (((n) & 3) << 6)

/* Set (via REG_SNDXFREQ) to make the sound timed.
  If this bit is not set, the length in REG_SNDXCNT is ignored.
*/
#define DMG_FREQ_TIMED        (1<<14)

/* Reset Flag for REG_SNDXCNT.

  It must always be set for channels 1,2,4!
  For channel 3, it has more nuanced control.
*/
#define SND_RESET             (1<<15)

// Sweep number for REG_SND1SWEEP
#define DMG_SWEEP_NUM(n)       ((n) & 7)
// Sweep direction for REG_SND1SWEEP
#define DMG_SWEEP_UP           (0 << 3)
// Sweep direction for REG_SND1SWEEP
#define DMG_SWEEP_DOWN         (1 << 3)
// Sweep step time for REG_SND1SWEEP
#define DMG_SWEEP_STEPTIME(n)  (((n) & 7) << 4)


#define DMG_SND4_S(s)  (((s) & 15) << 4)
#define DMG_SND4_BIT(b)  (((b) ? 1 : 0) << 3)
#define DMG_SND4_R(r)  (((r) & 7))

// Precalculated frequency settings for REG_SND1FREQ, REG_SND2FREQ
typedef enum note {
  // Sentinel value meaning to not play anything.
  REST      = 0,
  NOTE_C2   =44,
  NOTE_CS2  =157,
  NOTE_D2   =263,
  NOTE_DS2  =363,
  NOTE_E2   =457,
  NOTE_F2   =547,
  NOTE_FS2  =631,
  NOTE_G2   =711,
  NOTE_GS2  =786,
  NOTE_A2   =856,
  NOTE_AS2  =923,
  NOTE_B2   =986,
  NOTE_C3   =1046,
  NOTE_CS3  =1102,
  NOTE_D3   =1155,
  NOTE_DS3  =1205,
  NOTE_E3   =1253,
  NOTE_F3   =1297,
  NOTE_FS3  =1339,
  NOTE_G3   =1379,
  NOTE_GS3  =1417,
  NOTE_A3   =1452,
  NOTE_AS3  =1486,
  NOTE_B3   =1517,
  NOTE_C4   =1547,
  NOTE_CS4  =1575,
  NOTE_D4   =1602,
  NOTE_DS4  =1627,
  NOTE_E4   =1650,
  NOTE_F4   =1673,
  NOTE_FS4  =1694,
  NOTE_G4   =1714,
  NOTE_GS4  =1732,
  NOTE_A4   =1750,
  NOTE_AS4  =1767,
  NOTE_B4   =1783,
  NOTE_C5   =1798,
  NOTE_CS5  =1812,
  NOTE_D5   =1825,
  NOTE_DS5  =1837,
  NOTE_E5   =1849,
  NOTE_F5   =1860,
  NOTE_FS5  =1871,
  NOTE_G5   =1881,
  NOTE_GS5  =1890,
  NOTE_A5   =1899,
  NOTE_AS5  =1907,
  NOTE_B5   =1915,
  NOTE_C6   =1923,
  NOTE_CS6  =1930,
  NOTE_D6   =1936,
  NOTE_DS6  =1943,
  NOTE_E6   =1949,
  NOTE_F6   =1954,
  NOTE_FS6  =1959,
  NOTE_G6   =1964,
  NOTE_GS6  =1969,
  NOTE_A6   =1974,
  NOTE_AS6  =1978,
  NOTE_B6   =1982,
  NOTE_C7   =1985,
  NOTE_CS7  =1989,
  NOTE_D7   =1992,
  NOTE_DS7  =1995,
  NOTE_E7   =1998,
  NOTE_F7   =2001,
  NOTE_FS7  =2004,
  NOTE_G7   =2006,
  NOTE_GS7  =2009,
  NOTE_A7   =2011,
  NOTE_AS7  =2013,
  NOTE_B7   =2015,
  NOTE_C8   =2017
} Note;

typedef struct noteWithDuration {
  Note note;      // From enum note, by name
  unsigned char duration;  // in ms, 0-250
} NoteWithDuration;

void initSound();
void playDrumSound(u8 r, u8 s, u8 b, u8 length, u8 steptime);
void playNoteWithDuration(NoteWithDuration *n, u8 duty);
void playChannel1(u16 note, u8 length, u8 sweepShift, u8 sweepTime, u8 sweepDir, u8 envStepTime, u8 envDir, u8 duty);
void playAnalogSound(u16 sound);

// Simple Channel 2 tone (no sweep, good for layering variety)
void playChannel2(u16 note, u8 length, u8 envStepTime, u8 envDir, u8 duty);

// Preset SFX (small set of notes, easy to keep consistent)
typedef enum {
    SFXP_SHOOT = 0,
    SFXP_HIT,
    SFXP_BOMB,
    SFXP_POWERUP,
    SFXP_WIN,
    SFXP_LOSE
} SfxPreset;

void playSfxPreset(SfxPreset p);

#endif