#ifndef MENU_BITMAPS_H
#define MENU_BITMAPS_H

#include <Arduino.h>
#include "globals.h"
#include "MenuArt.h"
#include "Saves.h"
#include "SaveMenuArt.h"

#define FONT_TINY                 0
#define FONT_SMALL                1
#define FONT_BIG                  2

#define DATA_TIMER                0
#define DATA_SCORE                1
#define DATA_LEVEL                2

byte sparkleFrames = 0;
byte cont = 0;
byte settingsSelection = 0;
byte settingsOriginalOptions = 1;
byte settingsPreviewTimer = 0;
mystic::Palette settingsOriginalPalette = mystic::Color;
bool saveMenuError=false;

extern void drawNumbers(byte numbersX, byte numbersY, byte fontType, byte data);

// Back wins if both buttons are held: never accept an unwanted menu action.
static bool menuAccept() { return arduboy.justPressed(A_BUTTON) && !arduboy.pressed(B_BUTTON); }

void drawTitleScreen()
{
  arduboy.title = true;
}

void stateMenuIntro()
{
  if (menuAccept() || arduboy.justPressed(B_BUTTON))
  {
    gameState = STATE_MENU_MAIN;
    sound.tone(425, 20);
    return;
  }
  globalCounter++;
  if (globalCounter < 160)
  {
    arduboy.intro = true;
    sprites.drawSelfMasked(34, 4, T_arg, 0);
  }
  else
  {
    drawTitleScreen();
    if ((globalCounter > 250) || menuAccept() || arduboy.justPressed(B_BUTTON))
    {
      gameState = STATE_MENU_MAIN;
      sound.tone(425, 20);
    }
  }
}

void stateMenuMain()
{

  drawTitleScreen();
  sprites.drawOverwrite(51, 42, mainMenu, 0);
  if (arduboy.justPressed(DOWN_BUTTON) && (menuSelection < 5))
  {
    menuSelection++;
    sound.tone(300, 20, mystic::audio::Move);
  }
  if (arduboy.justPressed(UP_BUTTON) && (menuSelection > 2))
  {
    menuSelection--;
    sound.tone(300, 20, mystic::audio::Move);
  }
  if (menuAccept())
  {
    gameState = menuSelection;
    if (gameState==STATE_MENU_PLAY) {
      cont=mystic::saves::active<mystic::saves::count ? mystic::saves::active+1 : 0;
      saveMenuError=false;
    }
    if (gameState == STATE_MENU_SOUNDFX) {
      settingsOriginalOptions = progress.options;
      settingsOriginalPalette = mystic::palette();
      settingsSelection = settingsOriginalPalette;
      settingsPreviewTimer = 0;
    }
    sound.tone(425, 20);
  }
  sprites.drawPlusMask(46, 42 + 9 * (menuSelection - 2), selector_plus_mask, 0);
}

void stateMenuHelp()
{
  if (menuAccept() || arduboy.justPressed(B_BUTTON))
  {
    gameState = STATE_MENU_MAIN;
    sound.tone(425, 20);
  }
}


void stateMenuInfo()
{
  sprites.drawSelfMasked(43, 2, badgeMysticBalloon, 0);
  sprites.drawSelfMasked(37, 0, stars, sparkleFrames);
  sprites.drawSelfMasked(40, 48, madeBy, 0);
  scorePlayer = progress.highScore;
  if (progress.highCoins == TOTAL_COINS)
  {
    sprites.drawSelfMasked(21, 28, badgeSuper, 0);
  }
  else
  {
    sprites.drawSelfMasked(28, 28, badgeBorder, 0);
  }
  sprites.drawSelfMasked(30, 28, badgeHighScore, 0);
  drawNumbers(55, 30, FONT_BIG, DATA_SCORE);
  if (menuAccept() || arduboy.justPressed(B_BUTTON))
  {
    gameState = STATE_MENU_MAIN;
    sound.tone(425, 20);
  }
}

void stateMenuSoundfx()
{
  drawTitleScreen();
  const byte previousSelection = settingsSelection;
  if (arduboy.justPressed(DOWN_BUTTON)) settingsSelection = (settingsSelection+1)%6;
  if (arduboy.justPressed(UP_BUTTON)) settingsSelection = (settingsSelection+5)%6;
  if (settingsSelection != previousSelection)
    mystic::setPalette(settingsSelection<4 ? mystic::Palette(settingsSelection) : settingsOriginalPalette);
  const bool adjust = settingsSelection>=4 && arduboy.justPressed(LEFT_BUTTON | RIGHT_BUTTON);
  if (adjust) {
    if (settingsSelection==4)
      mystic::setAudioMode(mystic::audio::mode()==mystic::audio::Atm ? mystic::audio::Beeps : mystic::audio::Atm);
    else mystic::setSound(!arduboy.audio.enabled());
    sound.tone(300,20,mystic::audio::Move);
  }
  sprites.drawOverwrite(49, 40, mystic::art::configBlank, 0);
  for (byte i=0; i<6; ++i)
    sprites.drawSelfMasked(58, 44+i*8,
        mystic::art::labels[i<4 ? i : i==4 ? (mystic::audio::mode()==mystic::audio::Atm ? 10 : 11) : arduboy.audio.enabled() ? 4 : 5], 0);
  sprites.drawSelfMasked(96, 45+8*mystic::palette(), mystic::art::selected, 0);
  sprites.drawSelfMasked(51, 44+8*settingsSelection, mystic::art::cursor, 0);
  sprites.drawOverwrite(40,96,mystic::art::labels[14],0);
  if (menuAccept())
  {
    if (settingsSelection<4) mystic::setPalette(mystic::Palette(settingsSelection));
    sound.tone(425,20);
    gameState = STATE_MENU_MAIN;
    return;
  }
  if (arduboy.justPressed(B_BUTTON)) {
    mystic::setPalette(settingsOriginalPalette);
    mystic::setAudioMode((settingsOriginalOptions&8) ? mystic::audio::Beeps : mystic::audio::Atm);
    mystic::setSound(settingsOriginalOptions&1);
    gameState = STATE_MENU_MAIN;
    sound.tone(425, 20);
    return;
  }
  // Short, repeating gameplay samples make both modes audible without
  // leaving CONF. Muting stops previews; changing a value restarts them.
  if (settingsSelection!=previousSelection || adjust || arduboy.justPressed(SELECT_BUTTON))
    settingsPreviewTimer=0;
  if (settingsSelection<4 || !arduboy.audio.enabled() || adjust) return;
  if (settingsPreviewTimer==0) sound.tone(200,100,mystic::audio::Jump);
  if (settingsPreviewTimer==16) sound.tone(400,200,mystic::audio::Coin);
  if (settingsPreviewTimer==36) sound.tone(80,30,mystic::audio::Land);
  settingsPreviewTimer=(settingsPreviewTimer+1)%72;
}

void stateMenuPlaySelect()
{
  drawTitleScreen();
  if (mystic::saves::action!=mystic::saves::None) return;
  const unsigned rows=mystic::saves::count+1;
  if (cont>=rows) cont=0;
  if (arduboy.justPressed(DOWN_BUTTON)) { cont=(cont+1)%rows;sound.tone(300,20,mystic::audio::Move);saveMenuError=false; }
  if (arduboy.justPressed(UP_BUTTON)) { cont=(cont+rows-1)%rows;sound.tone(300,20,mystic::audio::Move);saveMenuError=false; }
  if (menuAccept())
  {
    mystic::saves::action=cont ? mystic::saves::Load : mystic::saves::New;
    sound.tone(425, 20);
  }
  if (arduboy.justPressed(B_BUTTON))
  {
    gameState = STATE_MENU_MAIN;
    sound.tone(425, 20);
  }
}

// Native overlay, drawn only after gfx_wait(): reading the save file shares
// the panel's SPI bus and the renderer's scratch buffer.
static void saveText(int x,int y,const char *s,uint8_t=1) {
  for (;*s;++s,x+=4) {
    unsigned c=uint8_t(*s);if (c<32 || c>90) continue;
    for (unsigned col=0;col<3;++col) {
      unsigned bits=saveGlyphs[(c-32)*3+col];
      for (unsigned row=0;row<5;++row,bits>>=1) if (bits&1)
        gfx_fb[(y+row)*64+(x+col)/2]|=1u<<(((x+col)&1)*4);
    }
  }
}
void paintHelpMenu() {
  memset(gfx_fb,0,GFX_FB_BYTES);
  for (unsigned y=4;y<124;++y) {
    uint8_t *row=gfx_fb+y*64+2;
    if (y==4 || y==123) memset(row,0x11,60);
    row[0]|=1;row[59]|=0x10;
  }
  saveText(56,11,"HELP");
  const char *lines[]={"LEFT/RIGHT MOVE","B JUMP","B AGAIN + HOLD FLOAT",
      "A INHALE","UP AT DOOR EXIT","FIND KEY TO UNLOCK","COINS ADD POINTS",
      "START PAUSE","PAUSE: RESTART / SAVE","PLAY: A LOAD A RUN","NEW KEEPS OTHER RUNS"};
  for (unsigned i=0;i<11;++i) saveText(10,24+i*8,lines[i]);
  saveText(42,114,"A OK  B BACK");
}
void paintSaveMenu() {
  using namespace mystic::saves;
  Store s;const bool available=read(s);
  for (unsigned y=40;y<122;++y) {
    uint8_t *row=gfx_fb+y*64+9;
    memset(row,y==40 || y==121 ? 0x11 : 0,47);
    row[0]|=1;row[46]|=0x10;
  }
  saveText(43,44,"SAVED RUNS",1);
  // NEW stays visible above an empty row. Scroll only the saved runs;
  // selection indices remain contiguous so the spacer cannot be selected.
  if (!cont) saveText(23,54,">",1);
  saveText(32,54,"NEW",1);
  const unsigned first=cont>4 ? cont-3 : 1;
  for (unsigned row=first;row<=count && row<first+4;++row) {
    const unsigned y=74+(row-first)*9;
    if (row==cont) saveText(23,y,">",1);
    if (available && row<=s.count) {
      char name[24];label(s.runs[row-1],name);saveText(32,y,name,1);
      char id[2]={char('0'+row),0};saveText(103,y,id,1);
    }
  }
  const char *hint=saveMenuError ? (count>=capacity() && !cont ? "SAVES FULL" : "SAVE FAILED") :
      status==Sd ? "SD CARD" : status==NeedsSetup ? "SET UP SD SAVES" :
      status==ForeignFlash ? "INSERT SD CARD" : status==Damaged || status==WriteFailed ? "SAVE ERROR" : "FLASH ONLY";
  saveText(24,108,hint,1);saveText(24,116,cont ? "A LOAD  B BACK" : "A NEW   B BACK",1);
}

#endif
