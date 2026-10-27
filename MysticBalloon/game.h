#ifndef GAME_H
#define GAME_H

#include <Arduino.h>
#include "globals.h"
#include "inputs.h"
#include "player.h"
#include "enemies.h"
#include "elements.h"
#include "levels.h"

#define TOTAL_TONES 10
PROGMEM const byte tones[] = {
  //200, 100, 250, 125, 300, 150, 350, 400, 425, 475
  131, 145, 139, 152, 131, 172, 200, 188, 213, 255
};

byte toneindex = 0;
byte pauseSelection = 0;
bool pauseSaveRequested = false;
bool pauseSaveFailed = false;

void stateMenuPlayNew()
{
  level = LEVEL_TO_START_WITH - 1;
  coinsCollected = 0;
  totalCoins = 0;
  balloonsLeft = 0;
  scorePlayer = 0;
  globalCounter = 0;
  kid.balloons = 3;
  canPressButton = false;
  toneindex = 0;
  gameState = STATE_GAME_NEXT_LEVEL;
  scoreIsVisible = false;
  nextLevelIsVisible = true;
  pressKeyIsVisible = false;
}

void stateMenuPlayContinue()
{
  level = progress.level;
  totalCoins = progress.coins;
  coinsCollected = 0;
  balloonsLeft = 0;
  //scorePlayer = 0;
  scorePlayer = progress.score;
  globalCounter = 0;
  kid.balloons = 3;
  canPressButton = false;
  toneindex = 0;
  gameState = STATE_GAME_NEXT_LEVEL;
  scoreIsVisible = false;
  nextLevelIsVisible = true;
  pressKeyIsVisible = false;
}


void stateGameNextLevel()
{
  //if (level < TOTAL_LEVELS)
  //{
    if (arduboy.everyXFrames(20))
    {
      canPressButton = false;
      if (coinsCollected > 0)
      {
        coinsCollected--;
        scorePlayer += 20;
        sound.tone(pgm_read_byte(tones + toneindex++), 150, mystic::audio::Count);
      }
      else if (balloonsLeft > 0)
      {
        balloonsLeft--;
        scorePlayer += 30;
        sound.tone(pgm_read_byte(tones + toneindex++), 150, mystic::audio::Count);
      }
      else
      {
        canPressButton = true;
        scoreIsVisible = false;
        pressKeyIsVisible = !pressKeyIsVisible;
        if (toneindex < TOTAL_TONES)
        {
          sound.tone(pgm_read_byte(tones + toneindex++), 200, mystic::audio::CountEnd);
          toneindex = TOTAL_TONES;
        }
        if (level >= TOTAL_LEVELS)
          gameState = STATE_GAME_OVER;
      }
    }
  /*}
  else
  {
    gameState = STATE_GAME_OVER;
    return;
  }*/

  // Stage the checkpoint; CHGame commits after the bonus tally finishes.
  progress.level = level;
  progress.coins = totalCoins;
  progress.score = scorePlayer;


  //if (nextLevelIsVisible)
  //{
  if (level < TOTAL_LEVELS)
  {
    sprites.drawSelfMasked(35, 4, badgeNextLevel, 0);
    drawNumbers(78, 13, FONT_BIG, DATA_LEVEL);
  }
  else
  {
    progress.level = LEVEL_TO_START_WITH - 1;
    // Score remains after completing game? (no)
    progress.score = 0;
    progress.coins = 0;
  }
  drawNumbers(43, 49, FONT_BIG, DATA_SCORE);
  //}

  if (scoreIsVisible)
  {
    byte totalBadges = coinsCollected + balloonsLeft;

    for (byte i = 0; i < totalBadges; ++i)
    {
      if (i < coinsCollected) sprites.drawOverwrite(65 - (7 * totalBadges) + (i * 14), 27, badgeElements, 0);
      else sprites.drawOverwrite(65 - (7 * totalBadges) + (i * 14), 27, badgeElements, 1);
    }
  }

  if (canPressButton)
  {

    if (pressKeyIsVisible) sprites.drawOverwrite(38, 29, badgePressKey, 0);
    if (arduboy.justPressed(A_BUTTON | B_BUTTON))
    {
      toneindex = 0;
      sound.tone(425, 20);
      setKid();
      enemiesInit();
      levelLoad(levels[level]);
      kid.pos = kid.actualpos >> FIXED_POINT;
      resetCamera();
      gameState = STATE_GAME_PLAYING;
    }
  }
};


void stateGamePlaying()
{
  checkInputs();
  checkKid();
  updateCamera();

  drawGrid();
  enemiesUpdate();

  drawKid();
  drawHUD();

  checkCollisions();
}


void stateGamePause()
{
  if (arduboy.justPressed(UP_BUTTON | DOWN_BUTTON)) pauseSelection ^= 1;
  sprites.drawOverwrite(20, 36, mystic::art::pausePanel, 0);
  sprites.drawOverwrite((mystic::VIEW_W - badgePause[0]) / 2,
                        38, badgePause, 0);
  sprites.drawSelfMasked(43, 64, mystic::art::labels[6], 0);
  sprites.drawSelfMasked(43, 74, mystic::art::labels[7], 0);
  sprites.drawSelfMasked(32, 64+10*pauseSelection, mystic::art::cursor, 0);
  const uint8_t *hint=mystic::art::labels[pauseSaveFailed ? 9 : 8];
  sprites.drawSelfMasked((mystic::VIEW_W-hint[0])/2, 84, hint, 0);
  if (arduboy.justPressed(A_BUTTON)) gameState = STATE_GAME_PLAYING;
  else if (arduboy.justPressed(B_BUTTON)) {
    if (!pauseSelection) gameState = STATE_GAME_PLAYING;
    else {
      if (scorePlayer > progress.highScore) {
        progress.highScore = scorePlayer;
        progress.highCoins = totalCoins;
      }
      pauseSaveRequested = true; // Commit only after the previous DMA ends.
    }
  }
}


void stateGameOver()
{
  byte x = 35 + 12;
  if (level < TOTAL_LEVELS)
  {
    drawNumbers(78, 26, FONT_BIG, DATA_LEVEL);
    x -= 12;
  }
  sprites.drawSelfMasked(x, 17, badgeGameOver, 0);
  drawNumbers(43, 49, FONT_BIG, DATA_SCORE);

  uint32_t highscore = progress.highScore;
  if (scorePlayer > highscore) {
    progress.highCoins = totalCoins;
    progress.highScore = scorePlayer;
  }

  if (arduboy.justPressed(A_BUTTON | B_BUTTON))
  {
    gameState = STATE_MENU_MAIN;
  }
}

#endif
