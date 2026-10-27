// Imported without changing score/effect bytes; see tools/soundtrack-upstream.json.
#pragma once

// Music and sound effects for Mystic Balloon (ATMlib).
//   mysticTitle   title screen and menus: Arabian night, 8 bars, 3 channels, loops forever
// Sound effects are one-track ATM_SFX_TRACKs for ATM.playSfx() (ATMlib has ONE effect slot, a new effect replaces the running one).

#include "atm/ATMcmds.h"

#ifndef Song
#define Song const uint8_t PROGMEM
#endif

// ---------------------------------------------------------------------------
// Mystic Balloon - title
// Arabian night: D hijaz (D Eb F# G A Bb C), tempo 28, 8 bars = 512 ticks.
// CH0 lead, CH1 darbuka rhythm, CH2 arpeggio pad (CH3 silent).
// 184 bytes
// ---------------------------------------------------------------------------
Song mysticTitle[] = {          // total song in bytes = 184
  0x06,                         // Number of tracks
  0x00, 0x00,                   // Address of track 0   //0  lead
  0x60, 0x00,                   // Address of track 1   //96  A1
  0x6F, 0x00,                   // Address of track 2   //111  drum
  0x76, 0x00,                   // Address of track 3   //118  hit
  0x89, 0x00,                   // Address of track 4   //137  pad
  0xA6, 0x00,                   // Address of track 5   //166  silent

  0x00,                         // Channel 0 entry track (lead)
  0x02,                         // Channel 1 entry track (drum)
  0x04,                         // Channel 2 entry track (pad)
  0x05,                         // Channel 3 entry track (silent)

  //"Track 0" lead  [96 bytes]
  ATM_SET_TEMPO(28),
  ATM_VOL(40),
  ATM_SL_VOL((uint8_t)-1),
  ATM_GOTO(1),   // -> A1
  ATM_NOTE_A4,
  ATM_DELAY(8),
  ATM_NOTE_A4_,
  ATM_DELAY(8),
  ATM_NOTE_A4,
  ATM_DELAY(4),
  ATM_NOTE_G4,
  ATM_DELAY(4),
  ATM_NOTE_F4_,
  ATM_DELAY(16),
  ATM_NOTE_G4,
  ATM_DELAY(8),
  ATM_NOTE_A4,
  ATM_DELAY(16),
  ATM_NOTE_A4_,
  ATM_DELAY(16),
  ATM_NOTE_A4,
  ATM_DELAY(4),
  ATM_NOTE_A4_,
  ATM_DELAY(4),
  ATM_NOTE_C5,
  ATM_DELAY(8),
  ATM_NOTE_D5,
  ATM_DELAY(8),
  ATM_NOTE_D5_,
  ATM_DELAY(8),
  ATM_NOTE_F5_,
  ATM_DELAY(16),
  ATM_NOTE_D5_,
  ATM_DELAY(8),
  ATM_NOTE_D5,
  ATM_DELAY(8),
  ATM_NOTE_C5,
  ATM_DELAY(8),
  ATM_NOTE_A4_,
  ATM_DELAY(8),
  ATM_NOTE_A4,
  ATM_DELAY(8),
  ATM_NOTE_G4,
  ATM_DELAY(8),
  ATM_NOTE_F4_,
  ATM_DELAY(16),
  ATM_GOTO(1),   // -> A1
  ATM_NOTE_A4,
  ATM_DELAY(8),
  ATM_NOTE_A4_,
  ATM_DELAY(8),
  ATM_NOTE_A4,
  ATM_DELAY(4),
  ATM_NOTE_G4,
  ATM_DELAY(4),
  ATM_NOTE_F4_,
  ATM_DELAY(16),
  ATM_NOTE_A4,
  ATM_DELAY(8),
  ATM_NOTE_D5,
  ATM_DELAY(16),
  ATM_NOTE_D5,
  ATM_DELAY(8),
  ATM_NOTE_D5_,
  ATM_DELAY(8),
  ATM_NOTE_F5_,
  ATM_DELAY(16),
  ATM_NOTE_G5,
  ATM_DELAY(8),
  ATM_NOTE_F5_,
  ATM_DELAY(4),
  ATM_NOTE_D5_,
  ATM_DELAY(4),
  ATM_NOTE_D5,
  ATM_DELAY(16),
  ATM_NOTE_C5,
  ATM_DELAY(8),
  ATM_NOTE_A4_,
  ATM_DELAY(8),
  ATM_NOTE_A4,
  ATM_DELAY(8),
  ATM_NOTE_G4,
  ATM_DELAY(8),
  ATM_NOTE_F4_,
  ATM_DELAY(8),
  ATM_NOTE_D4_,
  ATM_DELAY(8),
  ATM_NOTE_D4,
  ATM_DELAY(16),
  ATM_GOTO(0),   // -> lead

  //"Track 1" A1  [15 bytes]
  ATM_NOTE_D5,
  ATM_DELAY(16),
  ATM_NOTE_D5_,
  ATM_DELAY(4),
  ATM_NOTE_D5,
  ATM_DELAY(4),
  ATM_NOTE_C5,
  ATM_DELAY(8),
  ATM_NOTE_A4_,
  ATM_DELAY(8),
  ATM_NOTE_A4,
  ATM_DELAY(8),
  ATM_NOTE_G4,
  ATM_DELAY(16),
  ATM_RETURN,

  //"Track 2" drum  [7 bytes]
  ATM_SL_VOL((uint8_t)-3),
  ATM_REPEAT(7, 3),   // 8x hit
  ATM_GOTO(2),   // -> drum

  //"Track 3" hit  [19 bytes]
  ATM_VOL(36),
  ATM_NOTE_D2,
  ATM_DELAY(8),
  ATM_VOL(22),
  ATM_NOTE_D5,
  ATM_DELAY(16),
  ATM_NOTE_D5,
  ATM_DELAY(8),
  ATM_VOL(36),
  ATM_NOTE_D2,
  ATM_DELAY(16),
  ATM_VOL(22),
  ATM_NOTE_D5,
  ATM_DELAY(16),
  ATM_RETURN,

  //"Track 4" pad  [29 bytes]
  ATM_VOL(16),
  ATM_ARP(0x43, 0x23),
  ATM_NOTE_D3,
  ATM_DELAY(64),
  ATM_DELAY(64),
  ATM_ARP(0x43, 0x23),
  ATM_NOTE_D3_,
  ATM_DELAY(64),
  ATM_DELAY(64),
  ATM_NOTE_D3,
  ATM_DELAY(64),
  ATM_DELAY(64),
  ATM_ARP(0x34, 0x23),
  ATM_NOTE_G3,
  ATM_DELAY(64),
  ATM_ARP(0x43, 0x23),
  ATM_NOTE_A3,
  ATM_DELAY(64),
  ATM_GOTO(4),   // -> pad

  //"Track 5" silent  [1 bytes]
  ATM_STOP_CHAN,

};

// music volume scaling (see the ATMlib README): 100 = as written in the tunes; lower it to make the tunes quieter than the effects
#define MUSIC_VOL_PCT 100
#define MV(v) ((v) * MUSIC_VOL_PCT / 100)
#define MS(n) ((uint8_t)-((((n) * MUSIC_VOL_PCT + 50) / 100) < 1 ? 1 : (((n) * MUSIC_VOL_PCT + 50) / 100)))

// ---------------------------------------------------------------------------
// in-game tunes: Arabian / Egyptian, hijaz scale, CH1 lead + CH2 bass (CH0 stays free for the sound effects)
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Sphinx Sands
// Sphinx Sands: D hijaz (D Eb F# G A Bb C), tempo 30, 8 bars = 512 ticks. CH1 bass, CH2 lead (CH0 / CH3 silent).
// 112 bytes
// ---------------------------------------------------------------------------
Song tuneSphinx[] = {          // total song in bytes = 112
  0x06,                         // Number of tracks
  0x00, 0x00,                   // Address of track 0   //0  lead
  0x10, 0x00,                   // Address of track 1   //16  bass
  0x19, 0x00,                   // Address of track 2   //25  bb
  0x24, 0x00,                   // Address of track 3   //36  A
  0x3F, 0x00,                   // Address of track 4   //63  B
  0x5E, 0x00,                   // Address of track 5   //94  silent

  0x05,                         // Channel 0 entry track (silent)
  0x01,                         // Channel 1 entry track (bass)
  0x00,                         // Channel 2 entry track (lead)
  0x05,                         // Channel 3 entry track (silent)

  //"Track 0" lead  [16 bytes]
  ATM_SET_TEMPO(30),
  ATM_VOL(MV(28)),
  ATM_SL_VOL(MS(1)),
  ATM_GOTO(3),   // -> A
  ATM_GOTO(4),   // -> B
  ATM_GOTO(3),   // -> A
  ATM_GOTO(4),   // -> B
  ATM_GOTO(0),   // -> lead

  //"Track 1" bass  [9 bytes]
  ATM_VOL(MV(22)),
  ATM_SL_VOL(MS(2)),
  ATM_REPEAT(15, 2),   // 16x bb
  ATM_GOTO(1),   // -> bass

  //"Track 2" bb  [11 bytes]
  ATM_NOTE_D2,
  ATM_DELAY(4),
  ATM_NOTE_A2,
  ATM_DELAY(8),
  ATM_NOTE_A2,
  ATM_DELAY(4),
  ATM_NOTE_D2,
  ATM_DELAY(8),
  ATM_NOTE_A2,
  ATM_DELAY(8),
  ATM_RETURN,

  //"Track 3" A  [27 bytes]
  ATM_NOTE_A3,
  ATM_DELAY(12),
  ATM_NOTE_G3,
  ATM_DELAY(4),
  ATM_NOTE_F3_,
  ATM_DELAY(8),
  ATM_NOTE_D3_,
  ATM_DELAY(8),
  ATM_NOTE_D3,
  ATM_DELAY(32),
  ATM_NOTE_D3_,
  ATM_DELAY(4),
  ATM_NOTE_F3_,
  ATM_DELAY(4),
  ATM_NOTE_G3,
  ATM_DELAY(8),
  ATM_NOTE_A3,
  ATM_DELAY(8),
  ATM_NOTE_A3_,
  ATM_DELAY(8),
  ATM_NOTE_C4,
  ATM_DELAY(12),
  ATM_NOTE_A3_,
  ATM_DELAY(4),
  ATM_NOTE_A3,
  ATM_DELAY(16),
  ATM_RETURN,

  //"Track 4" B  [31 bytes]
  ATM_NOTE_D4,
  ATM_DELAY(12),
  ATM_NOTE_C4,
  ATM_DELAY(4),
  ATM_NOTE_A3_,
  ATM_DELAY(8),
  ATM_NOTE_A3,
  ATM_DELAY(8),
  ATM_NOTE_G3,
  ATM_DELAY(8),
  ATM_NOTE_F3_,
  ATM_DELAY(8),
  ATM_NOTE_G3,
  ATM_DELAY(8),
  ATM_NOTE_A3,
  ATM_DELAY(8),
  ATM_NOTE_F3_,
  ATM_DELAY(8),
  ATM_NOTE_D3_,
  ATM_DELAY(8),
  ATM_NOTE_D3,
  ATM_DELAY(16),
  ATM_NOTE_D3_,
  ATM_DELAY(4),
  ATM_NOTE_D3,
  ATM_DELAY(4),
  ATM_NOTE_D3_,
  ATM_DELAY(8),
  ATM_NOTE_D3,
  ATM_DELAY(16),
  ATM_RETURN,

  //"Track 5" silent  [1 bytes]
  ATM_STOP_CHAN,

};


// ---------------------------------------------------------------------------
// Nile Caravan
// Nile Caravan: A hijaz (A Bb C# D E F G), tempo 28, 8 bars = 512 ticks. CH1 bass, CH2 lead (CH0 / CH3 silent).
// 108 bytes
// ---------------------------------------------------------------------------
Song tuneCaravan[] = {          // total song in bytes = 108
  0x06,                         // Number of tracks
  0x00, 0x00,                   // Address of track 0   //0  lead
  0x10, 0x00,                   // Address of track 1   //16  bass
  0x19, 0x00,                   // Address of track 2   //25  bb
  0x26, 0x00,                   // Address of track 3   //38  A
  0x3D, 0x00,                   // Address of track 4   //61  B
  0x5A, 0x00,                   // Address of track 5   //90  silent

  0x05,                         // Channel 0 entry track (silent)
  0x01,                         // Channel 1 entry track (bass)
  0x00,                         // Channel 2 entry track (lead)
  0x05,                         // Channel 3 entry track (silent)

  //"Track 0" lead  [16 bytes]
  ATM_SET_TEMPO(28),
  ATM_VOL(MV(28)),
  ATM_SL_VOL(MS(1)),
  ATM_GOTO(3),   // -> A
  ATM_GOTO(4),   // -> B
  ATM_GOTO(3),   // -> A
  ATM_GOTO(4),   // -> B
  ATM_GOTO(0),   // -> lead

  //"Track 1" bass  [9 bytes]
  ATM_VOL(MV(22)),
  ATM_SL_VOL(MS(2)),
  ATM_REPEAT(15, 2),   // 16x bb
  ATM_GOTO(1),   // -> bass

  //"Track 2" bb  [13 bytes]
  ATM_NOTE_A2,
  ATM_DELAY(6),
  ATM_NOTE_A2,
  ATM_DELAY(2),
  ATM_NOTE_E3,
  ATM_DELAY(8),
  ATM_NOTE_A2,
  ATM_DELAY(6),
  ATM_NOTE_A2,
  ATM_DELAY(2),
  ATM_NOTE_E3,
  ATM_DELAY(8),
  ATM_RETURN,

  //"Track 3" A  [23 bytes]
  ATM_NOTE_A3,
  ATM_DELAY(12),
  ATM_NOTE_A3_,
  ATM_DELAY(4),
  ATM_NOTE_C4_,
  ATM_DELAY(12),
  ATM_NOTE_D4,
  ATM_DELAY(4),
  ATM_NOTE_E4,
  ATM_DELAY(32),
  ATM_NOTE_F4,
  ATM_DELAY(8),
  ATM_NOTE_E4,
  ATM_DELAY(8),
  ATM_NOTE_D4,
  ATM_DELAY(12),
  ATM_NOTE_C4_,
  ATM_DELAY(4),
  ATM_NOTE_A3_,
  ATM_DELAY(8),
  ATM_NOTE_A3,
  ATM_DELAY(24),
  ATM_RETURN,

  //"Track 4" B  [29 bytes]
  ATM_NOTE_E4,
  ATM_DELAY(12),
  ATM_NOTE_E4,
  ATM_DELAY(4),
  ATM_NOTE_F4,
  ATM_DELAY(12),
  ATM_NOTE_E4,
  ATM_DELAY(4),
  ATM_NOTE_D4,
  ATM_DELAY(12),
  ATM_NOTE_C4_,
  ATM_DELAY(4),
  ATM_NOTE_D4,
  ATM_DELAY(16),
  ATM_NOTE_C4_,
  ATM_DELAY(8),
  ATM_NOTE_A3_,
  ATM_DELAY(8),
  ATM_NOTE_A3,
  ATM_DELAY(12),
  ATM_NOTE_A3_,
  ATM_DELAY(4),
  ATM_NOTE_C4_,
  ATM_DELAY(8),
  ATM_NOTE_A3_,
  ATM_DELAY(8),
  ATM_NOTE_A3,
  ATM_DELAY(16),
  ATM_RETURN,

  //"Track 5" silent  [1 bytes]
  ATM_STOP_CHAN,

};


// ---------------------------------------------------------------------------
// Pharaoh's Hall
// Pharaoh's Hall: E hijaz (E F G# A B C D), tempo 24, 8 bars = 512 ticks. CH1 bass, CH2 lead (CH0 / CH3 silent).
// 100 bytes
// ---------------------------------------------------------------------------
Song tuneHall[] = {          // total song in bytes = 100
  0x06,                         // Number of tracks
  0x00, 0x00,                   // Address of track 0   //0  lead
  0x10, 0x00,                   // Address of track 1   //16  bass
  0x19, 0x00,                   // Address of track 2   //25  bb
  0x22, 0x00,                   // Address of track 3   //34  A
  0x3B, 0x00,                   // Address of track 4   //59  B
  0x52, 0x00,                   // Address of track 5   //82  silent

  0x05,                         // Channel 0 entry track (silent)
  0x01,                         // Channel 1 entry track (bass)
  0x00,                         // Channel 2 entry track (lead)
  0x05,                         // Channel 3 entry track (silent)

  //"Track 0" lead  [16 bytes]
  ATM_SET_TEMPO(24),
  ATM_VOL(MV(28)),
  ATM_SL_VOL(MS(1)),
  ATM_GOTO(3),   // -> A
  ATM_GOTO(4),   // -> B
  ATM_GOTO(3),   // -> A
  ATM_GOTO(4),   // -> B
  ATM_GOTO(0),   // -> lead

  //"Track 1" bass  [9 bytes]
  ATM_VOL(MV(22)),
  ATM_SL_VOL(MS(2)),
  ATM_REPEAT(15, 2),   // 16x bb
  ATM_GOTO(1),   // -> bass

  //"Track 2" bb  [9 bytes]
  ATM_NOTE_E2,
  ATM_DELAY(12),
  ATM_NOTE_B2,
  ATM_DELAY(4),
  ATM_NOTE_E2,
  ATM_DELAY(8),
  ATM_NOTE_B2,
  ATM_DELAY(8),
  ATM_RETURN,

  //"Track 3" A  [25 bytes]
  ATM_NOTE_B3,
  ATM_DELAY(16),
  ATM_NOTE_C4,
  ATM_DELAY(4),
  ATM_NOTE_B3,
  ATM_DELAY(4),
  ATM_NOTE_G3_,
  ATM_DELAY(8),
  ATM_NOTE_F3,
  ATM_DELAY(8),
  ATM_NOTE_E3,
  ATM_DELAY(24),
  ATM_NOTE_F3,
  ATM_DELAY(8),
  ATM_NOTE_G3_,
  ATM_DELAY(8),
  ATM_NOTE_B3,
  ATM_DELAY(16),
  ATM_NOTE_C4,
  ATM_DELAY(8),
  ATM_NOTE_D4,
  ATM_DELAY(8),
  ATM_NOTE_B3,
  ATM_DELAY(16),
  ATM_RETURN,

  //"Track 4" B  [23 bytes]
  ATM_NOTE_E4,
  ATM_DELAY(16),
  ATM_NOTE_D4,
  ATM_DELAY(8),
  ATM_NOTE_C4,
  ATM_DELAY(8),
  ATM_NOTE_B3,
  ATM_DELAY(16),
  ATM_NOTE_G3_,
  ATM_DELAY(8),
  ATM_NOTE_B3,
  ATM_DELAY(8),
  ATM_NOTE_C4,
  ATM_DELAY(12),
  ATM_NOTE_B3,
  ATM_DELAY(4),
  ATM_NOTE_G3_,
  ATM_DELAY(8),
  ATM_NOTE_F3,
  ATM_DELAY(8),
  ATM_NOTE_E3,
  ATM_DELAY(32),
  ATM_RETURN,

  //"Track 5" silent  [1 bytes]
  ATM_STOP_CHAN,

};


// ---------------------------------------------------------------------------
// Oasis Dance
// Oasis Dance: G hijaz (G Ab B C D Eb F), tempo 36, 8 bars = 512 ticks. CH1 bass, CH2 lead (CH0 / CH3 silent).
// 118 bytes
// ---------------------------------------------------------------------------
Song tuneOasis[] = {          // total song in bytes = 118
  0x06,                         // Number of tracks
  0x00, 0x00,                   // Address of track 0   //0  lead
  0x10, 0x00,                   // Address of track 1   //16  bass
  0x19, 0x00,                   // Address of track 2   //25  bb
  0x22, 0x00,                   // Address of track 3   //34  A
  0x45, 0x00,                   // Address of track 4   //69  B
  0x64, 0x00,                   // Address of track 5   //100  silent

  0x05,                         // Channel 0 entry track (silent)
  0x01,                         // Channel 1 entry track (bass)
  0x00,                         // Channel 2 entry track (lead)
  0x05,                         // Channel 3 entry track (silent)

  //"Track 0" lead  [16 bytes]
  ATM_SET_TEMPO(36),
  ATM_VOL(MV(28)),
  ATM_SL_VOL(MS(1)),
  ATM_GOTO(3),   // -> A
  ATM_GOTO(4),   // -> B
  ATM_GOTO(3),   // -> A
  ATM_GOTO(4),   // -> B
  ATM_GOTO(0),   // -> lead

  //"Track 1" bass  [9 bytes]
  ATM_VOL(MV(22)),
  ATM_SL_VOL(MS(2)),
  ATM_REPEAT(31, 2),   // 32x bb
  ATM_GOTO(1),   // -> bass

  //"Track 2" bb  [9 bytes]
  ATM_NOTE_G2,
  ATM_DELAY(4),
  ATM_NOTE_D3,
  ATM_DELAY(4),
  ATM_NOTE_G2,
  ATM_DELAY(4),
  ATM_NOTE_D3,
  ATM_DELAY(4),
  ATM_RETURN,

  //"Track 3" A  [35 bytes]
  ATM_NOTE_D4,
  ATM_DELAY(4),
  ATM_NOTE_D4_,
  ATM_DELAY(4),
  ATM_NOTE_D4,
  ATM_DELAY(4),
  ATM_NOTE_C4,
  ATM_DELAY(4),
  ATM_NOTE_B3,
  ATM_DELAY(8),
  ATM_NOTE_C4,
  ATM_DELAY(8),
  ATM_NOTE_D4,
  ATM_DELAY(16),
  ATM_NOTE_G3,
  ATM_DELAY(16),
  ATM_NOTE_G3_,
  ATM_DELAY(4),
  ATM_NOTE_B3,
  ATM_DELAY(4),
  ATM_NOTE_C4,
  ATM_DELAY(4),
  ATM_NOTE_B3,
  ATM_DELAY(4),
  ATM_NOTE_G3_,
  ATM_DELAY(8),
  ATM_NOTE_G3,
  ATM_DELAY(8),
  ATM_NOTE_D4,
  ATM_DELAY(16),
  ATM_NOTE_D4_,
  ATM_DELAY(8),
  ATM_NOTE_D4,
  ATM_DELAY(8),
  ATM_RETURN,

  //"Track 4" B  [31 bytes]
  ATM_NOTE_G4,
  ATM_DELAY(8),
  ATM_NOTE_F4,
  ATM_DELAY(8),
  ATM_NOTE_D4_,
  ATM_DELAY(8),
  ATM_NOTE_D4,
  ATM_DELAY(8),
  ATM_NOTE_C4,
  ATM_DELAY(8),
  ATM_NOTE_B3,
  ATM_DELAY(8),
  ATM_NOTE_C4,
  ATM_DELAY(8),
  ATM_NOTE_D4,
  ATM_DELAY(8),
  ATM_NOTE_D4_,
  ATM_DELAY(4),
  ATM_NOTE_D4,
  ATM_DELAY(4),
  ATM_NOTE_C4,
  ATM_DELAY(4),
  ATM_NOTE_B3,
  ATM_DELAY(4),
  ATM_NOTE_G3_,
  ATM_DELAY(8),
  ATM_NOTE_B3,
  ATM_DELAY(8),
  ATM_NOTE_G3,
  ATM_DELAY(32),
  ATM_RETURN,

  //"Track 5" silent  [1 bytes]
  ATM_STOP_CHAN,

};


// ---------------------------------------------------------------------------
// Pyramid Chase
// Pyramid Chase: C hijaz (C Db E F G Ab Bb), tempo 40, 8 bars = 512 ticks. CH1 bass, CH2 lead (CH0 / CH3 silent).
// 118 bytes
// ---------------------------------------------------------------------------
Song tuneChase[] = {          // total song in bytes = 118
  0x06,                         // Number of tracks
  0x00, 0x00,                   // Address of track 0   //0  lead
  0x10, 0x00,                   // Address of track 1   //16  bass
  0x19, 0x00,                   // Address of track 2   //25  bb
  0x22, 0x00,                   // Address of track 3   //34  A
  0x45, 0x00,                   // Address of track 4   //69  B
  0x64, 0x00,                   // Address of track 5   //100  silent

  0x05,                         // Channel 0 entry track (silent)
  0x01,                         // Channel 1 entry track (bass)
  0x00,                         // Channel 2 entry track (lead)
  0x05,                         // Channel 3 entry track (silent)

  //"Track 0" lead  [16 bytes]
  ATM_SET_TEMPO(40),
  ATM_VOL(MV(28)),
  ATM_SL_VOL(MS(1)),
  ATM_GOTO(3),   // -> A
  ATM_GOTO(4),   // -> B
  ATM_GOTO(3),   // -> A
  ATM_GOTO(4),   // -> B
  ATM_GOTO(0),   // -> lead

  //"Track 1" bass  [9 bytes]
  ATM_VOL(MV(22)),
  ATM_SL_VOL(MS(2)),
  ATM_REPEAT(31, 2),   // 32x bb
  ATM_GOTO(1),   // -> bass

  //"Track 2" bb  [9 bytes]
  ATM_NOTE_C2,
  ATM_DELAY(4),
  ATM_NOTE_C2,
  ATM_DELAY(4),
  ATM_NOTE_C2,
  ATM_DELAY(4),
  ATM_NOTE_G2,
  ATM_DELAY(4),
  ATM_RETURN,

  //"Track 3" A  [35 bytes]
  ATM_NOTE_G3,
  ATM_DELAY(4),
  ATM_NOTE_G3,
  ATM_DELAY(4),
  ATM_NOTE_G3_,
  ATM_DELAY(4),
  ATM_NOTE_G3,
  ATM_DELAY(4),
  ATM_NOTE_F3,
  ATM_DELAY(8),
  ATM_NOTE_E3,
  ATM_DELAY(8),
  ATM_NOTE_C3_,
  ATM_DELAY(8),
  ATM_NOTE_C3,
  ATM_DELAY(24),
  ATM_NOTE_C3,
  ATM_DELAY(4),
  ATM_NOTE_C3_,
  ATM_DELAY(4),
  ATM_NOTE_E3,
  ATM_DELAY(4),
  ATM_NOTE_F3,
  ATM_DELAY(4),
  ATM_NOTE_G3,
  ATM_DELAY(8),
  ATM_NOTE_G3_,
  ATM_DELAY(8),
  ATM_NOTE_A3_,
  ATM_DELAY(16),
  ATM_NOTE_G3_,
  ATM_DELAY(8),
  ATM_NOTE_G3,
  ATM_DELAY(8),
  ATM_RETURN,

  //"Track 4" B  [31 bytes]
  ATM_NOTE_C4,
  ATM_DELAY(8),
  ATM_NOTE_A3_,
  ATM_DELAY(8),
  ATM_NOTE_G3_,
  ATM_DELAY(8),
  ATM_NOTE_G3,
  ATM_DELAY(8),
  ATM_NOTE_F3,
  ATM_DELAY(8),
  ATM_NOTE_E3,
  ATM_DELAY(8),
  ATM_NOTE_F3,
  ATM_DELAY(8),
  ATM_NOTE_G3,
  ATM_DELAY(8),
  ATM_NOTE_G3_,
  ATM_DELAY(4),
  ATM_NOTE_G3,
  ATM_DELAY(4),
  ATM_NOTE_F3,
  ATM_DELAY(4),
  ATM_NOTE_E3,
  ATM_DELAY(4),
  ATM_NOTE_C3_,
  ATM_DELAY(8),
  ATM_NOTE_C3,
  ATM_DELAY(8),
  ATM_NOTE_C3,
  ATM_DELAY(32),
  ATM_RETURN,

  //"Track 5" silent  [1 bytes]
  ATM_STOP_CHAN,

};


const uint8_t *const gameTunes[] PROGMEM = { tuneSphinx, tuneCaravan, tuneHall, tuneOasis, tuneChase };
#define TUNE_COUNT 5

// ---------------------------------------------------------------------------
// sound effects
// ---------------------------------------------------------------------------
ATM_SFX_TRACK(sfxMove,   ATM_VOL(34), ATM_SL_VOL((uint8_t)-8), ATM_NOTE_D4, ATM_DELAY(2));
ATM_SFX_TRACK(sfxSelect, ATM_VOL(40), ATM_SL_VOL((uint8_t)-3), ATM_NOTE_E4, ATM_DELAY(2), ATM_NOTE_A4, ATM_DELAY(8));
ATM_SFX_TRACK(sfxJump,   ATM_VOL(44), ATM_SL_VOL((uint8_t)-3), ATM_NOTE_G3, ATM_DELAY(2), ATM_NOTE_C4, ATM_DELAY(5));
ATM_SFX_TRACK(sfxStep,   ATM_VOL(26), ATM_SL_VOL((uint8_t)-6), ATM_NOTE_D3, ATM_DELAY(2));
ATM_SFX_TRACK(sfxLand,   ATM_VOL(44), ATM_SL_VOL((uint8_t)-6), ATM_NOTE_E2, ATM_DELAY(3));
ATM_SFX_TRACK(sfxWind,   ATM_VOL(22), ATM_SL_VOL((uint8_t)-4), ATM_NOTE_E4, ATM_DELAY(2));
ATM_SFX_TRACK(sfxHurt,   ATM_VOL(46), ATM_SL_VOL((uint8_t)-2), ATM_NOTE_A4, ATM_DELAY(2), ATM_NOTE_E4, ATM_DELAY(2), ATM_NOTE_A3, ATM_DELAY(6));
ATM_SFX_TRACK(sfxKey,    ATM_VOL(44), ATM_SL_VOL((uint8_t)-2), ATM_NOTE_B4, ATM_DELAY(2), ATM_NOTE_E5, ATM_DELAY(2), ATM_NOTE_A5, ATM_DELAY(8));
ATM_SFX_TRACK(sfxCoin,   ATM_VOL(44), ATM_SL_VOL((uint8_t)-3), ATM_NOTE_B4, ATM_DELAY(1), ATM_NOTE_E5, ATM_DELAY(6));
ATM_SFX_TRACK(sfxPop,    ATM_VOL(44), ATM_SL_VOL((uint8_t)-2), ATM_NOTE_A4, ATM_DELAY(2), ATM_NOTE_E5, ATM_DELAY(6));
// the score count at the end of a level: four rising notes
ATM_SFX_TRACK(sfxCount0, ATM_VOL(40), ATM_SL_VOL((uint8_t)-3), ATM_NOTE_C4, ATM_DELAY(3));
ATM_SFX_TRACK(sfxCount1, ATM_VOL(40), ATM_SL_VOL((uint8_t)-3), ATM_NOTE_E4, ATM_DELAY(3));
ATM_SFX_TRACK(sfxCount2, ATM_VOL(40), ATM_SL_VOL((uint8_t)-3), ATM_NOTE_G4, ATM_DELAY(3));
ATM_SFX_TRACK(sfxCount3, ATM_VOL(40), ATM_SL_VOL((uint8_t)-3), ATM_NOTE_C5, ATM_DELAY(3));
const uint8_t *const sfxCountList[] PROGMEM = { sfxCount0, sfxCount1, sfxCount2, sfxCount3 };


Song silentSong[] = {
  0x01,                         // Number of tracks
  0x00, 0x00,                   // Address of track 0
  0x00, 0x00, 0x00, 0x00,       // entry track of channels 0..3
  ATM_VOL(0),
  ATM_DELAY(64),
  ATM_GOTO(0)
};
