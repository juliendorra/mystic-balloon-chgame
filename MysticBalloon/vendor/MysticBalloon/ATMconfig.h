#ifndef ATMCONFIG_H
#define ATMCONFIG_H

// Only what the title tune and the sound effects use. Everything else is left out.

// effects that are NOT used
#define ATM_FX_FREQ_SLIDE       0
#define ATM_FX_NOISE            0
#define ATM_FX_TREMOLO          0
#define ATM_FX_VIBRATO          0
#define ATM_FX_GLISSANDO        0
#define ATM_FX_NOTE_CUT         0
#define ATM_FX_CUE              0
#define ATM_FX_GOTO_ADV         0
#define ATM_FX_TRANSPOSE        0

// functions: the sketch only uses ATM.play() and ATM.playSfx() (the empty song replaces ATM.stop())
#define ATM_FUNC_STOP           0
#define ATM_FUNC_PLAYPAUSE      0
#define ATM_FUNC_PAUSE          0
#define ATM_FUNC_RESUME         0
#define ATM_FUNC_MUTE           0
#define ATM_FUNC_UNMUTE         0
#define ATM_FUNC_TEMPO          0
#define ATM_FUNC_CUE            0

// the song and the effects only use channels 0 and 1: no mixer code for channels 2 and 3
#define ATM_WAVE_CH3            ATM_WAVE_OFF

// octaves that are NOT used (C2-B2 = 2 ... C7-D7 = 7)
#define ATM_OCTAVE_6           0
#define ATM_OCTAVE_7           0

#endif
