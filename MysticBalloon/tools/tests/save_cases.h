#include "../../SaveCard.h"
#include <unistd.h>

static void writeFixture(const char *path,const uint8_t *data,unsigned size) {
    FILE *f=fopen(path,"wb");assert(f);assert(fwrite(data,1,size,f)==size);assert(!fclose(f));
}
static void readFixture(const char *path,uint8_t *data,unsigned size) {
    FILE *f=fopen(path,"rb");assert(f);assert(fread(data,1,size,f)==size);fclose(f);
}
static void seedCard(const char *path) {
    using namespace mystic::saves;
    alignas(4) uint8_t data[1536];memset(data,255,sizeof data);
    memcpy(data,"MYBLSD2",8);*reinterpret_cast<uint32_t *>(data+8)=1536;
    *reinterpret_cast<uint32_t *>(data+12)=MAX_RUNS;
    *reinterpret_cast<uint32_t *>(data+508)=crc(data,508);
    uint8_t *p=data+512;*reinterpret_cast<uint32_t *>(p)=mystic::SAVE_MAGIC;
    p[4]=VERSION;p[5]=0;p[6]=p[7]=0;Store s;memcpy(p+8,&s,sizeof s);
    *reinterpret_cast<uint32_t *>(p+8+sizeof s)=crc(p,8+sizeof s);
    writeFixture(path,data,sizeof data);
}
static void saveSystemTest() {
    using namespace mystic;
    namespace ss=mystic::saves;
    chmbtest::clearFlash();ss::testFlashOwners(0xFFFFFFFF,0xFFFFFFFF);
    unsetenv("CHMB_SAVE_CARD");loadProgress();
    for (unsigned options=0;options<16;++options) {
        chmbtest::clearFlash();Progress old;old.level=7;old.score=9400;old.coins=32;
        old.highScore=12345;old.highCoins=40;old.options=options;
        assert(save::store(SAVE_MAGIC,1,old));loadProgress();
        assert(ss::count==1 && ss::active==0 && progress.level==7 && progress.score==9400);
        assert(mystic::audio::enabled()==bool(options&1) && mystic::audio::mode()==((options&8) ? mystic::audio::Beeps : mystic::audio::Atm));
        assert(storeProgress());ss::Store migrated;assert(ss::read(migrated));
        assert(migrated.runs[0].score==9400 && migrated.prefs.highScore==12345 && migrated.prefs.options==options);
    }
    puts("PASS: all 16 legacy option combinations migrate without losing level, score, coins or records");
    chmbtest::clearFlash();loadProgress();
    for (unsigned i=0;i<ss::FLASH_RUNS;++i) assert(ss::create());
    const auto full=saveGeneration();assert(!ss::create() && saveGeneration()==full);
    chmbtest::corruptFlash(0,40);loadProgress();
    assert(ss::count==3); // CRC fallback to the previous complete page.
    chmbtest::corruptFlash(1,40);ss::testFlashOwners(SAVE_MAGIC,SAVE_MAGIC);loadProgress();
    assert(ss::status==ss::Damaged && !ss::create());ss::testFlashOwners(0xFFFFFFFF,0xFFFFFFFF);
    const uint32_t other=save::magic("OTHR");
    assert(save::store(other,1,other));assert(save::store(other,1,other));
    ss::testFlashOwners(other,other);loadProgress();
    assert(ss::status==ss::ForeignFlash && !ss::create());
    uint32_t marker=0;assert(save::load(other,1,marker) && marker==other);
    puts("PASS: flash-full refuses replacement, damaged newest page falls back, foreign game records are never erased");
    chmbtest::clearFlash();ss::testFlashOwners(0xFFFFFFFF,0xFFFFFFFF);
    char path[]="/tmp/mybl-save-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);
    seedCard(path);setenv("CHMB_SAVE_CARD",path,1);loadProgress();assert(ss::status==ss::Sd && !ss::count);
    gameState=STATE_MENU_MAIN;menuSelection=STATE_MENU_PLAY;runSaveKind=0;
    tap(A_BUTTON);tap(A_BUTTON);finishTone();
    assert(ss::count==1 && gameState==STATE_GAME_NEXT_LEVEL);
    startLevel(5,1400,13);coins[5].active=false;coinsActive=5;coinsCollected=1;
    key.haveKey=true;key.active=false;kid.actualpos=vec2(3200,5120);kid.pos=kid.actualpos>>5;
    kid.speed=vec2(11,-17);kid.balloons=2;walkers[0].HP=12;walkers[0].pos.x+=3;
    const auto snapshot=captureRun(true);assert(ss::valid([&]{ss::Store s;s.count=1;s.active=0;s.runs[0]=snapshot;return s;}(),8));
    assert(ss::checkpoint(snapshot));char name[24];ss::label(snapshot,name);assert(!strcmp(name,"LEVEL 06 28%"));
    ss::Store before;assert(ss::read(before));
    gameState=STATE_MENU_MAIN;menuSelection=STATE_MENU_PLAY;tap(A_BUTTON);tap(UP_BUTTON);tap(A_BUTTON);finishTone();
    ss::Store after;assert(ss::read(after));assert(after.count==2 && !memcmp(&after.runs[0],&snapshot,sizeof snapshot));
    // Reopening PLAY selects the last played run. The empty line is visual,
    // never a cursor stop, and A on a saved row actually restores that run.
    gameState=STATE_MENU_MAIN;menuSelection=STATE_MENU_PLAY;tap(A_BUTTON);
    assert(cont==2);
    tap(DOWN_BUTTON);assert(cont==0);
    tap(DOWN_BUTTON);assert(cont==1);
    tap(UP_BUTTON);assert(cont==0);
    tap(DOWN_BUTTON);assert(cont==1);
    tap(A_BUTTON);finishTone();
    ss::Run chosen;
    assert(ss::active==0);
    assert(gameState==STATE_GAME_PAUSE && kid.actualpos.x==3200 && kid.actualpos.y==5120 && kid.speed.y==-17);
    assert(!coins[5].active && key.haveKey && !key.active && walkers[0].HP==12 && kid.balloons==2);
    const auto restored=captureRun(true);assert(!memcmp(&restored,&snapshot,sizeof snapshot));
    loadProgress();gameState=STATE_MENU_MAIN;menuSelection=STATE_MENU_PLAY;tap(A_BUTTON);assert(cont==1);
    tap(B_BUTTON);
    auto dead=snapshot;dead.balloons=0;dead.kidFlags|=1024;assert(ss::checkpoint(dead));
    ss::Store playable;assert(ss::read(playable));assert(!memcmp(&playable.runs[0],&snapshot,sizeof snapshot));
    assert(save::store(other,1,other));assert(save::store(other,1,other));ss::testFlashOwners(other,other);
    progress=Progress{};ss::active=ss::NO_RUN;loadProgress();assert(ss::status==ss::Sd && ss::count==2);
    assert(ss::select(0,chosen));assert(!memcmp(&chosen,&snapshot,sizeof snapshot));
    setSound(false);setPalette(Amber);setAudioMode(mystic::audio::Beeps);assert(storeProgress());
    assert(save::load(other,1,marker) && marker==other);
    puts("PASS: real NEW menu creates a separate run, labelled level/percentage; exact gameplay state and preferences survive reboot and foreign-game flash writes");
    // Retry a partially completed level through the actual START menu.
    auto retry=snapshot;retry.entryScore=1400;retry.score=1900;retry.total=14;
    assert(ss::checkpoint(retry));restoreRun(retry);
    ss::Store untouched;assert(ss::read(untouched));
    tap(UP_BUTTON);assert(pauseSelection==2); // Up wraps to SAVE & QUIT.
    tap(DOWN_BUTTON);assert(pauseSelection==0);
    tap(DOWN_BUTTON);assert(pauseSelection==1);
    card::testCutAfter(0);tap(A_BUTTON);finishTone();
    assert(gameState==STATE_GAME_PAUSE && pauseSaveFailed && kid.actualpos.x==retry.x);
    ss::Store failed;assert(ss::read(failed));assert(!memcmp(&failed.runs[0],&retry,sizeof retry));
    card::testCutAfter(-1);setSound(true);sound.tone(300,1000);
    tap(A_BUTTON);assert(pauseRestartRequested);
    tap(START_BUTTON);assert(gameState==STATE_GAME_PAUSE && pauseRestartRequested);
    setSound(false);finishTone();
    assert(gameState==STATE_GAME_NEXT_LEVEL && level==5 && scorePlayer==1400 && totalCoins==13);
    ss::Store restarted;assert(ss::read(restarted));
    assert(restarted.count==2 && restarted.active==0 && !(restarted.runs[0].objects&ss::MID_LEVEL));
    assert(ss::percent(restarted.runs[0])==0 && restarted.runs[0].entryScore==1400);
    assert(!memcmp(&restarted.runs[1],&untouched.runs[1],sizeof(ss::Run)));
    while (!canPressButton) tick();
    tick(A_BUTTON);
    assert(gameState==STATE_GAME_PLAYING && kid.balloons==3 && key.active && !key.haveKey && coinsActive==6);
    tick();coins[0].pos=kid.pos;checkCollisions();
    assert(scorePlayer>1400 && coinsCollected==1 && totalCoins==14);
    tap(START_BUTTON);tap(DOWN_BUTTON);tap(DOWN_BUTTON);assert(pauseSelection==2);
    tap(A_BUTTON);finishTone();assert(gameState==STATE_MENU_MAIN);
    loadProgress();tap(A_BUTTON);tap(A_BUTTON);finishTone();
    assert(gameState==STATE_GAME_PAUSE && levelEntryScore==1400);
    tap(DOWN_BUTTON);tap(A_BUTTON);finishTone();
    assert(gameState==STATE_GAME_NEXT_LEVEL && scorePlayer==1400 && totalCoins==13 && coinsCollected==0);
    assert(ss::read(restarted) && !memcmp(&restarted.runs[1],&untouched.runs[1],sizeof(ss::Run)));
    puts("PASS: RESTART LEVEL resets the active level and entry score after reload; failures retain the paused world and other runs never change");
    uint8_t baseline[1536];readFixture(path,baseline,sizeof baseline);ss::Store baseStore;assert(ss::read(baseStore));
    for (unsigned cut=0;cut<512;++cut) {
        writeFixture(path,baseline,sizeof baseline);loadProgress();card::testCutAfter(cut);
        ss::create();progress=Progress{};ss::active=ss::NO_RUN;loadProgress();
        ss::Store recovered;assert(ss::read(recovered));assert(recovered.count==baseStore.count || recovered.count==baseStore.count+1);
        for (unsigned i=0;i<baseStore.count;++i) assert(!memcmp(&recovered.runs[i],&baseStore.runs[i],sizeof(ss::Run)));
    }
    puts("PASS: 512 interrupted SD-write positions recover a complete old or new record without altering any existing run");
    writeFixture(path,baseline,sizeof baseline);loadProgress();
    while (ss::count<ss::MAX_RUNS) assert(ss::create());
    const auto generation=saveGeneration();assert(!ss::create() && saveGeneration()==generation);
    uint8_t damaged[1536],unchanged[1536];readFixture(path,damaged,sizeof damaged);
    damaged[512+8+sizeof(ss::Store)]^=128;damaged[1024+8+sizeof(ss::Store)]^=128;writeFixture(path,damaged,sizeof damaged);
    loadProgress();assert(ss::status==ss::Damaged && !ss::create());readFixture(path,unchanged,sizeof unchanged);
    assert(!memcmp(damaged,unchanged,sizeof damaged));
    puts("PASS: SD capacity never overwrites runs; two damaged records are reported and left untouched");
    // Recreate v1.4.0's exact byte layout, including all eight SD runs.
    ss::Store v2;v2.generation=12;v2.count=8;v2.active=6;
    for (unsigned i=0;i<8;++i) { v2.runs[i].level=i+2;v2.runs[i].score=4000+i*100;v2.runs[i].total=20+i; }
    alignas(4) uint8_t oldStore[436]={};memcpy(oldStore,&v2,20);
    for (unsigned i=0;i<8;++i) memcpy(oldStore+20+i*52,&v2.runs[i],52);
    seedCard(path);readFixture(path,damaged,sizeof damaged);memset(damaged+512,255,1024);
    auto *p=damaged+512;*reinterpret_cast<uint32_t *>(p)=SAVE_MAGIC;p[4]=2;p[5]=0;p[6]=12;p[7]=0;
    memcpy(p+8,oldStore,sizeof oldStore);*reinterpret_cast<uint32_t *>(p+444)=ss::crc(p,444);
    writeFixture(path,damaged,sizeof damaged);loadProgress();
    ss::Store upgraded;assert(ss::read(upgraded) && upgraded.count==8 && upgraded.active==6);
    for (unsigned i=0;i<8;++i) {
        assert(!memcmp(&upgraded.runs[i],&v2.runs[i],52));assert(upgraded.runs[i].entryScore==v2.runs[i].score);
    }
    unsetenv("CHMB_SAVE_CARD");chmbtest::clearFlash();ss::testFlashOwners(0xFFFFFFFF,0xFFFFFFFF);
    v2.count=4;v2.active=2;memcpy(oldStore,&v2,20);
    memcpy(save::buffer(),oldStore,228);assert(save::write(SAVE_MAGIC,2,228));loadProgress();
    assert(ss::read(upgraded) && upgraded.count==4 && upgraded.active==2);
    for (unsigned i=0;i<4;++i) assert(upgraded.runs[i].entryScore==v2.runs[i].score && upgraded.runs[i].level==v2.runs[i].level);
    assert(storeProgress());assert(save::read(SAVE_MAGIC,ss::VERSION,ss::FLASH_BYTES));
    puts("PASS: all eight SD and four flash runs migrate from 1.4.0 without losing progress or selection");
    unsetenv("CHMB_SAVE_CARD");unlink(path);ss::testFlashOwners(0xFFFFFFFF,0xFFFFFFFF);chmbtest::clearFlash();loadProgress();
}
