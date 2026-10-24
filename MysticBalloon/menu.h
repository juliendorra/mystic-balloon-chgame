#ifndef MENU_BITMAPS_H
#define MENU_BITMAPS_H

#include <Arduino.h>
#include "globals.h"
#include "MenuArt.h"

#define FONT_TINY                 0
#define FONT_SMALL                1
#define FONT_BIG                  2

#define DATA_TIMER                0
#define DATA_SCORE                1
#define DATA_LEVEL                2

byte sparkleFrames = 0;
byte cont = 0;
byte settingsSelection = 0;
mystic::Palette settingsOriginalPalette = mystic::Color;

extern void drawNumbers(byte numbersX, byte numbersY, byte fontType, byte data);

void drawTitleScreen()
{
  arduboy.title = true;
}

void stateMenuIntro()
{
  if (arduboy.justPressed(A_BUTTON | B_BUTTON))
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
    if ((globalCounter > 250) || arduboy.justPressed(A_BUTTON | B_BUTTON))
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
    sound.tone(300, 20);
  }
  if (arduboy.justPressed(UP_BUTTON) && (menuSelection > 2))
  {
    menuSelection--;
    sound.tone(300, 20);
  }
  if (arduboy.justPressed(A_BUTTON | B_BUTTON))
  {
    gameState = menuSelection;
    if (gameState == STATE_MENU_SOUNDFX) {
      settingsOriginalPalette = mystic::palette();
      settingsSelection = settingsOriginalPalette;
    }
    sound.tone(425, 20);
  }
  sprites.drawPlusMask(46, 42 + 9 * (menuSelection - 2), selector_plus_mask, 0);
}

void stateMenuHelp()
{
  if (arduboy.justPressed(A_BUTTON | B_BUTTON))
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
  if (arduboy.justPressed(A_BUTTON | B_BUTTON))
  {
    gameState = STATE_MENU_MAIN;
    sound.tone(425, 20);
  }
}

void stateMenuSoundfx()
{
  drawTitleScreen();
  const byte previousSelection = settingsSelection;
  if (arduboy.justPressed(DOWN_BUTTON)) settingsSelection = (settingsSelection+1)%5;
  if (arduboy.justPressed(UP_BUTTON)) settingsSelection = (settingsSelection+4)%5;
  if (settingsSelection != previousSelection && settingsSelection<4)
    mystic::setPalette(mystic::Palette(settingsSelection));
  sprites.drawOverwrite(49, 40, mystic::art::configBlank, 0);
  for (byte i=0; i<5; ++i)
    sprites.drawSelfMasked(58, 44+i*8,
        mystic::art::labels[i<4 ? i : arduboy.audio.enabled() ? 4 : 5], 0);
  sprites.drawSelfMasked(96, 45+8*mystic::palette(), mystic::art::selected, 0);
  sprites.drawSelfMasked(51, 44+8*settingsSelection, mystic::art::cursor, 0);
  if (arduboy.justPressed(B_BUTTON))
  {
    if (settingsSelection<4) mystic::setPalette(mystic::Palette(settingsSelection));
    else {
      mystic::setPalette(settingsOriginalPalette);
      mystic::setSound(!arduboy.audio.enabled());
    }
    sound.tone(425, 20);
    gameState = STATE_MENU_MAIN;
  }
  if (arduboy.justPressed(A_BUTTON)) {
    mystic::setPalette(settingsOriginalPalette);
    gameState = STATE_MENU_MAIN;
  }
}

void stateMenuPlaySelect()
{
  drawTitleScreen();
  sprites.drawOverwrite(53, 58, continueMenu, 0);
  if (arduboy.justPressed(DOWN_BUTTON))
  {
    cont = 1;
    sound.tone(300, 20);
  }
  if (arduboy.justPressed(UP_BUTTON))
  {
    cont = 0;
    sound.tone(300, 20);
  }
  sprites.drawPlusMask(48, 58 + 9 * cont, selector_plus_mask, 0);
  if (arduboy.justPressed(B_BUTTON))
  {
    gameState = STATE_GAME_PLAYCONTNEW + cont;
    cont = 0;
    sound.tone(425, 20);
  }
  if (arduboy.justPressed(A_BUTTON))
  {
    gameState = STATE_MENU_MAIN;
    sound.tone(425, 20);
  }
}


#endif
