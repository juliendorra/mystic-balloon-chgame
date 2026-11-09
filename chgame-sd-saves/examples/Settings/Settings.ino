// SPDX-License-Identifier: MIT
#include <CHGame.h>
#include <CHSave.h>

chsave::Store saves;
constexpr auto SAVE_ID = chsave::id("DEMO");
struct Data {
    uint32_t bestScore;
    uint8_t sound;
    uint8_t reserved[3];
};
Data data{0, 1, {0, 0, 0}};
bool saveFailed = false;

void setup() {
    chgame.boot();
    gfx_begin(GFX_DIV2, GFX_12BPP);
    const uint16_t palette[] = {0x0000, 0xf800, 0x07e0, 0x001f};
    gfx_setPalette(palette, 4);
    chgame.setFrameRate(30);
    saves.load(SAVE_ID, 1, data); // keeps defaults on Empty or any error
}

void loop() {
    if (!chgame.nextFrame()) return;
    chgame.pollButtons();
    if (chgame.justPressed(A_BUTTON)) {
        data.sound ^= 1;
        saveFailed = !saves.store(SAVE_ID, 1, data);
    }
    // Red = write failed. Green = sound on. Blue = sound off.
    gfx_clear(saveFailed ? 1 : (data.sound ? 2 : 3));
    gfx_flush();
}
