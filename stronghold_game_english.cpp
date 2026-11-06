// Mini Stronghold 3 - castle RTS inspired by Stronghold Crusader (C++17 + raylib)
// Build from the tabs at the bottom, then click the map.  Q/W/E/K/D/Z train soldiers (Shift = train all, Ctrl = train 5).
// Drag = select | Right-click = move | A = select all | -/= tax | [ ] rations | P pause | TAB speed | O options | F11 full screen
#include "raylib.h"
#include "MainMenu.h"
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <queue>
#include <functional>
#include <random>
#include <cstring>
using namespace std;

const int TS = 24, MW = 56, MH = 34, HUD = 160, SW = 960, MAPH = 528, SH = MAPH + HUD;  // SW x SH is the virtual screen (scaled to the real window)
const int PKX = 4, PKY = 16;     // our keep (tile of its top-left corner)
const int EKX = 46, EKY = 16;    // enemy keep (top-left tile); its castle is built around it by the AI

// ---------------- data tables ----------------
enum BType { KEEP, WOOD, QUARRY, IRON, WHEAT, HOUSE, FLETCHER, POLE, SMITH, BARRACKS, TENT, TOWER, WALL, GATE, CHURCH, INN, MARKET, MILL, RTOWER, STORAGE, GRANARY,
             APPLE, GRAPE, DATE, BAKERY, WINERY, CANDLE, SIEGE, ENGGUILD, HEALER, HUNTER, GARDEN, NB };
struct BInfo { const char *name, *tag; int wood, stone, gold, hp; Color col; };
const BInfo INFO[NB] = {
    {"Keep", "Kp", 0, 0, 0, 1500, {170, 140, 80, 255}},
    {"Woodcutter", "Wd", 10, 0, 0, 120, BROWN},
    {"Quarry", "Qr", 20, 0, 0, 120, GRAY},
    {"Iron Mine", "Fe", 20, 0, 0, 120, {120, 100, 130, 255}},
    {"Wheat Farm", "Wf", 20, 0, 0, 120, GOLD},
    {"House", "Hs", 20, 0, 0, 100, ORANGE},
    {"Fletcher", "Fl", 20, 0, 0, 120, {200, 170, 110, 255}},
    {"Poleturner", "Pt", 20, 0, 0, 120, {180, 140, 90, 255}},
    {"Blacksmith", "Bk", 20, 20, 0, 150, {90, 90, 100, 255}},
    {"Barracks", "Br", 20, 30, 0, 250, MAROON},
    {"War Tent", "Te", 0, 0, 90, 160, {145, 70, 45, 255}},
    {"Tower", "Tw", 20, 50, 0, 350, LIGHTGRAY},
    {"Wall", "Wl", 0, 8, 0, 300, DARKGRAY},
    {"Gate", "Gt", 20, 10, 0, 300, {120, 80, 40, 255}},
    {"Church", "Ch", 0, 40, 250, 320, RAYWHITE},
    {"Inn", "In", 20, 0, 100, 220, {150, 90, 60, 255}},
    {"Market", "Mk", 20, 0, 0, 150, {200, 160, 80, 255}},
    {"Grain Mill", "Ml", 20, 0, 0, 180, {225, 205, 160, 255}},
    {"Round Tower", "Rt", 20, 50, 0, 450, {205, 165, 105, 255}},
    {"Storage Yard", "St", 20, 10, 0, 140, {150, 105, 60, 255}},
    {"Granary", "Gr", 20, 0, 0, 130, {185, 150, 70, 255}},
    {"Apple Orchard", "Ap", 20, 0, 0, 110, {200, 60, 50, 255}},
    {"Grape Farm", "Gp", 15, 0, 0, 110, {110, 50, 150, 255}},
    {"Date Farm", "Dt", 15, 0, 0, 110, {190, 110, 40, 255}},
    {"Bakery", "Bk", 20, 0, 0, 160, {210, 140, 80, 255}},
    {"Winery", "Wn", 20, 0, 50, 170, {140, 60, 120, 255}},
    {"Candle Maker", "Cm", 20, 0, 30, 120, {235, 210, 120, 255}},
    {"Siege Workshop", "Sw", 20, 30, 100, 240, {130, 95, 60, 255}},
    {"Engineer Guild", "Eg", 20, 40, 150, 240, {110, 120, 135, 255}},
    {"Healer", "Hl", 20, 0, 60, 160, {240, 240, 235, 255}},
    {"Hunter's Hut", "Hn", 20, 0, 0, 120, {120, 150, 80, 255}}, {"Garden", "Gd", 12, 0, 0, 90, {70, 145, 65, 255}}};
static_assert(NB == 32, "Building table changed unexpectedly");
// Keep construction prices approachable: no building exceeds 20 wood or 50 stone.
const int SZ[NB] = {2, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 2, 1, 1, 1, 2, 2, 2, 2, 2, 1, 2, 2, 1, 1, 1};  // footprint in tiles (n x n)
inline int sizeOf(int t) { return SZ[t]; }
const bool WORKS[NB] = {0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0};  // needs a peasant to work
const float BH[NB] = {90, 14, 12, 16, 3, 22, 22, 22, 22, 28, 42, 70, 34, 36, 64, 40, 34, 48, 56, 24, 32, 40, 30, 40, 40, 40, 24, 44, 40, 38, 34, 8};  // fallback heights, used for mouse picking

enum UType { ARCHER, SPEAR, SWORD, SLAVE, EARCHER, ESWORD, CATA, EHORSE, HORSE_ARCHER, KNIGHT, MACE, CROSSBOW, ENGINEER, RAM, LONGBOW, PIKEMAN, ARAB_SWORD, ARAB_GUARD, CAMEL_ARCHER, DEMOLISHER, NU };
struct UInfo { const char* name; float hp, dmg, range, speed, cd; Color col; };
const UInfo UINFO[NU] = {
    {"Archer", 40, 12, 130, 55, 1.2f, {60, 110, 200, 255}},   {"Spearman", 60, 9, 18, 60, 0.8f, {70, 150, 200, 255}},
    {"Swordsman", 100, 16, 18, 45, 1.0f, {40, 80, 170, 255}}, {"Slave", 25, 5, 16, 55, 0.8f, {200, 90, 60, 255}},
    {"Arab Archer", 35, 9, 120, 50, 1.3f, {210, 120, 40, 255}}, {"Assassin", 90, 14, 18, 50, 1.0f, {150, 30, 30, 255}},
    {"Catapult", 80, 45, 220, 20, 3.0f, {110, 70, 40, 255}}, {"Horseman", 70, 12, 20, 95, 0.9f, {140, 100, 60, 255}},
    {"Horse Archer", 55, 11, 145, 72, 1.15f, {185, 125, 55, 255}},
    {"Knight", 170, 24, 21, 88, 1.0f, {150, 160, 185, 255}}, {"Maceman", 95, 22, 18, 50, 1.3f, {90, 110, 150, 255}},
    {"Crossbowman", 48, 20, 140, 46, 2.0f, {60, 140, 120, 255}}, {"Engineer", 30, 2, 14, 52, 1.0f, {200, 170, 80, 255}},
    {"Battering Ram", 240, 70, 16, 28, 1.7f, {120, 85, 55, 255}},
    {"Longbowman", 52, 17, 155, 48, 1.35f, {55, 125, 75, 255}},
    {"Pikeman", 85, 14, 20, 48, 1.0f, {80, 125, 155, 255}},
    {"Arab Swordsman", 115, 20, 19, 52, 0.95f, {185, 115, 55, 255}},
    {"Arab Guard", 145, 23, 19, 43, 1.05f, {155, 85, 45, 255}},
    {"Camel Archer", 85, 18, 165, 78, 1.1f, {205, 155, 65, 255}},
    {"Demolisher", 330, 95, 18, 24, 2.0f, {105, 75, 45, 255}}};

struct Building { int type, x, y; float hp, t, fire; bool dead, working, enemy; int style; bool fed = false; int lord = -1, own = -1; float prog = 1.f; };   // prog: construction progress 0..1
const float BUILD_TIME[NB] = {0, 3, 3.5f, 4, 5, 3.5f, 4, 4, 5, 7, 4, 8, 1.5f, 3, 11, 7, 5, 7, 10, 3.5f, 4, 4.5f, 4.5f, 4.5f, 6, 6, 4, 8, 8, 5, 4, 3};  // seconds to build
inline bool built(const Building& b) { return b.prog >= 1.f; }
struct Unit { Vector2 p, dst; int type; bool enemy, moving, sel; float hp, cd, face, anim; bool guard; vector<Vector2> path; Vector2 goal; float repT, stuck; int own = -1; bool army = false; };
struct Proj { Vector2 a, b; float t, len, dmg; bool enemy, siege; };
struct HorseDeathFX { Vector2 p; float age; float face; };
vector<HorseDeathFX> horseDeathFX;

// ---------------- training table ----------------
struct TInfo { int unit; const char* name; int res; int gold, wood; const char* key; };  // res: 0 bows, 1 spears, 2 swords, -1 none
const TInfo TR[] = {
    {ARCHER, "Archer", 0, 10, 0, "Q"}, {CROSSBOW, "Crossbowman", 0, 16, 0, "Z"}, {SPEAR, "Spearman", 1, 7, 0, "W"}, {MACE, "Maceman", 1, 12, 0, "D"},
    {SWORD, "Swordsman", 2, 18, 0, "E"}, {KNIGHT, "Knight", 2, 55, 0, "K"},
    {LONGBOW, "Longbowman", 0, 22, 0, ""}, {PIKEMAN, "Pikeman", 1, 15, 0, ""},
    {SLAVE, "Arab Slave", -1, 5, 0, ""}, {EARCHER, "Arab Archer", -1, 18, 0, ""}, {ESWORD, "Assassin", -1, 32, 0, ""},
    {ARAB_SWORD, "Arab Swordsman", -1, 28, 0, ""}, {ARAB_GUARD, "Arab Guard", -1, 42, 0, ""},
    {EHORSE, "Arab Horseman", -1, 48, 0, ""}, {HORSE_ARCHER, "Horse Archer", -1, 38, 0, ""}, {CAMEL_ARCHER, "Camel Archer", -1, 100, 0, ""},
    {ENGINEER, "Engineer", -1, 40, 0, ""}, {CATA, "Catapult", -1, 100, 20, ""}, {RAM, "Battering Ram", -1, 80, 20, ""}, {DEMOLISHER, "Demolisher", -1, 150, 20, ""}};
const int NTR = 20;
const int ROSTER_BARRACKS[] = {0, 1, 2, 3, 4, 5, 6, 7};
const int ROSTER_TENT[] = {8, 9, 10, 11, 12, 13, 14, 15};
const int ROSTER_SIEGE[] = {17, 18, 19};
const int ROSTER_ENG[] = {16};

// ---------------- game state ----------------
vector<Building> B; vector<Unit> U; vector<Proj> P;
int occ[MH][MW];  // -1 empty, -2 tree, -4 water, >=0 building index
unsigned char ter[MH][MW];  // 0 sand, 1 grass, 2 rocky sand, 3 water
float gtime = 0;
// goods: wood stone iron gold | bread apples dates wine | bows spears swords | grain flour grapes candles
int meat = 0;
int wood, stone, iron, gold, bread, apples, dates, wine, bows, spears, swords, grain, flour, grapes, candles, pop, wave, kills;
int popularity, tax, ration, cap, pbonus, employed, speedMul, innServed, churchServed;
float ecoT, waveT, bannerT; string banner;
bool gameOver, won, paused;
int keepStyle = 0; bool inMenu = true;
int difficulty = 1; bool enemyOn[5] = {true, true, true, true, true};  // custom game: slaves, archers, assassins, horsemen, catapults
int mapStyle = 0, lordStyle = 13, opponentCount = 1;   // eight new skirmish lords; up to three opponents on the current map
int curMission = -1, campDone = 0; bool tutDone = false;   // curMission: -1 custom game, 0 tutorial, 1..10 campaign
bool autoTrade = true, enemyBuilding = false;
bool autoSellUpTo = false; // false = keep reserve (sell > limit), true = sell anything up to limit
const int NG = 10;  // market goods: wood, stone, iron, bread, apples, dates, wine, bows, spears, swords
bool autoBuyOn[NG]  = {true, false, true, true, false, false, false, false, false, false};
int  autoBuyLim[NG] = {50,   20,    10,   50,   10,    10,    10,    10,    10,    10};
bool autoSellOn[NG] = {false, false, false, false, false, false, false, false, false, false};
int  autoSellLim[NG]= {100,   50,    50,    100,   20,    20,    20,    20,    20,    20};
float autoTradeT = 0.f;
float enemyBuildT = 0, warningT = 0; string warning;
int selectedBuilding = -1;
int totalFood() { return bread + apples + dates + meat; }
bool hasGranary() { for (const auto& b : B) if (!b.dead && !b.enemy && b.type == GRANARY && b.prog >= 1.f) return true; return false; }
int gardenCount() { int n = 0; for (const auto& b : B) if (!b.dead && !b.enemy && b.type == GARDEN && b.prog >= 1.f) n++; return n; }
bool cheatInstant = false, cheatGod = false, cheatUsed = false, cheatOpen = false; int cheatCap = 0;   // cheat menu state (only usable when Options > Cheats is ON)

// ---------------- settings (saved to settings.cfg) ----------------
struct Settings {
    float master = 0.8f, music = 0.6f, sfx = 0.8f;
    bool fullscreen = true; int winScale = 0;           // 0 = Full HD, 1 = QHD 1440p, 2 = 4K UHD
    bool vsync = true; int fpsCap = 1;                   // 0 = 30, 1 = 60, 2 = 120, 3 = unlimited
    int quality = 2;                                     // 0 low, 1 medium, 2 high (presets)
    bool shadows = true, clouds = true, vignette = true, detail = true, smooth = true, showFps = false;
    int particles = 2;                                   // 0 off, 1 few, 2 all
    bool cheats = false;                                 // Options > Cheats
} cfg;
const char* CFG_FILE = "settings.cfg";
void saveSettings() {
    FILE* f = fopen(CFG_FILE, "w"); if (!f) return;
    fprintf(f, "%.2f %.2f %.2f %d %d %d %d %d %d %d %d %d %d %d\n", cfg.master, cfg.music, cfg.sfx, cfg.fullscreen, cfg.winScale, cfg.vsync, cfg.fpsCap, cfg.quality,
            cfg.shadows, cfg.clouds, cfg.vignette, cfg.detail, cfg.smooth, cfg.particles);
    fprintf(f, "%d\n", cfg.cheats ? 1 : 0);
    fclose(f);
}
void loadSettings() {
    FILE* f = fopen(CFG_FILE, "r"); if (!f) return;
    int fs, vs, sh, cl, vg, dt, sm; float a, b, c;
    if (fscanf(f, "%f %f %f %d %d %d %d %d %d %d %d %d %d %d", &a, &b, &c, &fs, &cfg.winScale, &vs, &cfg.fpsCap, &cfg.quality, &sh, &cl, &vg, &dt, &sm, &cfg.particles) == 14) {
        cfg.master = a; cfg.music = b; cfg.sfx = c; cfg.fullscreen = fs; cfg.vsync = vs; cfg.shadows = sh; cfg.clouds = cl; cfg.vignette = vg; cfg.detail = dt; cfg.smooth = sm;
        cfg.winScale = max(0, min(2, cfg.winScale)); cfg.fpsCap = max(0, min(3, cfg.fpsCap)); cfg.quality = max(0, min(2, cfg.quality)); cfg.particles = max(0, min(2, cfg.particles));
        int ch = 0; if (fscanf(f, "%d", &ch) == 1) cfg.cheats = ch != 0;
    }
    fclose(f);
}
void applyQuality(int q) {
    cfg.quality = q;
    cfg.shadows = q >= 1; cfg.clouds = q >= 2; cfg.vignette = q >= 1; cfg.detail = q >= 1; cfg.smooth = q >= 1; cfg.particles = q;
}

// ---------------- audio (WAV files in assets/sfx and assets/music) ----------------
enum { SFX_CLICK, SFX_PLACE, SFX_COIN, SFX_ARROW, SFX_SWORD, SFX_HIT, SFX_BOOM, SFX_CATAPULT, SFX_HORN, SFX_BELL, SFX_TRAIN, SFX_VICTORY, SFX_DEFEAT, SFX_DENY, SFX_REPAIR, SFX_COLLAPSE, SFX_MSG, SFX_VOICE, SFX_VVOICE, SFX_HAMMER, SFX_HEAL, SFX_BUILT, NSFX };
const char* SFX_FILES[NSFX] = {"click", "place", "coin", "arrow", "sword", "hit", "boom", "catapult", "horn", "bell", "train", "victory", "defeat", "deny", "repair", "collapse", "msg", "voice", "victory_voice", "hammer", "heal", "built"};
Sound SFX[NSFX]; bool sfxOk[NSFX]; float sfxCool[NSFX];
Music MUS[3]; bool musOk[3]; int curMusic = -1; float musFade[3] = {0, 0, 0}; bool audioOk = false;
void initAudio() {
    InitAudioDevice(); audioOk = IsAudioDeviceReady(); if (!audioOk) return;
#ifdef _WIN32
    // The "Victory!" voice: if the PC has a Windows speech voice, render it once to a WAV file (needs no internet). A built-in voice is the fallback.
    if (!FileExists("assets/sfx/victory_voice_sapi.wav"))
        system("powershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command \"try { Add-Type -AssemblyName System.Speech; $s = New-Object System.Speech.Synthesis.SpeechSynthesizer; $s.Rate = -1; $s.Volume = 100; $s.SetOutputToWaveFile('assets\\sfx\\victory_voice_sapi.wav'); $s.Speak('Victory!'); $s.Dispose() } catch {}\" >nul 2>&1");
#endif
    for (int i = 0; i < NSFX; i++) { string fn = string("assets/sfx/") + SFX_FILES[i] + ".wav"; if (i == SFX_VVOICE && FileExists("assets/sfx/victory_voice_sapi.wav")) fn = "assets/sfx/victory_voice_sapi.wav";
        SFX[i] = LoadSound(fn.c_str()); sfxOk[i] = SFX[i].frameCount > 0; sfxCool[i] = 0; }
    const char* mf[3] = {"menu", "peace", "battle"};
    for (int i = 0; i < 3; i++) { MUS[i] = LoadMusicStream((string("assets/music/music_") + mf[i] + ".wav").c_str()); musOk[i] = MUS[i].frameCount > 0; if (musOk[i]) { MUS[i].looping = true; SetMusicVolume(MUS[i], 0); } }
}
void sfx(int id, float vol = 1.f, float gap = 0.05f) {
    if (!audioOk || !sfxOk[id] || sfxCool[id] > 0) return;
    sfxCool[id] = gap; SetSoundVolume(SFX[id], vol * cfg.sfx); SetSoundPitch(SFX[id], 0.94f + (rand() % 120) / 1000.f); PlaySound(SFX[id]);
}
void sfxP(int id, float vol, float pitch, float gap = 0.f) {   // exact pitch (used for the lords' talking voices and the Victory voice)
    if (!audioOk || !sfxOk[id] || sfxCool[id] > 0) return;
    sfxCool[id] = gap; SetSoundVolume(SFX[id], vol * cfg.sfx); SetSoundPitch(SFX[id], pitch); PlaySound(SFX[id]);
}
void musicTick(float dt, int want) {  // want: 0 menu, 1 peaceful game, 2 battle; tracks cross-fade
    if (!audioOk) return;
    SetMasterVolume(cfg.master);
    for (int i = 0; i < NSFX; i++) sfxCool[i] -= dt;
    for (int i = 0; i < 3; i++) {
        if (!musOk[i]) continue;
        float target = (i == want) ? 1.f : 0.f; musFade[i] += max(-dt * 0.6f, min(dt * 0.6f, target - musFade[i]));
        if (musFade[i] > 0.001f) { if (!IsMusicStreamPlaying(MUS[i])) PlayMusicStream(MUS[i]); UpdateMusicStream(MUS[i]); SetMusicVolume(MUS[i], musFade[i] * cfg.music * 0.8f); }
        else if (IsMusicStreamPlaying(MUS[i])) StopMusicStream(MUS[i]);
    }
}

// ---------------- sprite assets (PNG files in the ./assets folder) ----------------
// Everything falls back to code-drawn graphics if a file is missing.
struct Spr { Texture2D t{}; bool ok = false; };
enum { S_CASTLE, S_WOOD, S_QUARRY, S_IRON, S_HOUSE, S_MAPBG, S_WHEAT, S_APPLE, S_GRAPE, S_DATE, S_CHURCH, S_INN, S_WINERY, S_BAKERY, S_MILL, S_SAWMILL, S_ADVISOR, S_BOOK, NSPR };
Spr SP[NSPR], TREE[8], ROCK[14];
Spr MENU_ART; // User-provided KINGS OF MESOPOTAMIA main-menu artwork
Texture2D LORD_CARD[3], LORD_FACE[3]; bool lordsOk = false, rocksOk = false;
Texture2D HORSE_ARCHER_FR[4], ARAB_LORD_WALK[7], ARAB_LORD_ATTACK[4];
Texture2D ENGLISH_LORD_WALK[6], ENGLISH_LORD_ATTACK[7];
const char* TREE_FILES[8] = {"oak", "poplar", "pine", "willow", "palm", "cherry", "maple", "acacia"};
const char* LORD_FILES[3] = {"sultan", "wolf", "wazir"};
RenderTexture2D screenRT;
Texture2D WTEX[5][5]; bool workersOk = false;   // worker animation frames: [lumberjack, wheat, apple, date, grape][idle, walk, work1, work2, carry]

Texture2D loadTex(const string& file, bool mip) {
    Texture2D t = LoadTexture((string("assets/") + file).c_str());
    if (t.id) { if (mip && cfg.smooth) { GenTextureMipmaps(&t); SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR); } else SetTextureFilter(t, cfg.smooth ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT); }
    return t;
}
void loadAssets() {
    static const char* sn[NSPR] = {"castle.png", "woodcutter.png", "quarry.png", "iron_mine.png", "house.png", "menu_map.png", "farm_wheat.png", "farm_apple.png", "farm_grape.png", "farm_date.png",
                                   "church.png", "inn.png", "winery.png", "bakery.png", "mill.png", "sawmill.png", "advisor.png", "scribe_book.png"};
    for (int i = 0; i < NSPR; i++) { SP[i].t = loadTex(sn[i], i != S_MAPBG && i != S_BOOK); SP[i].ok = SP[i].t.id != 0; }
    // This artwork is stored in the project root (next to the game source), not ./assets.
    MENU_ART.t = LoadTexture("Kings of Mesopotamia Main Menu.png");
    MENU_ART.ok = MENU_ART.t.id != 0;
    if (MENU_ART.ok) SetTextureFilter(MENU_ART.t, TEXTURE_FILTER_BILINEAR);
    for (int i = 0; i < 8; i++) { TREE[i].t = loadTex(string("tree_") + TREE_FILES[i] + ".png", true); TREE[i].ok = TREE[i].t.id != 0; }
    rocksOk = true;
    for (int i = 0; i < 14; i++) { ROCK[i].t = loadTex("rock_" + to_string(i) + ".png", true); ROCK[i].ok = ROCK[i].t.id != 0; rocksOk = rocksOk && ROCK[i].ok; }
    { static const char* wn[5] = {"lumber", "wheat", "apple", "date", "grape"}, *fn[5] = {"idle", "walk", "work1", "work2", "carry"}; workersOk = true;
      static const char* lf[5] = {"idle", "walk", "chop1", "chop2", "carry"};
      for (int k = 0; k < 5; k++) for (int f = 0; f < 5; f++) {
          WTEX[k][f] = loadTex(string("workers/") + wn[k] + "_" + (k == 0 ? lf[f] : fn[f]) + ".png", true);
          if (!WTEX[k][f].id && k == 3 && f == 3) WTEX[k][f] = WTEX[k][2];   // the date picker has one work frame
          if (!WTEX[k][f].id) workersOk = false; } }
    lordsOk = true;
    for (int i = 0; i < 3; i++) {
        LORD_CARD[i] = loadTex(string("lord_") + LORD_FILES[i] + "_card.png", false); LORD_FACE[i] = loadTex(string("lord_") + LORD_FILES[i] + "_face.png", false);
        lordsOk = lordsOk && LORD_CARD[i].id && LORD_FACE[i].id;
    }
    for (int i = 0; i < 4; ++i) HORSE_ARCHER_FR[i] = loadTex("units/horse_archer_" + to_string(i) + ".png", false);
    for (int i = 0; i < 7; ++i) ARAB_LORD_WALK[i] = loadTex("units/arab_lord_walk_" + to_string(i) + ".png", false);
    for (int i = 0; i < 4; ++i) ARAB_LORD_ATTACK[i] = loadTex("units/arab_lord_attack_" + to_string(i) + ".png", false);
    for (int i = 0; i < 6; ++i) ENGLISH_LORD_WALK[i] = loadTex("units/english_lord_walk_" + to_string(i) + ".png", false);
    for (int i = 0; i < 7; ++i) ENGLISH_LORD_ATTACK[i] = loadTex("units/english_lord_attack_" + to_string(i) + ".png", false);
}
// draws a sprite with its bottom-centre at (cx, by); pass w or h (the other is 0)
void drawSprite(const Texture2D& t, float cx, float by, float w, float h, bool flip = false, Color tint = WHITE) {
    if (w <= 0) w = h * t.width / t.height; else h = w * t.height / t.width;
    DrawTexturePro(t, Rectangle{0, 0, flip ? -(float)t.width : (float)t.width, (float)t.height}, Rectangle{cx - w / 2, by - h, w, h}, Vector2{0, 0}, 0, tint);
}
// which sprite (and size) a building type uses; w = drawn width, dy = how far below the footprint centre the sprite's bottom tip sits
bool sprDef(int type, int& id, float& w, float& dy) {
    switch (type) {
        case WOOD: id = S_WOOD; w = 62; dy = 9; return true;       case QUARRY: id = S_QUARRY; w = 84; dy = 11; return true;
        case IRON: id = S_IRON; w = 64; dy = 10; return true;     case HOUSE: id = S_HOUSE; w = 54; dy = 9; return true;
        case WHEAT: id = S_WHEAT; break;   case APPLE: id = S_APPLE; break;   case GRAPE: id = S_GRAPE; break;   case DATE: id = S_DATE; break;
        case CHURCH: id = S_CHURCH; break; case INN: id = S_INN; break;       case WINERY: id = S_WINERY; break; case BAKERY: id = S_BAKERY; break;
        case MILL: id = S_MILL; break;     case SIEGE: id = S_SAWMILL; break; default: return false;
    }
    w = 104; dy = 26; return true;
}

// ---------------- the enemy lords (campaign has 10 different ones + the tutorial steward) ----------------
struct Lord {
    const char *name, *title, *d1, *d2; Color tint;
    float hpMul, buildT;                // soldier HP multiplier, seconds between construction steps
    int wt[8];                          // how often each unit type is trained: slave, archer, assassin, horseman, horse archer, catapult, knight, crossbow
    int cataBase, cataDiv;              // catapult cap in an army = cataBase + attacks / cataDiv
    const char* taunt[3]; Color stoneCol; int keepKind, dome;
    int doctrine = 0; const char* emblem = "Crown"; // doctrine drives economy/army priorities
};
const int NLORD = 21;
const Lord LORDS[NLORD] = {
    {"The Sultan", "Rich and cunning", "Domed desert fortress. Many archers.", "Builds his walls very quickly.", {255, 232, 190, 255}, 1.0f, 4.f,
     {1, 3, 1, 1, 3, 1, 1, 2}, 1, 3, {"Your gold will soon be mine!", "My archers darken the sky!", "Bow before the Sultan!"}, {222, 190, 130, 255}, 0, 1},
    {"The Wolf", "Ruthless warlord", "Great stone keep with 2 walls. Knights!", "Tough soldiers.", {196, 204, 228, 255}, 1.1f, 7.f,
     {1, 1, 3, 3, 1, 1, 3, 1}, 1, 4, {"The wolves are hungry tonight!", "No mercy. No retreat.", "Your walls will not save you!"}, {168, 160, 152, 255}, 1, 0},
    {"The Wazir", "Scheming siege master", "Round double-ring fortress. Siege.", "Catapults and slave swarms.", {228, 192, 238, 255}, 1.0f, 5.5f,
     {3, 1, 1, 1, 1, 3, 1, 2}, 2, 2, {"The sands will bury your castle.", "Every stone has its price...", "Patience. Your walls will fall."}, {214, 182, 128, 255}, 2, 0},
    {"The Old Steward", "A sleepy caretaker", "A tiny walled yard.", "Almost harmless - practice on him.", {215, 205, 190, 255}, 0.7f, 6.f,
     {3, 1, 0, 0, 0, 0, 0, 0}, 0, 9, {"Oh dear, oh dear...", "Please go away!", "I only wanted a quiet life!"}, {170, 150, 120, 255}, 5, 0},
    {"The Rat", "Sneaky bandit chief", "Wooden-style fort. Slaves and a few archers.", "Weak, but he never stops raiding.", {210, 180, 130, 255}, 0.8f, 5.f,
     {4, 2, 1, 0, 0, 0, 0, 0}, 0, 9, {"Squeak! Hand over the loot!", "Rats are everywhere!", "You cannot catch me!"}, {150, 125, 95, 255}, 4, 0},
    {"The Snake", "Poisonous archer", "Small round fort. Archers and assassins.", "Strikes fast, hides behind walls.", {175, 225, 165, 255}, 0.9f, 5.f,
     {2, 3, 2, 0, 0, 0, 0, 1}, 0, 9, {"Sssso you came...", "My arrows are poisoned!", "Ssssurrender!"}, {150, 175, 130, 255}, 3, 0},
    {"The Pig", "Greedy glutton", "Big square keep with an inn and barracks.", "Lots of swordsmen and a few horsemen.", {245, 190, 195, 255}, 1.0f, 5.f,
     {2, 2, 3, 1, 0, 0, 0, 1}, 0, 9, {"Oink! More food for the Pig!", "I will eat your castle!", "Nobody out-eats the Pig!"}, {195, 165, 155, 255}, 5, 0},
    {"The Caliph", "Master of horse", "Round domed city. Horse archers galore.", "Fast raiders that ride around your walls.", {255, 225, 140, 255}, 1.1f, 5.f,
     {1, 2, 1, 3, 4, 2, 1, 2}, 1, 3, {"My riders are faster than the wind!", "Run, little castle, run!", "The desert is mine!"}, {232, 208, 150, 255}, 2, 2},
    {"The Emir", "Cold strategist", "Walled fortress with an inner keep.", "Assassins, knights and crossbows.", {160, 230, 220, 255}, 1.15f, 4.5f,
     {1, 2, 3, 2, 2, 1, 3, 3}, 1, 4, {"Every move has been planned.", "You are outmatched.", "Calculated. Inevitable."}, {212, 172, 112, 255}, 0, 1},
    {"The Marshal", "Iron general", "Strong castle, siege engines and knights.", "Disciplined armies with catapults.", {180, 200, 255, 255}, 1.2f, 4.5f,
     {1, 2, 2, 2, 1, 3, 4, 3}, 2, 3, {"Form the line! Forward!", "Discipline wins wars.", "Your castle will fall to my engines!"}, {182, 178, 190, 255}, 1, 0},
    {"The Emperor", "Master of the realm", "Vast fortress. Everything he has.", "Knights, siege and crossbows in great numbers.", {225, 175, 255, 255}, 1.3f, 4.f,
     {1, 3, 2, 3, 3, 3, 4, 4}, 3, 2, {"Kneel before your Emperor!", "I have ten thousand men!", "An empire is not built by the weak!"}, {162, 146, 178, 255} , 3, 2},
    {"The Arabic Lord", "Desert Commander", "Animated Arabic commander with fast cavalry.", "Horse archers and mobile desert troops.", {205, 145, 80, 255}, 1.1f, 4.5f,
     {1, 2, 1, 2, 5, 1, 1, 2}, 1, 3, {"The desert rides with me!", "My horse archers never miss!", "Welcome to my lands!"}, {170, 105, 65, 255}, 4, 2},
    {"The English Lord", "English Commander", "Armoured king with a royal shield.", "Disciplined knights and archers.", {180, 195, 220, 255}, 1.15f, 5.f,
     {1, 3, 1, 2, 1, 1, 4, 2}, 1, 3, {"For the crown!", "Hold the line!", "The kingdom will prevail!"}, {135, 145, 165, 255}, 5, 0},
    // Eight original skirmish lords: doctrines influence economy and military priorities.
    {"The Sultan", "Arabian • Balanced ruler", "Fortified desert capital; mixed infantry, archers and horsemen.", "Builds steadily, then attacks with a balanced force.", {238, 198, 120, 255}, 1.08f, 4.8f, {2,4,1,2,2,1,2,2}, 1,3, {"The desert kingdoms stand united!", "My army is ready, stranger.", "The Sultan does not retreat!"}, {190,145,85,255}, 4,2, 0, "Crescent"},
    {"The Desert Falcon", "Arabian • Raider", "Compact fortress with cavalry-focused recruitment.", "Fast build cycle and frequent mobile attacks.", {220,170,95,255}, 1.0f, 3.8f, {1,1,0,5,5,0,1,1}, 0,5, {"You cannot catch the Falcon!", "My riders are already upon you!", "Strike swiftly!"}, {165,115,65,255}, 4,2, 1, "Falcon"},
    {"The Emir", "Arabian • Prosperous trader", "Market-led settlement with farms and gardens.", "Builds a larger economy before fielding elite guards.", {150,210,185,255}, 1.12f, 5.2f, {1,2,1,1,2,1,2,3}, 1,4, {"Trade brings strength.", "My granaries are full.", "A rich kingdom is hard to conquer."}, {175,145,100,255}, 0,1, 2, "Palm"},
    {"The Desert Guardian", "Arabian • Fortress keeper", "Layered round walls, towers and defensive infantry.", "Prioritizes crossbows and waits for a strong garrison.", {185,175,130,255}, 1.25f, 5.8f, {1,3,1,0,1,1,1,5}, 2,3, {"My walls will hold.", "Come closer, and meet my archers.", "The fortress remains!"}, {145,125,95,255}, 3,0, 3, "Shield"},
    {"The Lionheart", "English • Crusading commander", "Large stone castle with knights and siege support.", "Builds a strong barracks and launches organized assaults.", {190,90,75,255}, 1.25f, 4.2f, {1,2,1,1,0,3,5,3}, 2,3, {"For the crown and the realm!", "Knights, advance!", "Your walls shall break!"}, {145,125,120,255}, 5,0, 4, "Lion"},
    {"The Iron Duke", "English • Defensive lord", "Stone ring fortress with towers and crossbowmen.", "Slow expansion, high durability and defensive reserves.", {125,145,175,255}, 1.35f, 6.0f, {1,2,1,0,0,2,2,5}, 2,2, {"Steel and stone endure.", "My crossbows are in position.", "You shall not pass!"}, {115,125,145,255}, 1,0, 3, "Anvil"},
    {"The Wolf", "English • Unpredictable raider", "Compact mixed castle and flexible army.", "Changes between infantry, cavalry and siege pressure.", {145,155,175,255}, 1.12f, 3.9f, {2,2,3,3,2,2,2,2}, 2,3, {"The pack is moving!", "I see weakness in your walls.", "The hunt is not over!"}, {115,120,130,255}, 1,0, 5, "Wolf"},
    {"The Rich Baron", "English • Industrial magnate", "Industry, market and weapon production.", "Builds up resources and fields expensive trained troops.", {190,175,115,255}, 1.18f, 5.0f, {1,2,1,1,1,2,4,4}, 2,3, {"Gold and iron win kingdoms.", "My armouries are full.", "Wealth has its own army."}, {150,135,100,255}, 5,0, 7, "Coin"}};

// ---------------- missions ----------------
struct MEnemy { int lord, kx, ky; };
struct Mission {
    const char *name, *brief, *goal; int map, n; MEnemy en[11];
    float hpMul, buildMul, trainInt, atkInt, firstAtk; int armyBase; float armyGrow; int armyMax; int wood0, stone0, gold0; int stars;
};
// castle slots: A = (46,7) top right, B = (46,25) bottom right, C = (26,16) middle, M = (46,16) the lone enemy
const Mission MISSIONS[11] = {
    {"Tutorial: The Old Steward", "Learn the basics: build, farm, recruit and fight. The Old Steward is harmless - follow the instructions in the corner of the screen.",
     "Follow the steps, then destroy the Steward's keep.", 0, 1, {{3, 46, 16}}, 0.6f, 2.0f, 30, 400, 700, 2, 0.5f, 6, 500, 300, 800, 0},
    {"1. The Rat", "A sneaky bandit has built a fort in the east. He is weak, but he trains slaves and archers non-stop.",
     "Destroy The Rat's keep.", 0, 1, {{4, 46, 16}}, 0.7f, 1.6f, 14, 75, 130, 3, 0.7f, 14, 150, 80, 300, 1},
    {"2. The Snake", "The Snake lurks in a round fort in the desert. Beware his poisoned arrows and assassins.",
     "Destroy The Snake's keep.", 2, 1, {{5, 46, 16}}, 0.8f, 1.4f, 12, 70, 120, 4, 1.0f, 16, 150, 80, 300, 1},
    {"3. The Pig", "The Pig has a fat castle, an inn and plenty of swordsmen. Build solid defences before he arrives.",
     "Destroy The Pig's keep.", 1, 1, {{6, 46, 16}}, 0.9f, 1.2f, 11, 65, 110, 4, 1.2f, 20, 150, 80, 300, 2},
    {"4. The Sultan", "The Sultan hides behind golden domes. His archers and horse archers will test your walls.",
     "Destroy The Sultan's keep.", 0, 1, {{0, 46, 16}}, 1.0f, 1.0f, 10, 60, 100, 5, 1.5f, 24, 150, 80, 300, 2},
    {"5. The Wolf", "The Wolf commands a huge double-walled keep and sends knights. Bring crossbows, maces and your own knights.",
     "Destroy The Wolf's keep.", 1, 1, {{1, 46, 16}}, 1.1f, 0.9f, 9, 55, 95, 6, 1.6f, 28, 150, 80, 300, 3},
    {"6. The Wazir and The Rat", "The Wazir, a siege master, has allied with The Rat. TWO enemy lords attack at once!",
     "Destroy BOTH enemy keeps.", 2, 2, {{2, 46, 7}, {4, 46, 25}}, 1.05f, 1.0f, 9, 60, 100, 6, 1.5f, 24, 200, 100, 400, 3},
    {"7. The Caliph and The Snake", "The Caliph's horse archers and The Snake's assassins attack from two sides. Use towers and catapults.",
     "Destroy BOTH enemy keeps.", 0, 2, {{7, 46, 7}, {5, 46, 25}}, 1.1f, 0.9f, 8, 55, 90, 7, 1.7f, 22, 200, 100, 400, 4},
    {"8. The Emir, The Pig and The Sultan", "Three lords have joined forces. The Emir plans every move. Hold the line - and strike back at the weakest.",
     "Destroy ALL three enemy keeps.", 2, 3, {{8, 46, 7}, {6, 46, 25}, {0, 26, 16}}, 1.15f, 0.9f, 8, 55, 90, 6, 1.6f, 18, 250, 120, 500, 4},
    {"9. The Marshal, The Wolf and The Wazir", "The Marshal's siege engines, The Wolf's knights and The Wazir's catapults. A very hard fight.",
     "Destroy ALL three enemy keeps.", 2, 3, {{9, 26, 16}, {1, 46, 7}, {2, 46, 25}}, 1.25f, 0.8f, 7, 50, 80, 7, 1.9f, 22, 250, 120, 500, 5},
    {"10. The Emperor's Armies", "The Emperor and his two greatest vassals, The Caliph and The Emir. This is the final war. Good luck.",
     "Defeat the Emperor and ten enemy lords. Eleven castles stand against you in the final war.", 2, 11, {{10, 26, 16}, {7, 46, 7}, {8, 46, 25}, {0, 46, 2}, {1, 46, 13}, {2, 46, 31}, {3, 36, 3}, {4, 36, 27}, {5, 30, 5}, {6, 30, 27}, {9, 18, 3}}, 1.35f, 0.7f, 6, 45, 70, 8, 2.2f, 26, 300, 150, 600, 5}};
Mission cur = MISSIONS[1];
// custom game difficulty presets: easy / normal / hard
Mission makeCustom() {
    Mission m = MISSIONS[4]; m.name = "Custom skirmish"; m.map = mapStyle;
    static const int SX[3] = {46, 46, 26}, SY[3] = {16, 7, 16};
    m.n = max(1, min(3, opponentCount));
    for (int i = 0; i < m.n; ++i) m.en[i] = {(13 + ((lordStyle - 13 + i) % 8)), SX[i], SY[i]};
    static const float hp[4] = {0.8f, 1.f, 1.25f, 1.5f}, bm[4] = {1.45f, 1.0f, 0.82f, 0.68f}, ti[4] = {15, 10, 7, 5}, ai[4] = {90, 62, 50, 38}, fa[4] = {145, 90, 70, 55}, ag[4] = {0.8f, 1.5f, 2.f, 2.5f};
    static const int ab[4] = {3, 5, 6, 8}, am[4] = {14, 26, 34, 42};
    difficulty = max(0, min(3, difficulty));
    m.hpMul = hp[difficulty]; m.buildMul = bm[difficulty]; m.trainInt = ti[difficulty]; m.atkInt = ai[difficulty]; m.firstAtk = fa[difficulty]; m.armyBase = ab[difficulty]; m.armyGrow = ag[difficulty]; m.armyMax = am[difficulty];
    m.wood0 = 150; m.stone0 = 80; m.gold0 = 300; return m;
}
string tauntLine = "";

struct Part { float x, y, z, vx, vy, vz, life, max; int kind; };  // kind: 0 dust, 1 spark, 2 smoke, 3 debris
vector<Part> PT;
void addPart(int kind, float x, float y, float z, int n = 1) {
    if (cfg.particles == 0) return; if (cfg.particles == 1) n = (n + 1) / 2;
    for (int i = 0; i < n && PT.size() < 500; i++) {
        float a = (rand() % 628) / 100.f, sp = (kind == 1 ? 40 : kind == 3 ? 50 : 8) * (rand() % 100) / 100.f, life = kind == 2 ? 1.6f : 0.6f;
        PT.push_back({x, y, z, cosf(a) * sp, sinf(a) * sp, kind == 2 ? 14.f : (kind == 1 || kind == 3) ? 40.f + rand() % 40 : 6.f, life, life, kind});
    }
}

float dist(Vector2 a, Vector2 b) { return hypotf(a.x - b.x, a.y - b.y); }
Vector2 center(const Building& b) { float s = sizeOf(b.type) * TS / 2.0f; return {b.x * TS + s, b.y * TS + s}; }
void say(const string& s) { banner = s; bannerT = 4; }
void warnEnemy(const string& s) { warning = s; warningT = 4.5f; say(s); }
// ---------------- messages from the enemy lords (portrait clip + voice + text, shown on the map) ----------------
struct EnemyMsg { int lord; string text; float t, dur; int kind; };   // kind: 0 taunt, 1 attack, 2 castle complete, 3 defeated, 4 intro
vector<EnemyMsg> MQ; EnemyMsg curMsg; bool msgActive = false; int msgShown = 0; float taunT = 70;
float victoryT = 0; bool victoryVoice = false;
const char* INTRO_LINES[3] = {"So you dare to challenge me? Fool!", "This land belongs to me. Leave now!", "You will not leave here alive."};
const char* COMPLETE_LINES[3] = {"My castle is complete. Come and see!", "Now my walls are ready for war!", "Let the siege begin!"};
const char* DEFEAT_LINES[3] = {"Impossible... my castle! Noooo!", "You will regret this!", "I am defeated... but you will pay!"};
void clearMsgs() { MQ.clear(); msgActive = false; msgShown = 0; }
void enemyMsg(int lord, const string& text, int kind = 0, bool urgent = false) {
    if (urgent) MQ.insert(MQ.begin(), {lord, text, 0.f, 0.f, kind}); else if (MQ.size() < 3) MQ.push_back({lord, text, 0.f, 0.f, kind});
    if (urgent && msgActive) msgActive = false;   // an urgent message replaces the one on screen
}
float lordPitch(int l) { static const float P[21] = {1.05f,0.72f,0.95f,1.30f,1.45f,1.20f,0.82f,1.00f,0.90f,0.78f,0.62f,0.92f,0.85f,1.08f,1.18f,0.96f,1.12f,0.82f,0.76f,0.88f,0.94f}; return P[max(0,min(20,l))]; }
void tickMsg(float dt) {
    if (!msgActive && !MQ.empty()) { curMsg = MQ.front(); MQ.erase(MQ.begin()); curMsg.t = 0; curMsg.dur = 2.4f + (float)curMsg.text.size() * 0.045f + 2.4f; msgActive = true; msgShown = 0; sfx(SFX_MSG, 0.7f, 0.f); }
    if (!msgActive) return;
    curMsg.t += dt; int shown = min((int)curMsg.text.size(), (int)(curMsg.t * 26));
    while (msgShown < shown) { msgShown++; if (msgShown % 2 == 0 && curMsg.text[msgShown - 1] != ' ') sfxP(SFX_VOICE, 0.5f, lordPitch(curMsg.lord) * (0.94f + (rand() % 13) / 100.f)); }
    if (curMsg.t >= curMsg.dur) msgActive = false;
}

// ---------------- enemy lords in a running game ----------------
struct BP { int type, x, y; };
struct Enemy {
    int lord, kx, ky, ek = -1, rad = 10; vector<BP> bpl; vector<int> idx;          // blueprint (build order) and the building index of every finished piece
    float wood = 60, stone = 25, gold = 40, buildT = 0, trainT = 0, atkT = 0; bool done = false, defeated = false;
    int gx = 0, gy = 0, wave = 0, trained = 0; vector<Vector2> guards; Vector2 rally{0, 0};
};
vector<Enemy> EN;
int liveEnemies() { int n = 0; for (auto& e : EN) if (!e.defeated) n++; return n; }

bool canPlace(int type, int x, int y) {
    int s = sizeOf(type);
    for (int j = 0; j < s; j++) for (int i = 0; i < s; i++)
        if (x + i < 0 || y + j < 0 || x + i >= MW || y + j >= MH || occ[y + j][x + i] != -1) return false;
    return true;
}
bool place(int type, int x, int y, bool enemy = false, int style = 0) {
    if (!canPlace(type, x, y)) return false;
    B.push_back({type, x, y, (float)INFO[type].hp, 0, 0, false, false, enemy, style});
    B.back().prog = (type == KEEP || (cheatInstant && cfg.cheats && !enemy)) ? 1.f : 0.f;   // every building is built stage by stage (see drawConstruction)
    int s = sizeOf(type);
    for (int j = 0; j < s; j++) for (int i = 0; i < s; i++) occ[y + j][x + i] = (int)B.size() - 1;
    return true;
}
bool allDefeatedCheck = false;
void killB(int idx) {
    Building& b = B[idx]; if (b.dead) return;
    b.dead = true; int s = sizeOf(b.type); addPart(3, center(b).x, center(b).y, 12, 18); addPart(2, center(b).x, center(b).y, 14, 8);
    for (int j = 0; j < s; j++) for (int i = 0; i < s; i++) occ[b.y + j][b.x + i] = -1;
    sfx(SFX_COLLAPSE, b.type == KEEP ? 1.f : 0.6f, 0.15f);
    if (b.type == KEEP) {
        if (b.enemy && b.own >= 0 && b.own < (int)EN.size()) {
            Enemy& e = EN[b.own]; e.defeated = true; int own = b.own;
            for (auto& u : U) if (u.own == own) u.hp = 0;                       // his army scatters
            for (size_t i = 0; i < B.size(); i++) if (B[i].own == own && !B[i].dead) killB((int)i);   // and his castle crumbles
            enemyMsg(e.lord, DEFEAT_LINES[rand() % 3], 3, true);
            if (liveEnemies() == 0) { won = true; victoryT = 0; victoryVoice = false; say("All enemy lords have been defeated!"); sfx(SFX_VICTORY, 1.f, 0.f); }
            else say(string(LORDS[e.lord].name) + " has been defeated! " + to_string(liveEnemies()) + " enemy lord(s) left.");
        }
        else if (!b.enemy) { gameOver = true; sfx(SFX_DEFEAT, 1.f, 0.f); }
    }
}
// nearest free tile centre around a point (used for spawning soldiers so they never start inside a building)
Vector2 freeNear(Vector2 p) {
    int tx = (int)(p.x / TS), ty = (int)(p.y / TS);
    for (int r = 0; r < 8; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        if (max(abs(dx), abs(dy)) != r) continue; int x = tx + dx, y = ty + dy;
        if (x >= 0 && y >= 0 && x < MW && y < MH && occ[y][x] == -1) return {(x + 0.5f) * TS, (y + 0.5f) * TS};
    }
    return p;
}

// ---------------- castle blueprints ----------------
struct Ov { int x, y, t; };
void ringRect(vector<BP>& defs, vector<BP>& walls, int x0, int y0, int x1, int y1, const vector<Ov>& ov) {
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        if (x != x0 && x != x1 && y != y0 && y != y1) continue;
        int t = WALL; for (auto& o : ov) if (o.x == x && o.y == y) t = o.t;
        (t == WALL ? walls : defs).push_back({t, x, y});
    }
}
void ringCircle(vector<BP>& defs, vector<BP>& walls, float cx, float cy, float r, int gateRowY, const vector<float>& towerAngles, bool gateAtWest) {
    vector<BP> ring;
    for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) if (fabsf(hypotf(x + 0.5f - cx, y + 0.5f - cy) - r) < 0.62f) ring.push_back({WALL, x, y});
    int gMin = 999; if (gateAtWest) for (auto& t : ring) if (t.y == gateRowY && t.x < gMin) gMin = t.x;
    for (auto& t : ring) if (gateAtWest && t.y == gateRowY && t.x <= gMin + 1) t.type = GATE;
    for (float a : towerAngles) {
        float ax = cx + cosf(a * 3.14159f / 180) * r, ay = cy + sinf(a * 3.14159f / 180) * r; int bi = -1; float bd = 1e9f;
        for (size_t i = 0; i < ring.size(); i++) if (ring[i].type == WALL) { float d = hypotf(ring[i].x + 0.5f - ax, ring[i].y + 0.5f - ay); if (d < bd) { bd = d; bi = (int)i; } }
        if (bi >= 0) ring[bi].type = RTOWER;
    }
    for (auto& t : ring) (t.type == WALL ? walls : defs).push_back(t);
}
// generic castle description used by the lords that have no hand-made layout
struct Cas { int shape; float a, b, ia; int towers, domed; vector<int> first, inside; };   // shape 0 rect (a = half width, b = half height) / 1 round (a = radius); ia = inner ring size (0 = none)
const Cas CAS[NLORD] = {
    {}, {}, {},
    /*Steward*/ {0, 4, 4, 0, 0, 0, {WOOD}, {HOUSE}},
    /*Rat*/     {0, 5, 4, 0, 0, 0, {WOOD, QUARRY, HOUSE}, {TENT, HOUSE, STORAGE}},
    /*Snake*/   {1, 5.6f, 0, 0, 0, 1, {WOOD, QUARRY, HOUSE}, {TENT, MARKET, HOUSE}},
    /*Pig*/     {0, 5, 5, 0, 1, 0, {WOOD, QUARRY, HOUSE}, {BARRACKS, INN, TENT, HOUSE, HOUSE, MARKET}},
    /*Caliph*/  {1, 7.2f, 0, 3.6f, 0, 1, {WOOD, QUARRY, HOUSE}, {BARRACKS, TENT, SIEGE, MARKET, HOUSE, HOUSE}},
    /*Emir*/    {0, 7, 6, 3, 2, 1, {WOOD, QUARRY, HOUSE, BARRACKS}, {TENT, BARRACKS, SMITH, POLE, HOUSE, HOUSE, MARKET}},
    /*Marshal*/ {0, 6, 6, 3, 2, 0, {WOOD, WOOD, QUARRY, HOUSE, BARRACKS}, {TENT, BARRACKS, SIEGE, CHURCH, HOUSE, HOUSE, SMITH}},
    /*Emperor*/ {0, 8, 7, 3, 2, 1, {WOOD, WOOD, QUARRY, QUARRY, HOUSE, BARRACKS}, {BARRACKS, TENT, SIEGE, INN, CHURCH, SMITH, POLE, HOUSE, HOUSE, HOUSE, MARKET}},
    /*Arabic Lord*/ {1, 7.0f, 0, 3.4f, 0, 1, {WOOD, QUARRY, HOUSE, BARRACKS}, {TENT, BARRACKS, SIEGE, MARKET, HOUSE, HOUSE}},
    /*English Lord*/ {0, 7, 6, 3, 2, 0, {WOOD, WOOD, QUARRY, HOUSE, BARRACKS}, {BARRACKS, TENT, SIEGE, CHURCH, HOUSE, HOUSE, SMITH}},
    /*The Sultan*/ {0, 7, 6, 2.5f, 2, 1, {WOOD, QUARRY, WHEAT, HOUSE, BARRACKS}, {TENT, BARRACKS, TOWER, MARKET, HOUSE, HOUSE, GARDEN}},
    /*Desert Falcon*/ {0, 5, 4, 0, 1, 0, {WOOD, HOUSE, TENT}, {TENT, HOUSE, STORAGE}},
    /*The Emir*/ {0, 7, 6, 3, 2, 1, {WOOD, QUARRY, WHEAT, HOUSE, MARKET}, {GRANARY, APPLE, DATE, MARKET, GARDEN, GARDEN}},
    /*Desert Guardian*/ {1, 7.5f, 0, 4.0f, 0, 1, {WOOD, QUARRY, HOUSE, BARRACKS}, {BARRACKS, TOWER, TOWER, TENT, STORAGE}},
    /*Lionheart*/ {0, 8, 7, 3, 2, 0, {WOOD, WOOD, QUARRY, HOUSE, BARRACKS}, {BARRACKS, SIEGE, SMITH, TENT, CHURCH}},
    /*Iron Duke*/ {0, 7, 6, 3, 2, 0, {WOOD, QUARRY, BARRACKS}, {TOWER, TOWER, BARRACKS, STORAGE, SMITH}},
    /*The Wolf*/ {1, 6.0f, 0, 2.8f, 0, 1, {WOOD, QUARRY, HOUSE, TENT}, {BARRACKS, TENT, SIEGE, MARKET, HOUSE}},
    /*Rich Baron*/ {0, 8, 7, 3, 2, 0, {WOOD, QUARRY, WHEAT, HOUSE, MARKET}, {GRANARY, MILL, BAKERY, SMITH, POLE, MARKET, GARDEN}}};
void genGeneric(Enemy& e, const Cas& d, vector<BP>& first, vector<BP>& defs, vector<BP>& walls, vector<BP>& rest) {
    int cx = e.kx + 1, cy = e.ky + 1; static bool res[MH][MW], ins[MH][MW]; memset(res, 0, sizeof res); memset(ins, 0, sizeof ins);
    vector<BP> idefs, iwalls; vector<pair<int, int>> cells;
    if (d.shape == 0) {
        int hw = (int)d.a, hh = (int)d.b, x0 = cx - hw, x1 = cx + hw - 1, y0 = cy - hh, y1 = cy + hh - 1, ct = d.domed ? RTOWER : TOWER;
        vector<Ov> ov = {{x0, y0, ct}, {x1, y0, ct}, {x0, y1, ct}, {x1, y1, ct}, {x0, cy, GATE}};
        if (d.towers >= 1) { ov.push_back({cx - 1, y0, TOWER}); ov.push_back({cx - 1, y1, TOWER}); ov.push_back({x1, cy, TOWER}); }
        if (d.towers >= 2) { for (auto o : vector<Ov>{{x0, cy - 2, TOWER}, {x0, cy + 2, TOWER}, {cx + 3, y0, TOWER}, {cx + 3, y1, TOWER}, {x1, cy - 3, TOWER}, {x1, cy + 3, TOWER}, {cx - 5, y0, TOWER}, {cx - 5, y1, TOWER}}) ov.push_back(o); }
        ringRect(defs, walls, x0, y0, x1, y1, ov); e.gx = x0; e.gy = cy;
        if (d.ia > 0) { int ih = (int)d.ia; ringRect(idefs, iwalls, cx - ih, cy - ih, cx + ih - 1, cy + ih - 1, {{cx - ih, cy - ih, TOWER}, {cx + ih - 1, cy - ih, TOWER}, {cx - ih, cy + ih - 1, TOWER}, {cx + ih - 1, cy + ih - 1, TOWER}, {cx - ih, cy, GATE}});
            for (int y = cy - ih; y < cy + ih; y++) for (int x = cx - ih; x < cx + ih; x++) if (x >= 0 && y >= 0 && x < MW && y < MH) res[y][x] = true; }
        for (int y = y0 + 1; y < y1; y++) for (int x = x0 + 1; x < x1; x++) { ins[y][x] = true; cells.push_back({x, y}); }
    } else {
        ringCircle(defs, walls, (float)cx, (float)cy, d.a, cy, {0, 60, 120, 240, 300, 165, 195}, true);
        e.gx = 999; for (auto& t : defs) if (t.type == GATE && t.x < e.gx) { e.gx = t.x; e.gy = t.y; }
        if (d.ia > 0) { ringCircle(idefs, iwalls, (float)cx, (float)cy, d.ia, cy, {45, 135, 225, 315}, true);
            for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) if (hypotf(x + 0.5f - cx, y + 0.5f - cy) < d.ia + 0.9f) res[y][x] = true; }
        for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) if (hypotf(x + 0.5f - cx, y + 0.5f - cy) < d.a - 1.0f) { ins[y][x] = true; cells.push_back({x, y}); }
    }
    auto resv = [&](const vector<BP>& v) { for (auto& t : v) res[t.y][t.x] = true; };
    resv(defs); resv(walls); resv(idefs); resv(iwalls);
    for (int y = cy - 2; y <= cy + 1; y++) for (int x = cx - 2; x <= cx + 1; x++) if (x >= 0 && y >= 0 && x < MW && y < MH) res[y][x] = true;   // keep + guards
    for (int x = e.gx; x <= cx; x++) res[cy][x] = true; for (int x = e.gx; x <= cx; x++) res[cy - 1][x] = true;                                      // corridor from the gate to the keep
    mt19937 rng(e.lord * 977 + e.kx * 13 + e.ky); shuffle(cells.begin(), cells.end(), rng);
    auto put = [&](int t, vector<BP>& out) {
        int s = sizeOf(t);
        for (auto& c : cells) {
            bool ok = true; for (int j = 0; j < s && ok; j++) for (int i = 0; i < s; i++) { int x = c.first + i, y = c.second + j; if (x >= MW || y >= MH || !ins[y][x] || res[y][x]) { ok = false; break; } }
            if (!ok) continue;
            for (int j = 0; j < s; j++) for (int i = 0; i < s; i++) res[c.second + j][c.first + i] = true;
            out.push_back({t, c.first, c.second}); return;
        }
    };
    for (int t : d.first) put(t, first);
    for (auto& t : idefs) rest.push_back(t); for (auto& t : iwalls) rest.push_back(t);
    for (int t : d.inside) put(t, rest);
}
void genBlueprint(Enemy& e) {
    e.bpl.clear(); e.guards.clear();
    vector<BP> first, defs, walls, rest;
    int cx = e.kx + 1, cy = e.ky + 1;      // the keep covers tiles cx-1..cx, cy-1..cy
    auto A = [](vector<BP>& v, int t, int x, int y) { v.push_back({t, x, y}); };
    auto guards4 = [&]() { e.guards = {{(cx - 2) * TS + 12.f, (cy - 2) * TS + 12.f}, {(cx + 1) * TS + 12.f, (cy - 2) * TS + 12.f}, {(cx - 2) * TS + 12.f, (cy + 1) * TS + 12.f}, {(cx + 1) * TS + 12.f, (cy + 1) * TS + 12.f}}; };
    if (e.lord == 0) {  // THE SULTAN: compact walled square, three domed corner towers, gatehouse in the south-west, domed keep in the middle
        ringRect(defs, walls, cx - 5, cy - 4, cx + 4, cy + 4, {{cx - 5, cy - 4, RTOWER}, {cx + 4, cy - 4, RTOWER}, {cx + 4, cy + 4, RTOWER}, {cx - 5, cy + 4, TOWER}, {cx - 5, cy + 2, GATE}, {cx - 5, cy, TOWER}});
        e.gx = cx - 5; e.gy = cy + 2;
        A(first, WOOD, cx - 4, cy - 1); A(first, QUARRY, cx + 3, cy); A(first, HOUSE, cx - 3, cy + 3);
        A(rest, MILL, cx - 4, cy - 3); A(rest, BAKERY, cx + 2, cy - 3); A(rest, MARKET, cx + 2, cy + 2); A(rest, HOUSE, cx - 1, cy + 3); A(rest, TENT, cx + 1, cy + 3); A(rest, BARRACKS, cx + 3, cy + 3);
        guards4(); e.rad = 8;
    } else if (e.lord == 1) {  // THE WOLF: huge stone keep, outer wall + inner wall, many towers, barracks and an inn inside
        ringRect(defs, walls, cx - 8, cy - 7, cx + 7, cy + 6, {{cx - 8, cy - 7, RTOWER}, {cx + 7, cy - 7, RTOWER}, {cx - 8, cy + 6, RTOWER}, {cx + 7, cy + 6, RTOWER},
            {cx - 1, cy - 7, TOWER}, {cx - 1, cy + 6, TOWER}, {cx + 7, cy, TOWER}, {cx - 8, cy, GATE}, {cx - 8, cy - 2, TOWER}, {cx - 8, cy + 2, TOWER}});
        e.gx = cx - 8; e.gy = cy;
        vector<BP> idef, iwall; ringRect(idef, iwall, cx - 3, cy - 3, cx + 2, cy + 2, {{cx - 3, cy - 3, TOWER}, {cx + 2, cy - 3, TOWER}, {cx - 3, cy + 2, TOWER}, {cx + 2, cy + 2, TOWER}, {cx - 3, cy, GATE}});
        A(first, WOOD, cx - 6, cy - 4); A(first, WOOD, cx - 6, cy - 2); A(first, QUARRY, cx - 6, cy + 3); A(first, HOUSE, cx - 6, cy + 5); A(first, BARRACKS, cx - 6, cy - 6);
        for (auto& t : idef) rest.push_back(t); for (auto& t : iwall) rest.push_back(t);
        A(rest, TENT, cx - 4, cy - 6); A(rest, POLE, cx - 2, cy - 6); A(rest, INN, cx + 4, cy - 6); A(rest, MARKET, cx + 4, cy + 4); A(rest, HOUSE, cx + 5, cy + 2);
        A(rest, HOUSE, cx + 3, cy + 5); A(rest, SMITH, cx, cy + 5); A(rest, BARRACKS, cx - 4, cy + 5); A(rest, STORAGE, cx + 5, cy - 3);
        guards4(); e.rad = 10;
    } else if (e.lord == 2) {  // THE WAZIR: round castle, an outer ring and an inner ring, round towers all around, siege workshop inside
        ringCircle(defs, walls, (float)cx, (float)cy, 7.5f, cy, {0, 55, 125, 235, 305, 160, 200}, true);
        e.gx = 999; for (auto& t : defs) if (t.type == GATE && t.x < e.gx) { e.gx = t.x; e.gy = t.y; }
        vector<BP> idef, iwall; ringCircle(idef, iwall, (float)cx, (float)cy, 4.2f, cy, {45, 135, 225, 315}, true);
        A(first, WOOD, cx - 6, cy - 3); A(first, QUARRY, cx - 6, cy + 2); A(first, HOUSE, cx + 3, cy - 6); A(first, BARRACKS, cx + 3, cy - 5);
        for (auto& t : idef) rest.push_back(t); for (auto& t : iwall) rest.push_back(t);
        A(rest, SIEGE, cx - 4, cy - 6); A(rest, TENT, cx + 5, cy + 2); A(rest, HOUSE, cx + 3, cy + 4); A(rest, MARKET, cx - 2, cy + 5); A(rest, HOUSE, cx - 4, cy + 4);
        guards4(); e.rad = 10;
    } else {
        genGeneric(e, CAS[e.lord], first, defs, walls, rest); guards4(); e.rad = (int)(max(CAS[e.lord].a, CAS[e.lord].shape == 0 ? CAS[e.lord].b : 0.f)) + 3;
    }
    // walls grow outwards from the gate
    int gx = e.gx, gy = e.gy;
    sort(walls.begin(), walls.end(), [&](const BP& a, const BP& b) { return hypotf((float)(a.x - gx), (float)(a.y - gy)) < hypotf((float)(b.x - gx), (float)(b.y - gy)); });
    e.bpl.insert(e.bpl.end(), first.begin(), first.end()); e.bpl.insert(e.bpl.end(), defs.begin(), defs.end());
    e.bpl.insert(e.bpl.end(), walls.begin(), walls.end()); e.bpl.insert(e.bpl.end(), rest.begin(), rest.end());
    e.idx.assign(e.bpl.size(), -1);
    e.rally = {(max(2, e.gx - 4) + 0.5f) * TS, (e.gy + 0.5f) * TS};
}

// ---------------- the AI lords: build the castle, train an army, send it to attack ----------------
bool aiPlace(int ei, int type, int x, int y) {   // the AI lord clears trees out of the way; water, rocks and other buildings still block
    int s = sizeOf(type);
    for (int j = 0; j < s; j++) for (int i = 0; i < s; i++) { int xx = x + i, yy = y + j; if (xx < 0 || yy < 0 || xx >= MW || yy >= MH) return false; if (occ[yy][xx] != -1 && occ[yy][xx] != -2) return false; }
    for (int j = 0; j < s; j++) for (int i = 0; i < s; i++) if (occ[y + j][x + i] == -2) occ[y + j][x + i] = -1;
    if (!place(type, x, y, true, 0)) return false;
    B.back().lord = EN[ei].lord; B.back().own = ei; return true;
}
int ownCount(int ei, int type) { int n = 0; for (auto& b : B) if (b.own == ei && !b.dead && b.prog >= 1.f && b.type == type) n++; return n; }
int armyCount(int ei) { int n = 0; for (auto& u : U) if (u.own == ei && u.army && u.hp > 0) n++; return n; }
int totalEnemyArmy() { int n = 0; for (auto& u : U) if (u.enemy && u.army && u.hp > 0) n++; return n; }
const int ET[8] = {SLAVE, EARCHER, ESWORD, EHORSE, HORSE_ARCHER, CATA, KNIGHT, CROSSBOW}, ENC[8] = {6, 12, 18, 24, 26, 55, 42, 16}, UNLV[8] = {1, 2, 3, 3, 3, 4, 5, 4}, ONI[8] = {0, 1, 2, 3, 3, 4, 3, 1};
void trainEnemy(int ei) {
    Enemy& e = EN[ei]; const Lord& L = LORDS[e.lord];
    if (armyCount(ei) >= cur.armyMax) return;
    int siege = ownCount(ei, SIEGE), cataNow = 0; for (auto& u : U) if (u.own == ei && u.army && u.type == CATA) cataNow++;
    vector<int> pool;
    for (int k = 0; k < 8; k++) {
        if (L.wt[k] <= 0 || (curMission < 1 && !enemyOn[ONI[k]]) || e.wave + 1 < UNLV[k]) continue;
        if (ET[k] == CATA && (siege == 0 || cataNow >= L.cataBase + e.wave / L.cataDiv)) continue;
        for (int w = 0; w < L.wt[k]; w++) pool.push_back(k);
    }
    if (pool.empty()) pool.push_back(0);
    int k = pool[rand() % pool.size()]; int t = ET[k];
    if (e.gold < ENC[k]) return;
    vector<int> src; for (size_t i = 0; i < B.size(); i++) if (B[i].own == ei && !B[i].dead && B[i].prog >= 1.f && (t == CATA ? B[i].type == SIEGE : (B[i].type == BARRACKS || B[i].type == TENT))) src.push_back((int)i);
    if (src.empty()) return;
    e.gold -= ENC[k]; const Building& b = B[src[rand() % src.size()]]; int s = sizeOf(b.type);
    Vector2 sp = freeNear({(b.x + s / 2.0f) * TS, (b.y + s + 0.5f) * TS}); int n = e.trained++; bool army = (n % 4) != 3;
    Vector2 dst = army ? freeNear({e.rally.x + (n % 5 - 2) * 16.f, e.rally.y + ((n / 5) % 5 - 2) * 16.f}) : e.guards[rand() % e.guards.size()];
    U.push_back({sp, dst, t, true, false, false, UINFO[t].hp * cur.hpMul * L.hpMul, 0, (float)PI, 0, true}); U.back().own = ei; U.back().army = army;
}
void aiTick(int ei, float dt) {
    Enemy& e = EN[ei]; if (e.defeated) return;
    const Lord& L = LORDS[e.lord];
    const float econBoost = L.doctrine == 2 || L.doctrine == 7 ? 1.45f : L.doctrine == 3 ? 0.9f : 1.0f;
    e.wood += dt * (1.6f + 1.0f * ownCount(ei, WOOD)) * econBoost;
    e.stone += dt * (2.2f + 1.2f * ownCount(ei, QUARRY)) * (L.doctrine == 3 ? 1.35f : econBoost);
    e.gold += dt * (2.0f + 0.8f * (ownCount(ei, HOUSE) + ownCount(ei, MARKET))) * econBoost;
    for (size_t i = 0; i < e.bpl.size(); i++) if (e.idx[i] >= 0 && B[e.idx[i]].dead) { e.idx[i] = -1; e.done = false; }  // destroyed pieces are rebuilt
    e.buildT += dt;
    if (e.buildT >= L.buildT * cur.buildMul) {
        e.buildT = 0; int placed = 0;
        for (size_t i = 0; i < e.bpl.size() && placed < 3; i++) {
            if (e.idx[i] != -1) continue;
            const BP& p = e.bpl[i]; const BInfo& in = INFO[p.type]; int w = in.wood / 3, s = in.stone / 3;
            if (e.wood < w || e.stone < s) break;                         // wait for resources, keep the build order
            if (!aiPlace(ei, p.type, p.x, p.y)) { e.idx[i] = -2; continue; }   // blocked for good (water or one of our buildings)
            e.wood -= w; e.stone -= s; e.idx[i] = (int)B.size() - 1; placed++;
            if ((p.type == RTOWER || p.type == BARRACKS || p.type == SIEGE) && EN.size() == 1) say(string(L.name) + " is building a " + INFO[p.type].name + "!");
            if (p.type != WALL) break;
        }
        if (!e.done) { bool all = true; for (int v : e.idx) if (v == -1) { all = false; break; } if (all) { e.done = true; say(string(L.name) + "'s castle is complete!"); sfx(SFX_BELL, 0.6f, 0.5f); enemyMsg(e.lord, COMPLETE_LINES[rand() % 3], 2); } }
    }
    // train soldiers in his barracks / tents / siege workshop (paid with his own gold), they gather in front of the gate
    int mil = ownCount(ei, BARRACKS) + ownCount(ei, TENT);
    if (mil > 0) {
        e.trainT += dt;
        float trainScale = L.doctrine == 1 || L.doctrine == 5 ? 0.78f : L.doctrine == 2 || L.doctrine == 7 ? 1.22f : L.doctrine == 3 ? 1.12f : 1.0f;
        if (e.trainT >= cur.trainInt * trainScale / (1 + 0.4f * (mil - 1))) { e.trainT = 0; trainEnemy(ei); if (ownCount(ei, SIEGE) > 0 && rand() % 3 == 0) trainEnemy(ei); }
    }
    // Personality changes when the lord commits to an attack or waits for a stronger garrison.
    e.atkT += dt; int army = armyCount(ei); int need = min(cur.armyMax, (int)(cur.armyBase + e.wave * cur.armyGrow));
    float attackScale = L.doctrine == 1 || L.doctrine == 5 ? 0.78f : L.doctrine == 3 || L.doctrine == 7 ? 1.25f : 1.0f;
    float iv = (e.wave == 0 ? cur.firstAtk : cur.atkInt) * attackScale;
    if (e.atkT >= iv && (army >= need || (e.atkT >= iv * 2.5f && army >= 2))) {
        e.atkT = 0; e.wave++; wave++; int sent = 0;
        for (auto& u : U) if (u.own == ei && u.army && u.guard) { u.guard = false; sent++; }
        tauntLine = L.taunt[e.wave % 3];
        warnEnemy(string("WARNING: ") + L.name + " attacks with " + to_string(sent) + " soldiers!"); sfx(SFX_HORN, 0.9f, 1.f);
        enemyMsg(e.lord, L.taunt[e.wave % 3], 1);
    }
}

void clearWorkers(); void clearHunting(); void initAnimals();
void reset() {
    clearWorkers(); clearHunting(); clearMsgs(); victoryT = 0; victoryVoice = false; cheatInstant = cheatGod = cheatUsed = false; cheatCap = 0; taunT = 70; B.clear(); U.clear(); P.clear(); PT.clear(); EN.clear(); selectedBuilding = -1; warningT = 0;
    for (int i = 0; i < cur.n; i++) { Enemy e; e.lord = cur.en[i].lord; e.kx = cur.en[i].kx; e.ky = cur.en[i].ky; EN.push_back(e); }
    for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) {
        float n = sinf(x * (0.25f + mapStyle * 0.08f)) + cosf(y * (0.38f + mapStyle * 0.05f))
                + sinf((x + y) * (0.16f + mapStyle * 0.03f));
        float left = hypotf(x - 5.f, y - 17.f), right = 99;
        for (auto& e : EN) right = min(right, hypotf(x - (e.kx + 1.f), y - (e.ky + 1.f)));
        if (mapStyle == 1) n += sinf(y * 0.75f) * 0.55f;
        if (mapStyle == 2 || mapStyle == 6) n -= cosf(x * 0.7f) * 0.35f;
        bool greenMap = mapStyle == 4 || mapStyle == 5;
        ter[y][x] = greenMap ? (n > -0.2f ? 1 : 0) : ((n > 0.7f || left < 8 || right < 9.f) ? 1 : (n < -1.2f ? 2 : 0));
        occ[y][x] = -1;
        bool water = false;
        // Eight original layouts: oasis, lake, mountain valley, arid dunes,
        // green highlands, twin-castle river, broken ruins, and central fortress.
        if (mapStyle == 0) water = ((x - 30) * (x - 30) + (y - 6) * (y - 6) * 1.5f < 30 || (x - 24) * (x - 24) + (y - 28) * (y - 28) < 14);
        else if (mapStyle == 1) water = ((x - 27) * (x - 27) + (y - 17) * (y - 17) < 42);
        else if (mapStyle == 2) { water = ((x == 16 || x == 17 || x == 38 || x == 39) && y > 5 && y < 29); if ((y < 10 || y > 24) && x > 14 && x < 42) ter[y][x] = 2; }
        else if (mapStyle == 3) water = ((x - 29) * (x - 29) + (y - 17) * (y - 17) < 8);
        else if (mapStyle == 4) water = (x == 28 || x == 29) && y > 3 && y < 31;
        else if (mapStyle == 5) water = (x == 27 || x == 28) && y > 2 && y < 32 && !(y >= 14 && y <= 18);
        else if (mapStyle == 6) { water = ((x - 27) * (x - 27) + (y - 17) * (y - 17) < 10); if (((x + y * 3) % 13) < 3 && x > 12) ter[y][x] = 2; }
        else if (mapStyle == 7) water = ((x - 28) * (x - 28) + (y - 17) * (y - 17) < 5);
        if (water) { ter[y][x] = 3; occ[y][x] = -4; }
        else if (x > 10 && rand() % 100 < (ter[y][x] == 1 ? 15 : 5)) occ[y][x] = -2;
    }
    for (int y = 10; y < 25; y++) for (int x = 1; x < 13; x++) if (occ[y][x] == -2) occ[y][x] = -1;       // room for our castle
    for (size_t i = 0; i < EN.size(); i++) {
        Enemy& e = EN[i]; genBlueprint(e);
        int cx = e.kx + 1, cy = e.ky + 1;
        for (int y = max(0, cy - e.rad); y <= min(MH - 1, cy + e.rad); y++) for (int x = max(0, cx - e.rad); x <= min(MW - 1, cx + e.rad); x++) {   // room for his castle: no trees, no water
            if (occ[y][x] == -2 || occ[y][x] == -4) { occ[y][x] = -1; if (ter[y][x] == 3) ter[y][x] = 1; }
        }
    }
    place(KEEP, PKX, PKY, false, keepStyle);
    place(GRANARY, PKX + 3, PKY + 1, false); // starting food storage
    place(STORAGE, PKX + 3, PKY + 4, false); // starting resource depot for woodcutters
    for (size_t i = 0; i < EN.size(); i++) {   // each lord starts with only his keep; his AI builds the rest
        Enemy& e = EN[i]; place(KEEP, e.kx, e.ky, true, e.lord); e.ek = (int)B.size() - 1; B.back().lord = e.lord; B.back().own = (int)i;
        const Lord& L = LORDS[e.lord];
        auto addStartingGuard = [&](Vector2 target, int seed) {
            int k = seed % 3; while (k < 8 && L.wt[k] <= 0) k++; int t = ET[k < 8 ? k : 0];
            U.push_back({freeNear(target), target, t, true, false, false, UINFO[t].hp * cur.hpMul * L.hpMul, 0, (float)PI, 0, true}); U.back().own = (int)i;
        };
        for (int g = 0; g < 4; g++) addStartingGuard(e.guards[g], g);
        // Add exactly eight extra starting defenders in the final campaign mission.
        if (curMission == 10) {
            const int extraPerLord[3] = {3, 3, 2};
            for (int g = 0; g < extraPerLord[i]; g++) {
                Vector2 base = e.guards[g % 4];
                Vector2 extra = {base.x + ((g % 2) ? 18.f : -18.f), base.y + ((g % 3) ? 16.f : -16.f)};
                addStartingGuard(extra, g + 3);
            }
        }
    }
    initAnimals();
    wood = cur.wood0; stone = cur.stone0; iron = 0; gold = cur.gold0; bread = 80; apples = dates = wine = grain = flour = grapes = candles = meat = 0; bows = 2; spears = 2; swords = 1; pop = 8;
    if (curMission == 0) { bows = 10; spears = 6; swords = 4; }
    wave = 0; kills = 0; popularity = 100; tax = 1; ration = 2; speedMul = 1; pbonus = 0; innServed = churchServed = 0;
    ecoT = 0; bannerT = 0; gameOver = won = paused = false;
    enemyMsg(EN[0].lord, INTRO_LINES[rand() % 3], 4);
    if (EN.size() == 1) say(string("Defeat ") + LORDS[EN[0].lord].name + "! He is building his castle - strike early or raise your own army.");
    else say(string("Defeat all ") + to_string(EN.size()) + " enemy lords! Press V to look at their castles.");
}
// ---------------- movement & combat ----------------
int step(Unit& u, Vector2 to, float dt) {  // returns index of a building blocking an enemy, else -1
    float l = dist(u.p, to); if (l < 1) return -1;
    Vector2 d = {(to.x - u.p.x) / l, (to.y - u.p.y) / l}; u.face = atan2f(d.y, d.x);
    float speed = UINFO[u.type].speed;
    if (u.type == HORSE_ARCHER) speed *= (dist(u.p, u.dst) > 90.f ? 1.22f : 0.62f); // gallop on long orders, slow walk near the destination
    float sp = speed * dt; d = {d.x * sp, d.y * sp};
    auto tile = [&](float x, float y) { int tx = (int)(x / TS), ty = (int)(y / TS);
        return (x < 0 || y < 0 || tx >= MW || ty >= MH) ? -3 : occ[ty][tx]; };
    auto ok = [&](int o) { return o == -1 || (o >= 0 && B[o].type == GATE && B[o].enemy == u.enemy); };  // trees/water/buildings block movement
    int o = tile(u.p.x + d.x, u.p.y + d.y);
    if (ok(o)) { u.p.x += d.x; u.p.y += d.y; u.anim += dt * ((u.type == HORSE_ARCHER || u.type == CAMEL_ARCHER) ? 6.f : 9.f); if (rand() % 24 == 0) addPart(0, u.p.x, u.p.y, 1); return -1; }
    if (o >= 0 && u.enemy && !B[o].enemy) return o;
    if (ok(tile(u.p.x + d.x, u.p.y))) u.p.x += d.x; else if (ok(tile(u.p.x, u.p.y + d.y))) u.p.y += d.y;
    return -1;
}

vector<Vector2> findPath(Vector2 a, Vector2 b, bool enemy) {  // A* on the tile grid: units walk around trees, water and buildings
    auto cl = [](int v, int hi) { return max(0, min(hi - 1, v)); };
    int sx = cl((int)(a.x / TS), MW), sy = cl((int)(a.y / TS), MH), gx = cl((int)(b.x / TS), MW), gy = cl((int)(b.y / TS), MH);
    auto cost = [&](int x, int y) -> float {
        int o = occ[y][x]; if (o == -1) return 1.f; if (o == -2 || o == -4) return -1.f; if (o < 0) return -1.f;
        if (B[o].type == GATE && B[o].enemy == enemy) return 1.f;
        if (enemy && !B[o].enemy) return 12.f;  // raiders break through our walls when there is no gap
        return -1.f; };
    static float g[MH][MW]; static int par[MH][MW]; static bool done[MH][MW];
    for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) { g[y][x] = 1e9f; par[y][x] = -1; done[y][x] = false; }
    priority_queue<pair<float, int>, vector<pair<float, int>>, greater<pair<float, int>>> pq;
    g[sy][sx] = 0; pq.push({0.f, sy * MW + sx});
    while (!pq.empty()) {
        int idx = pq.top().second; pq.pop(); int x = idx % MW, y = idx / MW;
        if (done[y][x]) continue; done[y][x] = true; if (x == gx && y == gy) break;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            if (!dx && !dy) continue; int nx = x + dx, ny = y + dy; if (nx < 0 || ny < 0 || nx >= MW || ny >= MH) continue;
            bool goal = nx == gx && ny == gy; float c = cost(nx, ny); if (c < 0 && !goal) continue; if (c < 0) c = 1;
            if (dx && dy && (cost(x + dx, y) < 0 || cost(x, y + dy) < 0)) continue;  // no corner cutting
            float ng = g[y][x] + c * (dx && dy ? 1.414f : 1.f);
            if (ng < g[ny][nx]) { g[ny][nx] = ng; par[ny][nx] = y * MW + x; pq.push({ng + hypotf((float)(gx - nx), (float)(gy - ny)), ny * MW + nx}); }
        }
    }
    vector<Vector2> path; if (g[gy][gx] >= 1e9f) return path;
    for (int i = gy * MW + gx; i != sy * MW + sx && i >= 0; i = par[i / MW][i % MW]) path.push_back({(i % MW + 0.5f) * TS, (i / MW + 0.5f) * TS});
    reverse(path.begin(), path.end()); if (!path.empty()) path.back() = b;
    return path;
}

int goTo(Unit& u, Vector2 target, float dt) {  // walk to a target along an A* path; returns a blocking building index (raiders only)
    u.repT -= dt;
    if (dist(u.p, target) < 30) { u.path.clear(); u.goal = target; }
    else if (u.repT <= 0 && (dist(u.goal, target) > 14 || u.path.empty())) { u.path = findPath(u.p, target, u.enemy); u.goal = target; u.repT = 0.5f; }
    Vector2 to = target;
    if (!u.path.empty()) { if (dist(u.p, u.path[0]) < 5) u.path.erase(u.path.begin()); if (!u.path.empty()) to = u.path[0]; }
    Vector2 before = u.p; int blk = step(u, to, dt);
    u.stuck = dist(before, u.p) < UINFO[u.type].speed * dt * 0.3f ? u.stuck + dt : 0;
    if (u.stuck > 0.5f && !u.path.empty()) { u.path.clear(); u.repT = 0; u.stuck = 0; }
    return blk;
}

void attack(Unit& u, Vector2 tp, int ui, int bi) {
    const UInfo& k = UINFO[u.type]; u.cd = k.cd; u.face = atan2f(tp.y - u.p.y, tp.x - u.p.x);
    if (k.range > 30) {
        P.push_back({u.p, tp, 0.f, max(1.f, dist(u.p, tp)), k.dmg, u.enemy, (u.type == CATA || u.type == DEMOLISHER)});
        if (u.type == CATA || u.type == DEMOLISHER) sfx(SFX_CATAPULT, 0.7f, 0.25f); else sfx(SFX_ARROW, 0.22f, 0.09f);
    }
    else if (ui >= 0) { U[ui].hp -= k.dmg * ((u.type == RAM ? 0.2f : 1.f)); addPart(1, tp.x, tp.y, 8, 3); sfx(SFX_SWORD, 0.28f, 0.11f); }
    else if (bi >= 0) { B[bi].hp -= k.dmg; addPart(3, tp.x, tp.y, 10, 2); sfx(SFX_HIT, 0.4f, 0.12f); if (B[bi].hp <= 0) killB(bi); }
}

void land(const Proj& p) {
    addPart(0, p.b.x, p.b.y, 1, p.siege ? 8 : 2); if (p.siege) addPart(3, p.b.x, p.b.y, 8, 6);
    if (p.siege) {  // catapult stone: hits building + splash on soldiers of the other side
        sfx(SFX_BOOM, 0.55f, 0.2f);
        int tx = (int)(p.b.x / TS), ty = (int)(p.b.y / TS);
        if (tx >= 0 && ty >= 0 && tx < MW && ty < MH && occ[ty][tx] >= 0 && B[occ[ty][tx]].enemy != p.enemy) { B[occ[ty][tx]].hp -= p.dmg; if (B[occ[ty][tx]].hp <= 0) killB(occ[ty][tx]); }
        for (auto& u : U) if (u.enemy != p.enemy && dist(u.p, p.b) < 28) u.hp -= p.dmg / 2;
        return;
    }
    for (auto& u : U) if (u.enemy != p.enemy && dist(u.p, p.b) < 12) { u.hp -= p.dmg; return; }
    int tx = (int)(p.b.x / TS), ty = (int)(p.b.y / TS);
    if (tx >= 0 && ty >= 0 && tx < MW && ty < MH && occ[ty][tx] >= 0 && B[occ[ty][tx]].enemy != p.enemy) {
        B[occ[ty][tx]].hp -= p.dmg / 3; if (B[occ[ty][tx]].hp <= 0) killB(occ[ty][tx]);
    }
}

// ---------------- training ----------------
bool canTrainAt(int btype, int ti) {
    const int* r = btype == BARRACKS ? ROSTER_BARRACKS : btype == TENT ? ROSTER_TENT : btype == SIEGE ? ROSTER_SIEGE : btype == ENGGUILD ? ROSTER_ENG : nullptr;
    int n = btype == BARRACKS ? 8 : btype == TENT ? 8 : btype == SIEGE ? 3 : btype == ENGGUILD ? 1 : 0;
    for (int i = 0; i < n && r; i++) if (r[i] == ti) return true;
    return false;
}
int findTrainer(int ti) { for (size_t i = 0; i < B.size(); i++) if (!B[i].dead && !B[i].enemy && B[i].prog >= 1.f && canTrainAt(B[i].type, ti)) return (int)i; return -1; }
string trainBlockReason(int ti, int br) {
    if (br < 0 || br >= (int)B.size() || B[br].dead || !built(B[br])) return "The required training building is not ready.";
    const TInfo& t = TR[ti]; bool arabTent = B[br].type == TENT;
    int stock = t.res == 0 ? bows : t.res == 1 ? spears : t.res == 2 ? swords : -1;
    if (!arabTent && t.res >= 0 && stock < 1) return string("Not enough ") + (t.res == 0 ? "bows." : t.res == 1 ? "spears." : "swords.");
    if (gold < t.gold) return TextFormat("Need %d more gold.", t.gold - gold);
    if (wood < t.wood) return TextFormat("Need %d more wood.", t.wood - wood);
    if (pop - employed < 1) return "No idle peasant available. Build houses or free a worker.";
    return "";
}
bool trainOne(int ti, int br) {
    const TInfo& t = TR[ti]; int* stock = t.res == 0 ? &bows : t.res == 1 ? &spears : t.res == 2 ? &swords : nullptr;
    // Barracks consume weapon stock; the Arabic War Tent recruits with gold only.
    bool arabTent = B[br].type == TENT;
    if (!trainBlockReason(ti, br).empty()) return false;
    if (!arabTent && stock) *stock -= 1;
    gold -= t.gold; wood -= t.wood; pop--;
    int s = sizeOf(B[br].type); Vector2 sp = freeNear({(B[br].x + s / 2.0f) * TS, (B[br].y + s + 0.5f) * TS});
    sp.x += (rand() % 9 - 4); sp.y += (rand() % 9 - 4);
    U.push_back({sp, {0, 0}, t.unit, false, false, false, UINFO[t.unit].hp, 0, 0});
    return true;
}
void trainMany(int ti, int n, int br = -1) {
    if (br < 0) br = findTrainer(ti);
    if (br < 0) { say(string("You need a ") + (ti == 16 ? "Engineer Guild" : ti >= 17 ? "Siege Workshop" : ti >= 8 ? "War Tent" : "Barracks") + " first!"); sfx(SFX_DENY); return; }
    int done = 0;
    while ((n < 0 || done < n) && done < 200) { if (!trainOne(ti, br)) break; done++; }
    if (done) {
        sfx(SFX_TRAIN, 0.55f, 0.1f);
        if (n >= 0 && done < n) say(TextFormat("Trained %d %s. %s", done, TR[ti].name, trainBlockReason(ti, br).c_str()));
        else say(TextFormat("Trained %d %s", done, TR[ti].name));
    } else { sfx(SFX_DENY); say(trainBlockReason(ti, br)); }
}

// ---------------- market goods ----------------
const char* GN[NG] = {"Wood", "Stone", "Iron", "Bread", "Apples", "Dates", "Wine", "Bows", "Spears", "Swords"};
const int BUY[NG] = {4, 6, 8, 5, 4, 4, 10, 12, 8, 20}, SELL[NG] = {2, 3, 4, 2, 2, 2, 5, 6, 4, 10};
int* goodPtr(int i) { int* g[NG] = {&wood, &stone, &iron, &bread, &apples, &dates, &wine, &bows, &spears, &swords}; return g[i]; }


Vector2 iso(float wx, float wy); bool inView(Vector2 p, float mg);
// ---------------- workers (animated peasants at woodcutters and farms) ----------------
struct Worker { int bi; float x, y, tx, ty, t, anim; int st; bool right, moving; int cargo = -1; };   // st: 0 walk to work spot, 1 work, 2 carry back, 3 rest
vector<Worker> WK;
int workerKind(int bt) { switch (bt) { case WOOD: return 0; case WHEAT: return 1; case APPLE: return 2; case DATE: return 3; case GRAPE: return 4; } return -1; }
Vector2 doorPos(const Building& b) { float s = sizeOf(b.type) * TS; return {b.x * TS + s / 2, b.y * TS + s + 5}; }
int nearestGranary(Vector2 from) { int best = -1; float bd = 1e9f; for (size_t i = 0; i < B.size(); i++) if (!B[i].dead && !B[i].enemy && B[i].type == GRANARY && built(B[i])) { float d = dist(from, doorPos(B[i])); if (d < bd) { bd = d; best = (int)i; } } return best; }
int nearestStorage(Vector2 from) { int best = -1; float bd = 1e9f; for (size_t i = 0; i < B.size(); i++) if (!B[i].dead && !B[i].enemy && B[i].type == STORAGE && built(B[i])) { float d = dist(from, doorPos(B[i])); if (d < bd) { bd = d; best = (int)i; } } return best; }
Vector2 pickWorkSpot(const Building& b) {
    Vector2 c = center(b);
    if (b.type == WOOD) {   // the lumberjack walks to a tree nearby
        int tx0 = (int)(c.x / TS), ty0 = (int)(c.y / TS); float bs = 1e9f; Vector2 best = {-1, -1};
        for (int dy = -7; dy <= 7; dy++) for (int dx = -7; dx <= 7; dx++) {
            int x = tx0 + dx, y = ty0 + dy; if (x < 0 || y < 0 || x >= MW || y >= MH || occ[y][x] != -2) continue;
            float sc = hypotf((float)dx, (float)dy) + (rand() % 40) / 10.f; if (sc < bs) { bs = sc; best = {(x + 0.5f) * TS, (y + 0.5f) * TS}; }
        }
        if (best.x >= 0) { float dx = c.x - best.x, dy = c.y - best.y, l = max(1.f, hypotf(dx, dy)); return {best.x + dx / l * 13, best.y + dy / l * 13}; }
        return {c.x + (rand() % 61 - 30), c.y + 22 + rand() % 20};
    }
    float s = sizeOf(b.type) * TS; return {b.x * TS + 9 + rand() % (int)(s - 18), b.y * TS + 9 + rand() % (int)(s - 18)};
}
bool walkTo(Worker& w, float tx, float ty, float dt, float sp) {
    float dx = tx - w.x, dy = ty - w.y, l = hypotf(dx, dy);
    if (l < 2.f) { w.moving = false; return true; }
    float st = min(l, sp * dt); w.x += dx / l * st; w.y += dy / l * st; w.moving = true; if (fabsf(dx - dy) > 0.5f) w.right = (dx - dy) > 0; return false;
}
void clearWorkers() { WK.clear(); }
void updateWorkers(float dt) {
    WK.erase(remove_if(WK.begin(), WK.end(), [](const Worker& w) { return w.bi >= (int)B.size() || B[w.bi].dead || !B[w.bi].working; }), WK.end());
    for (size_t i = 0; i < B.size(); i++) {
        const Building& b = B[i]; if (workerKind(b.type) < 0 || b.dead || b.enemy || !b.working) continue;
        bool has = false; for (auto& w : WK) if (w.bi == (int)i) { has = true; break; }
        if (!has) { Vector2 d = doorPos(b); Worker w; w.bi = (int)i; w.x = w.tx = d.x; w.y = w.ty = d.y; w.t = (rand() % 50) / 100.f; w.anim = (float)(rand() % 100); w.st = 3; w.right = true; w.moving = false; w.cargo = -1; WK.push_back(w); }
    }
    for (auto& w : WK) {
        const Building& b = B[w.bi]; Vector2 d = doorPos(b); w.anim += dt;
        switch (w.st) {
            case 0: if (walkTo(w, w.tx, w.ty, dt, 30)) { w.st = 1; w.t = 0; } break;
            case 1: w.moving = false; w.t += dt;
                if (b.type == WOOD && fmodf(w.anim, 0.9f) < dt * 1.01f && inView(iso(w.x, w.y), 40)) { sfx(SFX_HIT, 0.12f, 0.45f); addPart(3, w.x, w.y, 8, 1); }
                if (w.t >= (b.type == WOOD ? 3.6f : 4.2f)) {
                    int kind = workerKind(b.type);
                    int gi = kind == 0 ? nearestStorage(d) : nearestGranary(d);
                    if (gi >= 0 && kind >= 0) { Vector2 gd = doorPos(B[gi]); w.tx = gd.x; w.ty = gd.y; w.cargo = kind; w.st = 2; }
                    else { w.st = 3; w.t = 0; }
                }
                break;
            case 2: if (walkTo(w, w.tx, w.ty, dt, 26)) {
                    int gi = w.cargo == 0 ? nearestStorage({w.x, w.y}) : nearestGranary({w.x, w.y});
                    if (gi >= 0 && dist({w.x, w.y}, doorPos(B[gi])) < 90) {
                        if (w.cargo == 0) wood += 3;
                        else if (w.cargo == 1) grain += 4; else if (w.cargo == 2) apples += 5;
                        else if (w.cargo == 3) dates += 4; else if (w.cargo == 4) grapes += 4;
                    }
                    w.cargo = -1; Vector2 sp = pickWorkSpot(b); w.tx = sp.x; w.ty = sp.y; w.st = 0; w.t = 0;
                } break;
            default: w.moving = false; w.t += dt; if (w.t > 0.9f && nearestGranary(d) >= 0) { Vector2 sp = pickWorkSpot(b); w.tx = sp.x; w.ty = sp.y; w.st = 0; }
        }
    }
}

// ---------------- healer ----------------
const float HEAL_RADIUS = 125.f, HEAL_PER_SEC = 7.f;   // world pixels (about 5 tiles) and hit points per second

// ---------------- hunting: wild animals, the Hunter's Hut and its hunter ----------------
struct Animal { float x, y, tx, ty, t, hp, anim, fleeT, age; int kind, state; bool right; };   // kind 0 deer, 1 rabbit, 2 boar; state 0 alive, 1 carcass, 2 gone
vector<Animal> AN; float animalSpawnT = 0;
const int ANIMAL_MEAT[3] = {12, 5, 16}; const float ANIMAL_HP[3] = {40, 15, 60};
struct Hunter { int bi; float x, y, t, anim; int st, target, carry; bool right, moving; };   // st: 0 idle, 1 approach, 2 aim, 3 fetch carcass, 4 pick up, 5 carry home, 6 butcher
vector<Hunter> HT;
bool tileFree(float x, float y) { int tx = (int)(x / TS), ty = (int)(y / TS); return tx >= 0 && ty >= 0 && tx < MW && ty < MH && occ[ty][tx] == -1; }
bool stepTo(float& x, float& y, float tx, float ty, float dt, float sp, bool& right, bool& moving) {
    float dx = tx - x, dy = ty - y, l = hypotf(dx, dy);
    if (l < 3.f) { moving = false; return true; }
    float st = min(l, sp * dt); x += dx / l * st; y += dy / l * st; moving = true; if (fabsf(dx - dy) > 0.5f) right = (dx - dy) > 0; return false;
}
void spawnAnimal() {
    for (int tries = 0; tries < 80; tries++) {
        int x = 10 + rand() % (MW - 10), y = 1 + rand() % (MH - 2);
        if (occ[y][x] != -1 || ter[y][x] == 3) continue;
        Vector2 pp = {(x + 0.5f) * TS, (y + 0.5f) * TS}; bool ok = dist(pp, center(B[0])) > 7 * TS;
        for (auto& e : EN) if (dist(pp, Vector2{(e.kx + 1.f) * TS, (e.ky + 1.f) * TS}) < (e.rad + 3) * TS) ok = false;
        if (!ok) continue;
        Animal a{}; a.x = a.tx = pp.x; a.y = a.ty = pp.y; int r = rand() % 100; a.kind = r < 50 ? 0 : (r < 80 ? 1 : 2); a.hp = ANIMAL_HP[a.kind]; a.t = (rand() % 30) / 10.f; a.state = 0; a.right = rand() % 2;
        for (auto& o : AN) if (o.state == 2) { o = a; return; }
        AN.push_back(a); return;
    }
}
void initAnimals() { AN.clear(); HT.clear(); animalSpawnT = 0; for (int i = 0; i < 9; i++) spawnAnimal(); }
void updateAnimals(float dt) {
    int alive = 0;
    for (auto& a : AN) {
        if (a.state == 2) continue;
        if (a.state == 1) { a.age += dt; if (a.age > 70) a.state = 2; continue; }
        alive++; a.anim += dt; a.t -= dt; a.fleeT -= dt;
        float tx = 0, ty = 0; bool scared = false;
        for (auto& h : HT) if (dist({a.x, a.y}, {h.x, h.y}) < 70) { scared = true; tx += a.x - h.x; ty += a.y - h.y; }
        for (auto& u : U) if (dist({a.x, a.y}, u.p) < 55) { scared = true; tx += a.x - u.p.x; ty += a.y - u.p.y; }
        float sp;
        if (scared || a.fleeT > 0) { if (scared) { float l = max(1.f, hypotf(tx, ty)); a.tx = a.x + tx / l * 60; a.ty = a.y + ty / l * 60; a.fleeT = max(a.fleeT, 1.2f); } sp = a.kind == 1 ? 60.f : 48.f; }
        else { if (a.t <= 0) { a.tx = a.x + (rand() % 101 - 50); a.ty = a.y + (rand() % 101 - 50); a.t = 2 + (rand() % 40) / 10.f; if (rand() % 3 == 0) { a.tx = a.x; a.ty = a.y; } } sp = a.kind == 1 ? 20.f : 15.f; }
        float dx = a.tx - a.x, dy = a.ty - a.y, l = hypotf(dx, dy);
        if (l > 2) { float st = min(l, sp * dt), nx = a.x + dx / l * st, ny = a.y + dy / l * st; if (tileFree(nx, ny)) { a.x = nx; a.y = ny; if (fabsf(dx - dy) > 0.5f) a.right = (dx - dy) > 0; } else { a.tx = a.x; a.ty = a.y; a.t = 0.5f; } }
    }
    animalSpawnT += dt; if (animalSpawnT > 16) { animalSpawnT = 0; if (alive < 9) spawnAnimal(); }
}
void updateHunters(float dt) {
    HT.erase(remove_if(HT.begin(), HT.end(), [](const Hunter& h) { return h.bi >= (int)B.size() || B[h.bi].dead || !B[h.bi].working; }), HT.end());
    for (size_t i = 0; i < B.size(); i++) {
        const Building& b = B[i]; if (b.type != HUNTER || b.dead || b.enemy || !b.working || b.prog < 1.f) continue;
        bool has = false; for (auto& h : HT) if (h.bi == (int)i) { has = true; break; }
        if (!has) { Vector2 d = doorPos(b); Hunter h{}; h.bi = (int)i; h.x = d.x; h.y = d.y; h.target = -1; h.carry = -1; h.right = true; HT.push_back(h); }
    }
    for (auto& h : HT) {
        const Building& b = B[h.bi]; Vector2 d = doorPos(b); h.anim += dt; h.moving = false;
        auto valid = [&](int st) { return h.target >= 0 && h.target < (int)AN.size() && AN[h.target].state == st; };
        switch (h.st) {
            case 0: h.t += dt; if (h.t > 1.2f) { h.t = 0; float best = 360; h.target = -1;
                    for (size_t i = 0; i < AN.size(); i++) if (AN[i].state == 0) { float dd = dist({AN[i].x, AN[i].y}, d); if (dd < best) { best = dd; h.target = (int)i; } }
                    if (h.target >= 0) h.st = 1; } break;
            case 1: { if (!valid(0)) { h.st = 0; h.target = -1; break; } Animal& a = AN[h.target]; float dd = dist({h.x, h.y}, {a.x, a.y});
                if (dd > 430) { h.st = 0; h.target = -1; break; } if (dd <= 95) { h.st = 2; h.t = 0; break; } stepTo(h.x, h.y, a.x, a.y, dt, 34, h.right, h.moving); break; }
            case 2: { if (!valid(0)) { h.st = valid(1) ? 3 : 0; if (h.st == 0) h.target = -1; break; } Animal& a = AN[h.target]; float dd = dist({h.x, h.y}, {a.x, a.y});
                h.right = ((a.x - a.y) - (h.x - h.y)) > 0; if (dd > 120) { h.st = 1; break; } h.t += dt;
                if (h.t >= 0.9f) { h.t = 0; P.push_back({{h.x, h.y}, {a.x, a.y}, 0.f, max(1.f, dd), 0.f, false, false}); sfx(SFX_ARROW, 0.25f, 0.1f); a.hp -= 22;
                    if (a.hp <= 0) { a.state = 1; a.age = 0; h.st = 3; } else { a.fleeT = 2.5f; h.st = 1; } } break; }
            case 3: { if (!valid(1)) { h.st = 0; h.target = -1; break; } Animal& a = AN[h.target]; if (stepTo(h.x, h.y, a.x, a.y, dt, 32, h.right, h.moving)) { h.st = 4; h.t = 0; } break; }
            case 4: { if (!valid(1)) { h.st = 0; h.target = -1; break; } h.t += dt; if (h.t > 0.7f) { h.carry = AN[h.target].kind; AN[h.target].state = 2; h.st = 5; } break; }
            case 5: if (stepTo(h.x, h.y, d.x, d.y, dt, 28, h.right, h.moving)) { h.st = 6; h.t = 0; } break;
            default: h.t += dt; if (fmodf(h.anim, 0.45f) < dt * 1.01f && inView(iso(h.x, h.y), 40)) sfx(SFX_HAMMER, 0.14f, 0.4f);
                if (h.t >= 1.8f) { meat += ANIMAL_MEAT[max(0, h.carry)]; h.carry = -1; sfx(SFX_COIN, 0.5f, 0.1f); addPart(1, h.x, h.y, 12, 5); h.st = 0; h.t = 0; h.target = -1; }
        }
    }
}
void clearHunting() { AN.clear(); HT.clear(); }

// ---------------- simulation ----------------
bool battleNear = false;
void update(float dt) {
    cap = 8; int jobs = pop;
    for (auto& b : B) if (!b.dead && !b.enemy && b.prog >= 1.f) { if (b.type == HOUSE) cap += 8; }
    if (cfg.cheats) cap += cheatCap;
    bannerT -= dt; gtime += dt;
    // automatic market trading tick (every 1s) for buy & sell thresholds (10, 20, 50, 100, All)
    autoTradeT += dt;
    if (autoTrade && autoTradeT >= 1.0f) {
        autoTradeT = 0.f;
        for (int i = 0; i < NG; i++) {
            int& st = *goodPtr(i);
            if (autoSellOn[i] && st > 0) {
                if (autoSellUpTo) {
                    int lim = autoSellLim[i] > 0 ? autoSellLim[i] : st;
                    int sellAmt = min(st, lim);
                    if (sellAmt > 0) { st -= sellAmt; gold += sellAmt * SELL[i]; }
                } else if (st > autoSellLim[i]) {
                    int excess = st - autoSellLim[i];
                    st -= excess; gold += excess * SELL[i];
                }
            }
            if (autoBuyOn[i] && autoBuyLim[i] > 0 && st < autoBuyLim[i] && gold >= BUY[i]) {
                int need = autoBuyLim[i] - st;
                int canBuy = min(need, gold / BUY[i]);
                if (canBuy > 0) { gold -= canBuy * BUY[i]; st += canBuy; }
            }
        }
    }
    // economy tick (every 5s): food, taxes, popularity, immigration, inns and churches
    ecoT += dt;
    if (ecoT >= 5) {
        ecoT = 0;
        static const int RP[4] = {-20, -5, 5, 15}; static const float RM[4] = {0, 0.5f, 1, 2};
        bool granaryReady = hasGranary();
        const int foodRequired = granaryReady ? (int)ceilf(pop * RM[ration] / 4.f) : (ration > 0 ? max(1, (int)ceilf(pop * RM[ration] / 4.f)) : 0);
        int need = foodRequired;
        auto eat = [&](int& stock) { int consumed = min(stock, need); stock -= consumed; need -= consumed; };
        eat(meat); eat(bread); eat(apples); eat(dates);
        // Any unmet ration demand causes hunger, not just a completely empty food stockpile.
        bool starving = ration > 0 && need > 0;
        gold += (int)(pop * tax * 0.5f); if (gold < 0) gold = 0;
        if (autoTrade) {
            if (bows < 4 && wood >= 2 && !autoSellOn[0]) { wood -= 2; bows++; }
            if (spears < 4 && wood >= 2 && !autoSellOn[0]) { wood -= 2; spears++; }
            if (swords < 2 && iron >= 2 && !autoSellOn[2]) { iron -= 2; swords++; }
        }
        innServed = churchServed = 0;   // the inn drinks wine and the church burns candles - without them they give no happiness
        for (auto& b : B) if (!b.dead && !b.enemy && b.prog >= 1.f) {
            if (b.type == INN) { if (wine >= 1) { wine--; b.fed = true; innServed++; } else b.fed = false; }
            if (b.type == CHURCH) { if (candles >= 1) { candles--; b.fed = true; churchServed++; } else b.fed = false; }
        }
        pbonus = min(25, innServed * 6 + churchServed * 7);
        int kinds = (bread > 0) + (apples > 0) + (dates > 0) + (meat > 0);
        int gardenBonus = min(15, gardenCount() * 3);
        int targetPopularity = max(0, min(100, 65 + RP[ration] - 6 * tax + pbonus + gardenBonus + (kinds > 1 ? (kinds - 1) * 3 : 0)));
        if (starving) popularity = max(0, popularity - 4); // slow hunger penalty, every 5 seconds
        else if (popularity < targetPopularity) popularity = min(targetPopularity, popularity + 2);
        else if (popularity > targetPopularity) popularity = max(targetPopularity, popularity - 1);
        if (churchServed > 0 && rand() % 4 == 0) sfx(SFX_BELL, 0.35f, 1.f);
        if (popularity >= 50 && pop < cap) pop += popularity >= 75 ? 2 : 1;
        else if ((popularity < 30 && pop > 0) || pop > cap) { pop--; say("Peasants are leaving your keep!"); }
        if (starving) say("Your people are starving!");
    }
    // construction sites grow from the foundation to the finished building
    for (auto& b : B) if (!b.dead && b.prog < 1.f) {
        b.prog = min(1.f, b.prog + dt * (b.enemy ? 1.6f : 1.f) / max(0.5f, BUILD_TIME[b.type])); Vector2 c = center(b); bool vis = inView(iso(c.x, c.y), 60);
        if (rand() % 35 == 0) addPart(0, c.x + (rand() % 25 - 12), c.y + (rand() % 25 - 12), 2, 2);
        if (vis && rand() % 50 == 0) sfx(SFX_HAMMER, 0.22f, 0.25f);
        if (b.prog >= 1.f) { addPart(0, c.x, c.y, 4, 8); if (!b.enemy) sfx(SFX_BUILT, 0.5f, 0.2f); else if (vis) sfx(SFX_BUILT, 0.2f, 0.3f); }
    }
    if (cfg.cheats && cheatGod) { for (auto& b : B) if (!b.dead && !b.enemy) b.hp = (float)INFO[b.type].hp; for (auto& u : U) if (!u.enemy) u.hp = UINFO[u.type].hp; }
    // buildings
    for (auto& b : B) {
        if (b.dead || (b.enemy && b.type != TOWER && b.type != RTOWER)) continue;
        if (b.prog < 1.f) continue;   // still under construction
        if ((b.type == HOUSE || b.type == INN || b.type == SMITH || b.type == BAKERY || b.type == WINERY) && rand() % 90 == 0) addPart(2, center(b).x, center(b).y, 26);
        if (WORKS[b.type]) {
            if (jobs <= 0) { b.working = false; continue; }
            jobs--;
            bool ok = true;
            if ((workerKind(b.type) >= 0 || b.type == BAKERY || b.type == HUNTER) && !hasGranary()) ok = false;
            switch (b.type) {
                case MILL: ok = ok && grain >= 3; break; case BAKERY: ok = ok && flour >= 3; break; case WINERY: ok = ok && grapes >= 3; break; case CANDLE: ok = ok && wood >= 2; break;
                case FLETCHER: case POLE: ok = ok && wood >= 2; break; case SMITH: ok = ok && iron >= 2; break; default: break; }
            b.working = ok; if (!ok) continue;
            b.t += dt;
            if (b.t >= 3) { b.t = 0;
                switch (b.type) {
                    case QUARRY: stone += 2; break; case IRON: iron += 2; break;
                    case MILL: grain -= 3; flour += 3; break; case BAKERY: flour -= 3; bread += 4; break;
                    case WINERY: grapes -= 3; wine += 3; break; case CANDLE: wood -= 2; candles += 3; break;
                    case FLETCHER: wood -= 2; bows++; break; case POLE: wood -= 2; spears++; break; case SMITH: iron -= 2; swords++; break; }
            }
        } else if (b.type == HEALER) {   // heals every friendly soldier inside the green circle
            b.fire -= dt; Vector2 c = center(b);
            if (b.fire <= 0) {
                b.fire = 0.5f; int healed = 0;
                for (auto& u : U) if (!u.enemy && u.hp < UINFO[u.type].hp && dist(c, u.p) < HEAL_RADIUS) { u.hp = min((float)UINFO[u.type].hp, u.hp + HEAL_PER_SEC * 0.5f); healed++; if (PT.size() < 450) PT.push_back({u.p.x + (rand() % 9 - 4), u.p.y, 14.f, 0, 0, 22.f, 1.0f, 1.0f, 4}); }
                if (healed > 0 && inView(iso(c.x, c.y), 80)) sfx(SFX_HEAL, 0.25f, 1.5f);
            }
        } else if (b.type == TOWER || b.type == RTOWER) {
            b.fire -= dt; Vector2 c = center(b);
            if (b.fire <= 0) {
                int t = -1; float best = b.type == RTOWER ? 190 : 170;
                for (size_t i = 0; i < U.size(); i++) if (U[i].enemy != b.enemy && dist(c, U[i].p) < best) { best = dist(c, U[i].p); t = (int)i; }
                if (t >= 0) { P.push_back({c, U[t].p, 0.f, max(1.f, best), (b.type == RTOWER ? 18.f : 14.f), b.enemy, false}); b.fire = 1.1f; sfx(SFX_ARROW, 0.2f, 0.09f); }
            }
        }
    }
    employed = pop - jobs;
    updateWorkers(dt);
    updateHunters(dt); updateAnimals(dt);
    // units
    battleNear = false; Vector2 myKeep = center(B[0]);
    for (size_t i = 0; i < U.size(); i++) {
        Unit& u = U[i]; const UInfo& k = UINFO[u.type]; u.cd -= dt; if ((u.type == HORSE_ARCHER || u.type == CAMEL_ARCHER) && u.cd > 0.f) u.anim += dt * 5.f;
        if (u.enemy && dist(u.p, myKeep) < 380) battleNear = true;
        if (!u.enemy) {
            bool eng = u.type == ENGINEER, ram = (u.type == RAM || u.type == DEMOLISHER);
            int t = -1; float best = max(k.range, 80.f);
            if (!eng && !ram) for (size_t j = 0; j < U.size(); j++) if (U[j].enemy && dist(u.p, U[j].p) < best) { best = dist(u.p, U[j].p); t = (int)j; }
            if (t >= 0) {
                if (best <= k.range) { if (u.cd <= 0) attack(u, U[t].p, t, -1); }
                else if (!u.moving) goTo(u, U[t].p, dt);
            } else if (!u.moving) {
                if (eng) {  // engineers repair damaged buildings of ours
                    int bi = -1; float bd = 260;
                    for (size_t j = 0; j < B.size(); j++) if (!B[j].dead && !B[j].enemy && B[j].hp < INFO[B[j].type].hp - 0.5f) { float d = dist(u.p, center(B[j])); if (d < bd) { bd = d; bi = (int)j; } }
                    if (bi >= 0) {
                        if (bd <= sizeOf(B[bi].type) * 12 + 14) { B[bi].hp = min((float)INFO[B[bi].type].hp, B[bi].hp + 16 * dt); u.anim += dt * 6; u.face = atan2f(center(B[bi]).y - u.p.y, center(B[bi]).x - u.p.x); if (rand() % 25 == 0) addPart(1, u.p.x, u.p.y, 10, 2); sfx(SFX_REPAIR, 0.3f, 0.45f); }
                        else goTo(u, center(B[bi]), dt);
                    }
                } else {    // idle: attack nearby enemy buildings
                    int bi = -1; float bd = max(110.f, k.range + 30);
                    for (size_t j = 0; j < B.size(); j++) if (!B[j].dead && B[j].enemy) { float d = dist(u.p, center(B[j])); if (d < bd) { bd = d; bi = (int)j; } }
                    if (bi >= 0) { if (bd <= k.range + sizeOf(B[bi].type) * 12 + 4) { if (u.cd <= 0) attack(u, center(B[bi]), -1, bi); } else goTo(u, center(B[bi]), dt); }
                }
            }
            if (u.moving) { goTo(u, u.dst, dt); if (dist(u.p, u.dst) < 4 || (u.path.empty() && u.stuck > 0.5f)) u.moving = false; }
        } else {
            bool siege = u.type == CATA; float best = 1e9f; Vector2 tp = {0, 0}; int tu = -1, tb = -1;
            if (!siege) for (size_t j = 0; j < U.size(); j++) if (!U[j].enemy) { float d = dist(u.p, U[j].p); if (d < best) { best = d; tp = U[j].p; tu = (int)j; } }
            for (size_t j = 0; j < B.size(); j++) if (!B[j].dead && !B[j].enemy) { float d = dist(u.p, center(B[j])); if (d < best) { best = d; tp = center(B[j]); tb = (int)j; tu = -1; } }
            if (best >= 1e9f) continue;
            if (u.guard && best > 150) { if (dist(u.p, u.dst) > 14) goTo(u, u.dst, dt); continue; }   // defenders and gathering armies wait at their post
            float reach = k.range + (tb >= 0 ? sizeOf(B[tb].type) * 12 : 0);
            if (best <= reach) { if (u.cd <= 0) attack(u, tp, tu, tb); }
            else { int blk = goTo(u, tp, dt); if (blk >= 0 && u.cd <= 0) attack(u, center(B[blk]), -1, blk); }
        }
    }
    for (auto& u : U) if (u.hp <= 0) {
        if (u.type == HORSE_ARCHER) horseDeathFX.push_back({u.p, 0.f, u.face});
        if (u.enemy) { kills++; gold += 2; }
    }
    for (auto& fx : horseDeathFX) fx.age += dt;
    horseDeathFX.erase(remove_if(horseDeathFX.begin(), horseDeathFX.end(), [](const HorseDeathFX& fx) { return fx.age >= 0.72f; }), horseDeathFX.end());
    U.erase(remove_if(U.begin(), U.end(), [](const Unit& u) { return u.hp <= 0; }), U.end());
    for (size_t i = 0; i < U.size(); i++) for (size_t j = i + 1; j < U.size(); j++) {  // soldiers don't stack on top of each other
        Vector2 d = {U[j].p.x - U[i].p.x, U[j].p.y - U[i].p.y}; float l = hypotf(d.x, d.y);
        if (l < 9 && l > 0.01f) {
            float push = (9 - l) * 0.25f; Vector2 n = {d.x / l * push, d.y / l * push}, pi = {U[i].p.x - n.x, U[i].p.y - n.y}, pj = {U[j].p.x + n.x, U[j].p.y + n.y};
            auto okp = [&](Vector2 p) { int tx = (int)(p.x / TS), ty = (int)(p.y / TS); return tx >= 0 && ty >= 0 && tx < MW && ty < MH && occ[ty][tx] == -1; };
            if (okp(pi)) U[i].p = pi; if (okp(pj)) U[j].p = pj;
        }
    }
    for (auto& q : PT) { q.x += q.vx * dt; q.y += q.vy * dt; q.z += q.vz * dt; if (q.kind == 1 || q.kind == 3) q.vz -= 160 * dt; q.life -= dt; if (q.z < 0) { q.z = 0; q.vz = 0; q.vx *= 0.5f; q.vy *= 0.5f; } }
    PT.erase(remove_if(PT.begin(), PT.end(), [](const Part& q) { return q.life <= 0; }), PT.end());
    // projectiles
    for (auto& p : P) p.t += dt * (p.siege ? 140.f : 240.f) / p.len;
    for (auto& p : P) if (p.t >= 1) land(p);
    P.erase(remove_if(P.begin(), P.end(), [](const Proj& p) { return p.t >= 1; }), P.end());
    taunT -= dt;   // now and then an enemy lord sends a taunting message
    if (taunT <= 0) { taunT = 100 + rand() % 80; vector<int> alive; for (size_t i = 0; i < EN.size(); i++) if (!EN[i].defeated) alive.push_back((int)i); if (!alive.empty()) { int ei = alive[rand() % alive.size()]; enemyMsg(EN[ei].lord, LORDS[EN[ei].lord].taunt[rand() % 3], 0); } }
    // the AI lords build their castles, train armies and send them to attack
    for (size_t i = 0; i < EN.size(); i++) aiTick((int)i, dt);
    warningT -= dt;
}
Color terBase(int t) { switch (t) { case 1: return {96, 142, 62, 255}; case 2: return {178, 148, 100, 255}; case 3: return {52, 125, 170, 255}; } return {222, 192, 128, 255}; }

// ---------------- isometric graphics (all drawn with code, no image files) ----------------
Vector2 iso(float wx, float wy) { return {wx - wy, (wx + wy) * 0.5f}; }
Vector2 isoH(float wx, float wy, float h) { Vector2 p = iso(wx, wy); p.y -= h; return p; }
Color shade(Color c, float f) { return {(unsigned char)min(255.f, c.r * f), (unsigned char)min(255.f, c.g * f), (unsigned char)min(255.f, c.b * f), c.a}; }
float hsh(int x, int y) { unsigned h = x * 374761393u + y * 668265263u; h = (h ^ (h >> 13)) * 1274126177u; return (h & 1023) / 1023.f; }
void tri(Vector2 a, Vector2 b, Vector2 c, Color col) { DrawTriangle(a, b, c, col); DrawTriangle(a, c, b, col); }
void quad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color col) { tri(a, b, c, col); tri(a, c, d, col); }
Vector2 lerpV(Vector2 a, Vector2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }

void box(float x0, float y0, float x1, float y1, float z0, float z1, Color c) {  // 3D block, lit from the top-left
    Vector2 D0 = isoH(x0, y1, z0), C0 = isoH(x1, y1, z0), B0 = isoH(x1, y0, z0);
    Vector2 A1 = isoH(x0, y0, z1), B1 = isoH(x1, y0, z1), C1 = isoH(x1, y1, z1), D1 = isoH(x0, y1, z1);
    quad(D0, C0, C1, D1, shade(c, 0.85f)); quad(C0, B0, B1, C1, shade(c, 0.6f)); quad(A1, B1, C1, D1, shade(c, 1.12f));
    quad(D0, C0, lerpV(C0, C1, 0.12f), lerpV(D0, D1, 0.12f), Color{0, 0, 0, 40}); quad(C0, B0, lerpV(B0, B1, 0.12f), lerpV(C0, C1, 0.12f), Color{0, 0, 0, 40});
    Color ln = {0, 0, 0, 70};
    if (z1 - z0 > 9) for (int i = 1; i < (int)((z1 - z0) / 7); i++) { float t = i * 7 / (z1 - z0); DrawLineV(lerpV(D0, D1, t), lerpV(C0, C1, t), ln); DrawLineV(lerpV(C0, C1, t), lerpV(B0, B1, t), ln); }
    DrawLineV(D0, C0, ln); DrawLineV(C0, B0, ln); DrawLineV(C0, C1, ln); DrawLineV(D0, D1, ln); DrawLineV(B0, B1, ln); DrawLineV(D1, C1, ln); DrawLineV(C1, B1, ln);
}
void roof(float x0, float y0, float x1, float y1, float z, float h, Color c) {
    Vector2 apex = isoH((x0 + x1) / 2, (y0 + y1) / 2, z + h), D = isoH(x0, y1, z), C = isoH(x1, y1, z), B = isoH(x1, y0, z), A = isoH(x0, y0, z);
    tri(A, B, apex, shade(c, 0.95f)); tri(A, D, apex, shade(c, 1.1f)); tri(D, C, apex, shade(c, 0.85f)); tri(C, B, apex, shade(c, 0.62f));
    DrawLineV(D, apex, {0, 0, 0, 80}); DrawLineV(C, apex, {0, 0, 0, 80}); DrawLineV(B, apex, {0, 0, 0, 80});
    for (int i = 1; i < 4; i++) { float f = i / 4.f; DrawLineV(lerpV(D, apex, f), lerpV(C, apex, f), {0, 0, 0, 40}); DrawLineV(lerpV(C, apex, f), lerpV(B, apex, f), {0, 0, 0, 40}); }
}
void merlons(float x0, float y0, float x1, float y1, float z, Color c) {  // castle battlements
    for (float t = x0; t < x1 - 3; t += 8) box(t, y0, t + 4, y0 + 4, z, z + 5, c);
    for (float t = y0 + 8; t < y1 - 8; t += 8) box(x0, t, x0 + 4, t + 4, z, z + 5, c);
    for (float t = y0 + 8; t < y1 - 8; t += 8) box(x1 - 4, t, x1, t + 4, z, z + 5, c);
    for (float t = x0; t < x1 - 3; t += 8) box(t, y1 - 4, t + 4, y1, z, z + 5, c);
}
void door(float xa, float xb, float y, float h, Color c) { quad(isoH(xa, y, 0), isoH(xb, y, 0), isoH(xb, y, h), isoH(xa, y, h), c); }
void win(float xa, float xb, float y, float z0, float z1) {
    quad(isoH(xa, y, z0), isoH(xb, y, z0), isoH(xb, y, z1), isoH(xa, y, z1), {50, 70, 110, 255}); DrawLineV(isoH((xa + xb) / 2, y, z0), isoH((xa + xb) / 2, y, z1), {230, 220, 190, 255});
}
void flag(float x, float y, float z, Color c) {
    Vector2 a = isoH(x, y, z + 20), b = isoH(x, y, z + 9), p = isoH(x, y, z + 14); p.x += 11; p.y += sinf(gtime * 5 + x) * 2;
    DrawLineEx(isoH(x, y, z), a, 2, DARKBROWN); tri(a, b, p, c);
}

void cyl(float cx, float cy, float r, float z0, float z1, Color c) {  // round tower
    const int N = 14;
    for (int pass = 0; pass < 2; pass++) for (int i = 0; i < N; i++) {
        float a0 = i * 6.2832f / N, a1 = (i + 1) * 6.2832f / N, am = (a0 + a1) / 2;
        if ((cosf(am) + sinf(am) > 0) != (pass == 1)) continue;
        Vector2 b0 = isoH(cx + cosf(a0) * r, cy + sinf(a0) * r, z0), b1 = isoH(cx + cosf(a1) * r, cy + sinf(a1) * r, z0);
        Vector2 t0 = isoH(cx + cosf(a0) * r, cy + sinf(a0) * r, z1), t1 = isoH(cx + cosf(a1) * r, cy + sinf(a1) * r, z1);
        float lx = (cosf(am) - sinf(am)) * 0.707f; quad(b0, b1, t1, t0, shade(c, 0.85f - 0.3f * lx));
        if (pass == 1) for (float z = z0 + 7; z < z1; z += 7) { float f = (z - z0) / (z1 - z0); DrawLineV(lerpV(b0, t0, f), lerpV(b1, t1, f), {0, 0, 0, 55}); }
    }
    Vector2 tc = isoH(cx, cy, z1); DrawEllipse((int)tc.x, (int)tc.y, r * 1.414f, r * 0.707f, shade(c, 1.15f));
}
void ringMerlons(float cx, float cy, float r, float z, Color c) {
    for (int pass = 0; pass < 2; pass++) for (int k = 0; k < 8; k++) {
        float a = k * 0.785f; if ((cosf(a) + sinf(a) > 0) != (pass == 1)) continue;
        float x = cx + cosf(a) * (r - 2), y = cy + sinf(a) * (r - 2); box(x - 2, y - 2, x + 2, y + 2, z, z + 5, c);
    }
}

Rectangle viewR = {-100000, -100000, 200000, 200000};   // visible part of the world (set every frame, used to skip off-screen drawing)
bool inView(Vector2 p, float mg) { return p.x > viewR.x - mg && p.x < viewR.x + viewR.width + mg && p.y > viewR.y - mg - 80 && p.y < viewR.y + viewR.height + mg + 40; }

void dome(float cx, float cy, float z, float r, Color c) {   // onion/half-sphere dome made of stacked ellipses
    for (int k = 0; k < 9; k++) {
        float f = k / 9.f, rad = r * sqrtf(1 - f * f); Vector2 p = isoH(cx, cy, z + r * 0.95f * f);
        DrawEllipse((int)p.x, (int)p.y, rad * 1.414f, rad * 0.707f, shade(c, 0.78f + 0.32f * f));
    }
    Vector2 a = isoH(cx, cy, z + r * 0.95f), b = isoH(cx, cy, z + r * 0.95f + 6); DrawLineEx(a, b, 1.6f, GOLD); DrawCircleV(b, 1.8f, GOLD);
}

// is this building drawn with a PNG sprite? (id/width/offset are returned)
bool sprFor(const Building& b, int& id, float& w, float& dy) {
    if (b.type == KEEP) { bool want = b.enemy ? (b.lord >= 0 && (LORDS[b.lord].keepKind == 1 || LORDS[b.lord].keepKind == 5)) : b.style == 0; if (!SP[S_CASTLE].ok || !want) return false; id = S_CASTLE; w = 128; dy = 30; return true; }
    return sprDef(b.type, id, w, dy) && SP[id].ok;
}
float pickH(const Building& b) {  // clickable height above the footprint (sprites are taller than the code-drawn buildings)
    int id; float w, dy; if (!sprFor(b, id, w, dy)) return 0.f;
    return w * SP[id].t.height / SP[id].t.width - dy + 8;
}

void drawBuildingFinal(const Building& b) {
    float s = sizeOf(b.type) * TS, x0 = b.x * TS, y0 = b.y * TS, x1 = x0 + s, y1 = y0 + s, xc = (x0 + x1) / 2, yc = (y0 + y1) / 2;
    const Lord& L = LORDS[b.enemy && b.lord >= 0 ? b.lord : 0];
    Color st = b.enemy ? L.stoneCol : Color{205, 165, 105, 255}, red = b.enemy ? Color{200, 30, 30, 255} : Color{40, 80, 210, 255}; float top = 20; bool spr = false;
    {   // sprite versions (PNG assets)
        int id = -1; float w = 0, dy = 0; Color tint = b.enemy ? L.tint : WHITE;
        if (sprFor(b, id, w, dy)) {
            Vector2 g = iso(xc, yc);
            if (b.type == KEEP) quad(iso(x0, y0), iso(x1, y0), iso(x1, y1), iso(x0, y1), Color{red.r, red.g, red.b, 70});  // team-coloured ground plate
            drawSprite(SP[id].t, g.x, g.y + dy, w, 0, false, tint);
            top = w * SP[id].t.height / SP[id].t.width - dy + 8; spr = true;
        }
    }
    auto house = [&](Color wall, Color rf, float h, float rh) {   // generic fallback building
        box(x0 + 3, y0 + 3, x1 - 3, y1 - 3, 0, h, wall); roof(x0 + 2, y0 + 2, x1 - 2, y1 - 2, h, rh, rf); door(xc - 3, xc + 3, y1 - 3, 8, {70, 45, 25, 255}); top = h + rh + 6; };
    if (!spr) switch (b.type) {
    case KEEP: {
        static const int PK[3] = {5, 3, 4}; int ks = b.enemy ? L.keepKind : PK[b.style % 3];   // 0 domed teal, 1/3 round towers, 2 domed gold, 4 desert fortress, 5 grand
        if (ks == 0 || ks == 2) {  // domed palace
            Color teal = ks == 0 ? Color{60, 160, 150, 255} : Color{220, 175, 60, 255};
            box(x0 + 3, y0 + 3, x1 - 3, y1 - 3, 0, 30, st); merlons(x0 + 3, y0 + 3, x1 - 3, y1 - 3, 30, st);
            door(x0 + 18, x0 + 30, y1 - 3, 16, {60, 40, 20, 255});
            for (int i = 0; i < 4; i++) { float cx = (i & 1) ? x1 - 7 : x0 + 7, cy = (i & 2) ? y1 - 7 : y0 + 7; cyl(cx, cy, 6, 0, 42, shade(st, 1.05f)); dome(cx, cy, 42, 6.5f, teal); }
            box(xc - 9, yc - 9, xc + 9, yc + 9, 30, 44, shade(st, 1.08f)); dome(xc, yc, 44, 11, teal); flag(xc, yc, 64, red); top = 96;
        } else if (ks == 1 || ks == 3) {  // round-tower castle
            float cx[4] = {x0 + 9, x1 - 9, x0 + 9, x1 - 9}, cy[4] = {y0 + 9, y0 + 9, y1 - 9, y1 - 9};
            cyl(cx[0], cy[0], 9, 0, 54, st); ringMerlons(cx[0], cy[0], 9, 54, st);
            box(x0 + 8, y0 + 8, x1 - 8, y1 - 8, 0, 42, st); merlons(x0 + 8, y0 + 8, x1 - 8, y1 - 8, 42, st);
            door(x0 + 19, x0 + 29, y1 - 8, 17, {60, 40, 20, 255}); roof(xc - 10, yc - 10, xc + 10, yc + 10, 42, 14, {150, 105, 65, 255});
            for (int i = 1; i < 4; i++) { cyl(cx[i], cy[i], 9, 0, 54, st); ringMerlons(cx[i], cy[i], 9, 54, st); }
            flag(xc, yc, 56, red); top = 80;
        } else if (ks == 4) {  // desert fortress with wooden-roofed watchtowers
            box(x0 + 6, y0 + 6, x1 - 6, y1 - 6, 0, 32, st); merlons(x0 + 6, y0 + 6, x1 - 6, y1 - 6, 32, st);
            door(x0 + 18, x0 + 30, y1 - 6, 18, {60, 40, 20, 255}); roof(xc - 9, yc - 9, xc + 9, yc + 9, 32, 10, {170, 130, 85, 255});
            for (int i = 0; i < 4; i++) { float cx = (i & 1) ? x1 - 8 : x0 + 8, cy = (i & 2) ? y1 - 8 : y0 + 8;
                box(cx - 6, cy - 6, cx + 6, cy + 6, 0, 56, shade(st, 1.05f)); box(cx - 7, cy - 7, cx + 7, cy + 7, 56, 61, {150, 110, 65, 255}); roof(cx - 8, cy - 8, cx + 8, cy + 8, 61, 11, {80, 90, 120, 255}); }
            flag(xc, yc, 42, red); top = 80;
        } else {
        box(x0 + 2, y0 + 2, x1 - 2, y1 - 2, 0, 40, st); merlons(x0 + 2, y0 + 2, x1 - 2, y1 - 2, 40, st);
        door(x0 + 16, x0 + 32, y1 - 2, 18, {70, 45, 20, 255}); door(x0 + 18, x0 + 30, y1 - 2, 24, {40, 25, 10, 255});
        box(xc - 8, yc - 8, xc + 8, yc + 8, 40, 64, shade(st, 1.05f)); merlons(xc - 8, yc - 8, xc + 8, yc + 8, 64, st);
        for (int i = 0; i < 4; i++) { float cx = (i & 1) ? x1 - 10 : x0, cy = (i & 2) ? y1 - 10 : y0; box(cx, cy, cx + 10, cy + 10, 0, 54, shade(st, 1.05f)); merlons(cx, cy, cx + 10, cy + 10, 54, st); }
        flag(xc, yc, 69, red); top = 90;
        }
        break; }
    case WOOD: box(x0 + 4, y0 + 4, x1 - 10, y1 - 4, 0, 9, {150, 110, 70, 255}); roof(x0 + 3, y0 + 3, x1 - 9, y1 - 3, 9, 8, {120, 70, 40, 255});
        box(x1 - 9, y0 + 4, x1 - 3, y0 + 9, 0, 4, BROWN); box(x1 - 9, y0 + 10, x1 - 3, y0 + 15, 0, 4, {140, 95, 50, 255}); box(x1 - 8, y0 + 6, x1 - 4, y0 + 12, 4, 8, BROWN); break;
    case QUARRY: box(x0 + 1, y0 + 1, x1 - 1, y1 - 1, 0, 3, {150, 150, 150, 255}); box(x0 + 5, y0 + 5, x0 + 12, y0 + 12, 3, 11, {175, 175, 180, 255});
        box(x0 + 13, y0 + 10, x0 + 20, y0 + 18, 3, 8, {140, 140, 145, 255}); box(x0 + 4, y0 + 14, x0 + 9, y0 + 19, 3, 6, {160, 160, 165, 255}); break;
    case IRON: box(x0 + 1, y0 + 1, x1 - 1, y1 - 1, 0, 13, {105, 90, 105, 255}); box(x0 + 4, y0 + 4, x1 - 4, y1 - 4, 13, 17, {90, 75, 90, 255});
        door(x0 + 7, x0 + 17, y1 - 1, 9, {15, 10, 15, 255}); box(x0 + 6, y1 - 2, x0 + 8, y1, 0, 10, BROWN); box(x0 + 16, y1 - 2, x0 + 18, y1, 0, 10, BROWN); break;
    case WHEAT: {
        quad(isoH(x0 + 1, y0 + 1, 1), isoH(x1 - 1, y0 + 1, 1), isoH(x1 - 1, y1 - 1, 1), isoH(x0 + 1, y1 - 1, 1), {165, 125, 60, 255});
        for (int i = 0; i < 12; i++) { float y = y0 + 3 + i * 3.6f; DrawLineEx(isoH(x0 + 2, y, 1), isoH(x1 - 2, y, 1), 2, (i % 2) ? Color{150, 185, 50, 255} : Color{210, 180, 60, 255}); }
        break; }
    case APPLE: case GRAPE: case DATE: {
        Color fr = b.type == APPLE ? Color{210, 50, 40, 255} : b.type == GRAPE ? Color{110, 50, 150, 255} : Color{200, 110, 40, 255}, lf = b.type == DATE ? Color{50, 110, 60, 255} : Color{60, 130, 50, 255};
        quad(isoH(x0 + 1, y0 + 1, 1), isoH(x1 - 1, y0 + 1, 1), isoH(x1 - 1, y1 - 1, 1), isoH(x0 + 1, y1 - 1, 1), {110, 75, 45, 255});
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) { Vector2 p = isoH(x0 + 8 + i * 16, y0 + 8 + j * 16, 12); DrawCircleV(p, 8, lf); DrawCircleV({p.x - 3, p.y - 2}, 1.8f, fr); DrawCircleV({p.x + 3, p.y + 2}, 1.8f, fr); }
        top = 40; break; }
    case HOUSE: box(x0 + 3, y0 + 3, x1 - 3, y1 - 3, 0, 12, {225, 205, 160, 255}); roof(x0 + 1, y0 + 1, x1 - 1, y1 - 1, 12, 10, {190, 70, 50, 255});
        door(x0 + 9, x0 + 14, y1 - 3, 7, {100, 60, 30, 255}); win(x0 + 4.5f, x0 + 7.5f, y1 - 3, 5, 10); win(x0 + 16, x0 + 19, y1 - 3, 5, 10); break;
    case FLETCHER: case POLE: case SMITH: {
        Color w = b.type == SMITH ? Color{120, 120, 130, 255} : Color{200, 165, 110, 255}, r = b.type == FLETCHER ? Color{70, 130, 60, 255} : b.type == POLE ? Color{60, 100, 160, 255} : Color{70, 60, 60, 255};
        box(x0 + 3, y0 + 3, x1 - 3, y1 - 3, 0, 11, w); roof(x0 + 1, y0 + 1, x1 - 1, y1 - 1, 11, 9, r); door(x0 + 9, x0 + 14, y1 - 3, 7, {80, 50, 25, 255});
        if (b.type == SMITH) { box(x1 - 8, y0 + 5, x1 - 4, y0 + 9, 11, 22, {90, 90, 90, 255}); DrawCircleV(isoH(x1 - 6, y0 + 7, 24), 3 + sinf(gtime * 8), ORANGE); }
        break; }
    case BARRACKS: box(x0 + 1, y0 + 1, x1 - 1, y1 - 1, 0, 16, {170, 150, 130, 255}); roof(x0, y0, x1, y1, 16, 12, {150, 40, 40, 255});
        door(x0 + 9, x0 + 15, y1 - 1, 10, {70, 40, 20, 255}); flag(x1 - 3, y0 + 3, 20, red); top = 40; break;
    case TENT:
        quad(isoH(x0 + 2, y1 - 2, 0), isoH(xc, y0 + 2, 0), isoH(xc, y1 - 2, 28), isoH(x0 + 2, y0 + 2, 0), {165, 85, 55, 255});
        quad(isoH(x1 - 2, y1 - 2, 0), isoH(xc, y0 + 2, 0), isoH(xc, y1 - 2, 28), isoH(x1 - 2, y0 + 2, 0), {125, 60, 40, 255});
        flag(xc, yc, 30, red); top = 42; break;
    case STORAGE:
        box(x0 + 2, y0 + 2, x1 - 2, y1 - 2, 0, 9, {150, 105, 60, 255});
        for (int i = 0; i < 3; i++) box(x0 + 4 + i * 6, y0 + 5, x0 + 8 + i * 6, y0 + 9, 9, 13, {110, 70, 35, 255});
        top = 24; break;
    case GRANARY:
        box(x0 + 3, y0 + 3, x1 - 3, y1 - 3, 0, 14, {190, 155, 75, 255});
        roof(x0 + 2, y0 + 2, x1 - 2, y1 - 2, 14, 9, {145, 95, 50, 255});
        top = 32; break;
    case TOWER: box(x0 + 4, y0 + 4, x1 - 4, y1 - 4, 0, 46, st); merlons(x0 + 3, y0 + 3, x1 - 3, y1 - 3, 46, st);
        door(xc - 1, xc + 2, y1 - 4, 32, {25, 20, 20, 255}); door(xc - 1, xc + 2, y1 - 4, 22, {60, 55, 50, 255}); flag(xc, yc, 51, red); top = 70; break;
    case WALL: box(x0, y0, x1, y1, 0, 26, st); merlons(x0, y0, x1, y1, 26, st); top = 34; break;
    case GATE: box(x0, y0, x1, y1, 0, 30, shade(st, 0.92f)); door(x0 + 3, x1 - 3, y1, 22, {90, 55, 25, 255});
        for (int i = 1; i < 4; i++) { float x = x0 + 3 + i * 4.5f; DrawLineV(isoH(x, y1, 0), isoH(x, y1, 22), {40, 25, 10, 255}); } top = 36; break;
    case CHURCH: box(x0 + 5, y0 + 5, x1 - 5, y1 - 5, 0, 22, {235, 235, 225, 255}); roof(x0 + 4, y0 + 4, x1 - 4, y1 - 4, 22, 11, {70, 100, 160, 255});
        box(xc - 5, yc - 5, xc + 5, yc + 5, 28, 46, {235, 235, 225, 255}); roof(xc - 6, yc - 6, xc + 6, yc + 6, 46, 14, {70, 100, 160, 255});
        DrawLineEx(isoH(xc, yc, 60), isoH(xc, yc, 72), 2, GOLD); DrawLineEx(isoH(xc - 3, yc, 68), isoH(xc + 3, yc, 68), 2, GOLD); top = 78; break;
    case INN: house({170, 110, 70, 255}, {200, 120, 40, 255}, 24, 14);
        box(x1 - 12, y1 - 12, x1 - 4, y1 - 4, 0, 7, {120, 80, 40, 255}); top = 46; break;
    case MARKET:
        box(x0 + 2, y0 + 2, x1 - 2, y1 - 2, 0, 7, {140, 100, 60, 255});
        box(x0 + 5, y0 + 5, x0 + 10, y0 + 10, 7, 12, ORANGE); box(x0 + 12, y0 + 9, x0 + 18, y0 + 15, 7, 11, {200, 60, 60, 255}); box(x0 + 5, y0 + 13, x0 + 10, y0 + 19, 7, 10, {90, 160, 70, 255});
        for (int i = 0; i < 4; i++) { float xa = x0 + 1 + i * 5.5f; quad(isoH(xa, y0 + 1, 24), isoH(xa + 5.5f, y0 + 1, 24), isoH(xa + 5.5f, y1 - 1, 19), isoH(xa, y1 - 1, 19), (i % 2) ? RAYWHITE : Color{200, 40, 40, 255}); }
        box(x0 + 1, y0 + 1, x0 + 3, y0 + 3, 7, 24, BROWN); box(x1 - 3, y1 - 3, x1 - 1, y1 - 1, 7, 20, BROWN); top = 38; break;
    case MILL: {
        box(x0 + 10, y0 + 10, x1 - 10, y1 - 10, 0, 28, {225, 205, 160, 255}); roof(x0 + 9, y0 + 9, x1 - 9, y1 - 9, 28, 12, {140, 95, 60, 255});
        Vector2 hub = isoH(xc, y1 - 9, 26);
        for (int k = 0; k < 4; k++) { float a = gtime * 1.2f + k * 1.5708f; Vector2 e = {hub.x + cosf(a) * 24, hub.y + sinf(a) * 24}; DrawLineEx(hub, e, 2, BROWN); DrawLineEx(lerpV(hub, e, 0.3f), e, 6, {235, 225, 200, 255}); }
        DrawCircleV(hub, 3, DARKBROWN); top = 56; break; }
    case RTOWER:
        cyl(xc, yc, 10, 0, 50, st);
        if (b.enemy && L.dome) { dome(xc, yc, 50, 10.5f, L.dome == 1 ? Color{60, 160, 150, 255} : Color{220, 175, 60, 255}); flag(xc, yc, 70, red); top = 82; }   // domed towers
        else { ringMerlons(xc, yc, 10, 50, st); flag(xc, yc, 55, red); top = 76; }
        DrawLineEx(isoH(xc + 7, yc + 7, 14), isoH(xc + 7, yc + 7, 26), 2, {25, 20, 20, 255}); break;
    case BAKERY: house({225, 200, 155, 255}, {190, 90, 55, 255}, 22, 12); box(x1 - 12, y0 + 5, x1 - 6, y0 + 11, 22, 36, {150, 90, 70, 255}); top = 50; break;
    case WINERY: house({205, 175, 140, 255}, {140, 60, 120, 255}, 22, 12); box(x0 + 4, y1 - 12, x0 + 12, y1 - 4, 0, 8, {120, 80, 40, 255}); top = 46; break;
    case CANDLE: house({240, 225, 170, 255}, {200, 150, 50, 255}, 14, 9); DrawLineEx(isoH(xc, yc, 24), isoH(xc, yc, 32), 3, {250, 240, 210, 255}); DrawCircleV(isoH(xc, yc, 35), 3 + sinf(gtime * 9), ORANGE); top = 40; break;
    case SIEGE: house({165, 125, 85, 255}, {110, 80, 50, 255}, 20, 12); box(x0 + 6, y1 - 14, x0 + 20, y1 - 8, 0, 6, {100, 70, 40, 255}); DrawCircleV(isoH(x1 - 8, y0 + 8, 8), 5, DARKBROWN); top = 44; break;
    case ENGGUILD: house({150, 155, 165, 255}, {70, 85, 110, 255}, 24, 12);
        for (int i = 0; i < 8; i++) { float a = i * 0.785f + gtime * 0.8f; Vector2 g0 = isoH(xc + 14, y1 - 3, 14); DrawLineEx(g0, {g0.x + cosf(a) * 7, g0.y + sinf(a) * 7}, 2, DARKGRAY); }
        DrawCircleV(isoH(xc + 14, y1 - 3, 14), 4, GRAY); top = 46; break;
    case HEALER: {
        house({244, 244, 238, 255}, {70, 130, 190, 255}, 16, 10);
        Color rc = {215, 35, 35, 255};
        DrawLineEx(isoH(xc, y1 - 3, 6), isoH(xc, y1 - 3, 15), 3.5f, rc); DrawLineEx(isoH(xc - 5, y1 - 3, 10.5f), isoH(xc + 5, y1 - 3, 10.5f), 3.5f, rc);
        DrawLineEx(isoH(xc, yc, 26), isoH(xc, yc, 40), 2, BROWN);
        Vector2 f0 = isoH(xc, yc, 40); DrawRectangle((int)f0.x, (int)f0.y, 11, 8, WHITE); DrawLineEx({f0.x + 5.5f, f0.y + 1}, {f0.x + 5.5f, f0.y + 7}, 2, rc); DrawLineEx({f0.x + 2, f0.y + 4}, {f0.x + 9, f0.y + 4}, 2, rc);
        float pulse = 0.5f + 0.5f * sinf(gtime * 3); DrawCircleV(isoH(xc, yc, 50), 2 + pulse * 2, Color{90, 255, 130, 200}); top = 56; break; }
    case GARDEN: { quad(isoH(x0+1,y0+1,0), isoH(x1-1,y0+1,0), isoH(x1-1,y1-1,0), isoH(x0+1,y1-1,0), {95,125,58,255}); box(x0+3,y0+3,x1-3,y1-3,0,2,{125,92,55,255}); for(int i=0;i<5;i++){ Vector2 fp=isoH(x0+5+i*3.2f,y0+7+(i%2)*3,6); DrawCircleV(fp,2.5f,i%2?Color{230,90,120,255}:Color{245,205,70,255}); } Vector2 tr=isoH(x1-8,y1-8,4); DrawLineEx(tr,{tr.x,tr.y-14},3,BROWN); DrawCircleV({tr.x,tr.y-17},9,{45,125,48,255}); top=25; break; }
    case HUNTER: {
        house({150, 108, 66, 255}, {72, 105, 62, 255}, 13, 10);
        Vector2 a0 = isoH(xc, y1 - 2, 21);   // antlers over the door
        DrawLineEx(a0, {a0.x - 6, a0.y - 5}, 1.6f, RAYWHITE); DrawLineEx(a0, {a0.x + 6, a0.y - 5}, 1.6f, RAYWHITE); DrawLineEx({a0.x - 4, a0.y - 3}, {a0.x - 7, a0.y - 8}, 1.3f, RAYWHITE); DrawLineEx({a0.x + 4, a0.y - 3}, {a0.x + 7, a0.y - 8}, 1.3f, RAYWHITE);
        box(x1 - 6, y1 - 3, x1 - 4, y1 - 1, 0, 14, DARKBROWN); box(x1 - 1, y1 - 3, x1 + 1, y1 - 1, 0, 14, DARKBROWN);   // drying rack with a pelt
        quad(isoH(x1 - 5, y1 - 2, 13), isoH(x1, y1 - 2, 13), isoH(x1, y1 - 2, 5), isoH(x1 - 5, y1 - 2, 5), Color{176, 128, 80, 255});
        top = 38; break; }
    }
    Vector2 t = isoH(xc, yc, top);
    if (!b.enemy && WORKS[b.type]) DrawCircleV({t.x, t.y - 4}, 3, b.working ? LIME : RED);
    if (!b.enemy && (b.type == INN || b.type == CHURCH)) DrawCircleV({t.x, t.y - 4}, 3, b.fed ? GOLD : RED);
    if (b.hp < INFO[b.type].hp * 0.5f) for (int i = 0; i < 3; i++) {  // damaged: fire + smoke
        float k = fmodf(gtime * 0.8f + i * 0.33f, 1.f); DrawCircleV({t.x + (i - 1) * 6.f, t.y + 10 - k * 25}, 6 * (1 - k) + 1, Color{60, 60, 60, (unsigned char)(160 * (1 - k))});
        DrawCircleV({t.x + (i - 1) * 6.f, t.y + 14 - sinf(gtime * 9 + i) * 2}, 4, ORANGE); DrawCircleV({t.x + (i - 1) * 6.f, t.y + 15}, 2, YELLOW);
    }
    if (b.hp < INFO[b.type].hp) { DrawRectangle((int)t.x - 12, (int)t.y - 12, 24, 3, DARKGRAY); DrawRectangle((int)t.x - 12, (int)t.y - 12, (int)(24 * b.hp / INFO[b.type].hp), 3, b.enemy ? RED : LIME); }
}

void drawWorker(const Worker& w) {
    if (!workersOk) return;
    int k = workerKind(B[w.bi].type); if (k < 0) return;
    Vector2 g = iso(w.x, w.y);
    int f; bool flip = !w.right;
    if (w.st == 1) { float rate = k == 0 ? 2.2f : 1.4f; f = 2 + ((int)(w.anim * rate) % 2); flip = false; }
    else if (w.st == 2) { f = 4; static const bool carryLeft[5] = {true, true, false, true, false}; flip = carryLeft[k] ? w.right : !w.right; }
    else if (w.st == 0) f = ((int)(w.anim * 4.5f) % 2) ? 1 : 0;
    else f = 0;
    const Texture2D& t = WTEX[k][f]; if (!t.id) return;
    float bob = w.moving ? fabsf(sinf(w.anim * 9)) * 1.6f : (w.st == 1 && k == 3 ? sinf(w.anim * 5) * 1.2f : 0.f);
    DrawEllipse((int)g.x, (int)g.y, 7, 3, Color{0, 0, 0, 70});
    drawSprite(t, g.x, g.y + 1 - bob, t.width * 0.18f, 0, flip);
}

// ---------------- construction animation: foundation -> scaffolding -> the building rises -> finished ----------------
Camera2D gCam = {{0, 0}, {0, 0}, 0, 1};   // the map camera (set every frame; the construction effect needs it for a scissor rectangle)
int hoverB = -1;                           // our building under the mouse
void drawConstruction(const Building& b) {
    float s = sizeOf(b.type) * TS, x0 = b.x * TS, y0 = b.y * TS, x1 = x0 + s, y1 = y0 + s, xc = (x0 + x1) / 2, yc = (y0 + y1) / 2, p = b.prog;
    Color pole = {132, 92, 52, 255}, beam = {176, 128, 76, 255};
    // 1. foundation: levelled earth, a stone border and stacks of material
    quad(isoH(x0 + 1, y0 + 1, 0), isoH(x1 - 1, y0 + 1, 0), isoH(x1 - 1, y1 - 1, 0), isoH(x0 + 1, y1 - 1, 0), Color{118, 92, 62, 255});
    Color st = {172, 162, 150, 255};
    DrawLineEx(isoH(x0 + 1, y0 + 1, 1), isoH(x1 - 1, y0 + 1, 1), 2, st); DrawLineEx(isoH(x1 - 1, y0 + 1, 1), isoH(x1 - 1, y1 - 1, 1), 2, st);
    DrawLineEx(isoH(x1 - 1, y1 - 1, 1), isoH(x0 + 1, y1 - 1, 1), 2, st); DrawLineEx(isoH(x0 + 1, y1 - 1, 1), isoH(x0 + 1, y0 + 1, 1), 2, st);
    if (p < 0.8f) { box(x0 + 3, y1 - 8, x0 + 10, y1 - 3, 0, 4, beam); box(x0 + 3, y1 - 8, x0 + 10, y1 - 3, 4, 7, Color{150, 105, 62, 255}); box(x1 - 9, y0 + 3, x1 - 3, y0 + 8, 0, 5, Color{150, 150, 150, 255}); }
    // 2. the finished building is revealed from the ground up (scissor window), so it "grows" as the work progresses
    float H = max(BH[b.type], pickH(b)) + 12, q = max(0.f, (p - 0.15f) / 0.85f);
    if (q > 0.f) {
        Vector2 tl = GetWorldToScreen2D({iso(x0, y1).x - 14, iso(x1, y1).y + 16 - H * q}, gCam), br = GetWorldToScreen2D({iso(x1, y0).x + 14, iso(x1, y1).y + 18}, gCam);
        int sx = max(0, (int)tl.x), sy = max(0, (int)tl.y), ex = min(SW, (int)br.x), ey = min(MAPH, (int)br.y);
        if (ex > sx && ey > sy) { BeginScissorMode(sx, sy, ex - sx, ey - sy); drawBuildingFinal(b); EndScissorMode(); BeginScissorMode(0, 0, SW, MAPH); }
    }
    // 3. scaffolding poles, rails and braces (taken down just before the end)
    if (p < 0.95f) {
        float Hs = H * 0.8f * min(1.f, 0.35f + p * 0.9f); float cx[4] = {x0 + 2, x1 - 2, x1 - 2, x0 + 2}, cy[4] = {y0 + 2, y0 + 2, y1 - 2, y1 - 2};
        for (int i = 0; i < 4; i++) DrawLineEx(isoH(cx[i], cy[i], 0), isoH(cx[i], cy[i], Hs), 2.2f, pole);
        for (float z = 10; z <= Hs; z += 12) for (int i = 0; i < 4; i++) DrawLineEx(isoH(cx[i], cy[i], z), isoH(cx[(i + 1) % 4], cy[(i + 1) % 4], z), 1.6f, beam);
        DrawLineEx(isoH(x0 + 2, y1 - 2, 0), isoH(x1 - 2, y1 - 2, Hs * 0.7f), 1.2f, beam); DrawLineEx(isoH(x1 - 2, y1 - 2, 0), isoH(x1 - 2, y0 + 2, Hs * 0.7f), 1.2f, beam);
    }
    // 4. two builders swinging their tools (the lumberjack frames are reused as builders)
    if (p < 0.97f) for (int k = 0; k < 2; k++) {
        float bx = k == 0 ? xc - 4 : x1 + 5, by = k == 0 ? y1 + 6 : yc + 6; Vector2 g = iso(bx, by);
        DrawEllipse((int)g.x, (int)g.y, 6, 2.5f, Color{0, 0, 0, 70});
        if (workersOk) { const Texture2D& t = WTEX[0][2 + ((int)(gtime * 2.4f + k) % 2)]; if (t.id) drawSprite(t, g.x, g.y + 1, t.width * 0.15f, 0, k == 1); }
        else { float sw = sinf(gtime * 8 + k * 2); DrawRectangle((int)g.x - 3, (int)g.y - 14, 6, 9, ORANGE); DrawCircleV({g.x, g.y - 17}, 3, Color{225, 180, 140, 255}); DrawLineEx({g.x + 2, g.y - 11}, {g.x + 8, g.y - 15 - sw * 3}, 2, DARKBROWN); }
    }
    // 5. progress bar
    Vector2 tp = isoH(xc, yc, H + 12); DrawRectangle((int)tp.x - 15, (int)tp.y - 4, 30, 5, Color{20, 14, 8, 210}); DrawRectangle((int)tp.x - 14, (int)tp.y - 3, (int)(28 * p), 3, b.enemy ? Color{210, 90, 70, 255} : GOLD);
}
void drawBuilding(const Building& b) { if (built(b)) drawBuildingFinal(b); else drawConstruction(b); }

// ---------------- animals, hunters and the healer's range ----------------
void drawAnimal(const Animal& a) {
    if (a.state == 2) return;
    Vector2 g = iso(a.x, a.y); float f = a.right ? 1.f : -1.f; bool mv = a.fleeT > 0 || hypotf(a.tx - a.x, a.ty - a.y) > 3;
    Color body = a.kind == 0 ? Color{194, 150, 96, 255} : a.kind == 1 ? Color{206, 196, 180, 255} : Color{88, 62, 48, 255};
    if (a.state == 1) {   // carcass lying on the ground
        float rx = a.kind == 1 ? 6.f : 10.f; DrawEllipse((int)g.x, (int)g.y, rx + 2, 3.5f, Color{0, 0, 0, 60}); DrawEllipse((int)g.x, (int)g.y - 3, rx, rx * 0.45f, Color{(unsigned char)(body.r * 0.8f), (unsigned char)(body.g * 0.8f), (unsigned char)(body.b * 0.8f), 255});
        DrawLineEx({g.x - 3, g.y - 5}, {g.x - 5, g.y - 10}, 1.4f, body); DrawLineEx({g.x + 3, g.y - 5}, {g.x + 5, g.y - 10}, 1.4f, body); return;
    }
    float hop = a.kind == 1 ? (mv ? fabsf(sinf(a.anim * 9)) * 3.5f : 0.f) : (mv ? fabsf(sinf(a.anim * 7)) * 1.3f : 0.f);
    DrawEllipse((int)g.x, (int)g.y, a.kind == 1 ? 5 : 10, 3, Color{0, 0, 0, 60});
    if (a.kind == 0) {   // deer
        float by = g.y - 10 - hop;
        for (int i = 0; i < 4; i++) { float lx = g.x + (i < 2 ? -6 : 5) + (i % 2) * 2.f, sw = mv ? sinf(a.anim * 8 + i * 1.6f) * 3 : 0; DrawLineEx({lx, by + 3}, {lx + sw, g.y - hop * 0.2f}, 1.6f, Color{150, 110, 70, 255}); }
        DrawEllipse((int)g.x, (int)by, 9, 4.6f, body); DrawEllipse((int)g.x + (int)(f * 2), (int)by + 2, 6, 2.2f, Color{228, 205, 165, 255});
        DrawLineEx({g.x + f * 7, by - 2}, {g.x + f * 10, by - 9}, 3, body); DrawCircleV({g.x + f * 11.f, by - 10}, 2.8f, body);
        DrawLineEx({g.x + f * 10, by - 12}, {g.x + f * 8, by - 17}, 1.2f, Color{90, 62, 40, 255}); DrawLineEx({g.x + f * 11, by - 12}, {g.x + f * 13, by - 17}, 1.2f, Color{90, 62, 40, 255});
        DrawLineEx({g.x + f * 8, by - 15}, {g.x + f * 5, by - 17}, 1.1f, Color{90, 62, 40, 255}); DrawLineEx({g.x + f * 13, by - 15}, {g.x + f * 16, by - 17}, 1.1f, Color{90, 62, 40, 255});
        DrawCircleV({g.x - f * 9, by - 2}, 1.8f, WHITE);
    } else if (a.kind == 1) {   // rabbit
        float by = g.y - 5 - hop; DrawEllipse((int)g.x, (int)by, 5, 3.6f, body); DrawCircleV({g.x + f * 5, by - 2}, 2.5f, body);
        DrawLineEx({g.x + f * 4.5f, by - 4}, {g.x + f * 3.5f, by - 9}, 1.5f, body); DrawLineEx({g.x + f * 6, by - 4}, {g.x + f * 6.5f, by - 9}, 1.5f, body); DrawCircleV({g.x - f * 5, by}, 1.6f, WHITE);
    } else {   // boar
        float by = g.y - 8 - hop;
        for (int i = 0; i < 4; i++) { float lx = g.x + (i < 2 ? -6 : 5) + (i % 2) * 2.f, sw = mv ? sinf(a.anim * 9 + i * 1.6f) * 2.5f : 0; DrawLineEx({lx, by + 3}, {lx + sw, g.y - hop * 0.2f}, 2, Color{60, 42, 32, 255}); }
        DrawEllipse((int)g.x, (int)by, 10.5f, 5.8f, body); DrawEllipse((int)(g.x + f * 10), (int)by + 1, 4.2f, 3.4f, Color{70, 50, 38, 255});
        DrawLineEx({g.x + f * 12, by + 3}, {g.x + f * 14, by - 1}, 1.4f, RAYWHITE); DrawLineEx({g.x - 6, by - 5}, {g.x + 6, by - 6}, 1.6f, Color{50, 36, 28, 255});
    }
    if (a.hp < ANIMAL_HP[a.kind] && a.fleeT > 0) DrawCircleV({g.x, g.y - 26}, 2, RED);
}
void drawHunter(const Hunter& h) {
    Vector2 g = iso(h.x, h.y); float f = h.right ? 1.f : -1.f, bob = h.moving ? fabsf(sinf(h.anim * 9)) * 1.5f : 0.f, sw = h.moving ? sinf(h.anim * 9) * 2.5f : 0.f;
    DrawEllipse((int)g.x, (int)g.y, 6, 2.5f, Color{0, 0, 0, 70});
    DrawLineEx({g.x - 2, g.y - 7 - bob}, {g.x - 2 + sw, g.y}, 2, Color{90, 65, 40, 255}); DrawLineEx({g.x + 2, g.y - 7 - bob}, {g.x + 2 - sw, g.y}, 2, Color{90, 65, 40, 255});
    DrawRectangle((int)g.x - 4, (int)(g.y - 18 - bob), 8, 11, Color{70, 112, 62, 255}); DrawRectangle((int)g.x - 4, (int)(g.y - 9 - bob), 8, 2, Color{110, 78, 45, 255});
    DrawCircleV({g.x, g.y - 22 - bob}, 3.2f, Color{226, 182, 142, 255});
    DrawTriangle({g.x - 4.5f, g.y - 23.5f - bob}, {g.x + 4.5f, g.y - 23.5f - bob}, {g.x, g.y - 29 - bob}, Color{112, 78, 44, 255}); DrawLineEx({g.x, g.y - 28 - bob}, {g.x - f * 4, g.y - 32 - bob}, 1.5f, RED);
    DrawRectangle((int)(g.x - f * 5) - 1, (int)(g.y - 19 - bob), 3, 8, Color{96, 66, 38, 255});   // quiver
    if (h.st == 2) {   // aiming: bow drawn
        Vector2 hand = {g.x + f * 8, g.y - 17 - bob}; DrawLineEx({g.x, g.y - 16 - bob}, hand, 2, Color{226, 182, 142, 255});
        DrawLineEx({hand.x, hand.y - 8}, {hand.x + f * 2, hand.y}, 1.8f, BROWN); DrawLineEx({hand.x + f * 2, hand.y}, {hand.x, hand.y + 8}, 1.8f, BROWN);
        DrawLineEx({hand.x, hand.y - 8}, {g.x + f * 2, g.y - 16 - bob}, 1, LIGHTGRAY); DrawLineEx({hand.x, hand.y + 8}, {g.x + f * 2, g.y - 16 - bob}, 1, LIGHTGRAY); DrawLineEx({g.x + f * 2, g.y - 16 - bob}, {hand.x + f * 6, hand.y}, 1.4f, DARKBROWN);
    } else {
        DrawLineEx({g.x + f * 3, g.y - 16 - bob}, {g.x + f * 7, g.y - 14 - bob}, 2, Color{226, 182, 142, 255}); DrawLineEx({g.x + f * 7, g.y - 22 - bob}, {g.x + f * 8, g.y - 8 - bob}, 1.8f, BROWN);
    }
    if (h.st == 5 && h.carry >= 0) {   // carrying the kill on his shoulders
        Color c = h.carry == 0 ? Color{194, 150, 96, 255} : h.carry == 1 ? Color{206, 196, 180, 255} : Color{88, 62, 48, 255}; float rx = h.carry == 1 ? 5.f : 8.f;
        DrawEllipse((int)g.x, (int)(g.y - 27 - bob), rx, 3, c); DrawLineEx({g.x - rx + 1, g.y - 25 - bob}, {g.x - rx, g.y - 19 - bob}, 1.5f, c); DrawLineEx({g.x + rx - 1, g.y - 25 - bob}, {g.x + rx, g.y - 19 - bob}, 1.5f, c);
    }
    if (h.st == 6) {   // butchering: knife strokes over the carcass on the ground
        float k = sinf(h.anim * 14); DrawEllipse((int)g.x + (int)(f * 9), (int)g.y - 3, h.carry == 1 ? 5 : 8, 3, h.carry == 0 ? Color{194, 150, 96, 255} : h.carry == 1 ? Color{206, 196, 180, 255} : Color{88, 62, 48, 255});
        DrawLineEx({g.x + f * 4, g.y - 15 - bob}, {g.x + f * 9, g.y - 11 + k * 3}, 2, Color{226, 182, 142, 255}); DrawLineEx({g.x + f * 9, g.y - 11 + k * 3}, {g.x + f * 12, g.y - 7 + k * 3}, 1.6f, LIGHTGRAY);
    }
}
void drawRangeRing(Vector2 c, float r, float alpha, Color col) {   // the healer's circle, drawn flat on the ground
    Vector2 g = iso(c.x, c.y); unsigned char a1 = (unsigned char)(alpha * 0.3f), a2 = (unsigned char)alpha;
    DrawEllipse((int)g.x, (int)g.y, r * 1.414f, r * 0.707f, Color{col.r, col.g, col.b, a1});
    DrawEllipseLines((int)g.x, (int)g.y, r * 1.414f, r * 0.707f, Color{col.r, col.g, col.b, a2}); DrawEllipseLines((int)g.x, (int)g.y, r * 1.414f - 1, r * 0.707f - 1, Color{col.r, col.g, col.b, a2});
    float pr = r * (0.2f + 0.8f * fmodf(gtime * 0.45f, 1.f)); DrawEllipseLines((int)g.x, (int)g.y, pr * 1.414f, pr * 0.707f, Color{col.r, col.g, col.b, (unsigned char)(alpha * 0.45f)});
}
int treeKind(int x, int y) {  // which tree sprite grows on this tile (grass = leafy trees, sand = palms, rocky sand = acacia)
    float r = hsh(x * 3 + 11, y * 5 + 7); int t = ter[y][x];
    if (t == 1) return r < 0.32f ? 0 : r < 0.49f ? 1 : r < 0.73f ? 2 : r < 0.88f ? 3 : r < 0.94f ? 6 : 5;
    if (t == 2) return r < 0.6f ? 7 : 4;
    return r < 0.8f ? 4 : 7;
}
int rockAt(int x, int y) {  // decorative boulders / gravel piles scattered on sand
    if (!rocksOk || occ[y][x] != -1 || ter[y][x] == 3 || ter[y][x] == 1) return -1;
    if (hsh(x, y) >= (ter[y][x] == 2 ? 0.15f : 0.045f)) return -1;
    return (int)(hsh(x + 91, y + 17) * 14) % 14;
}
void drawRock(int x, int y) {
    int r = rockAt(x, y); if (r < 0) return; Vector2 p = iso((x + 0.5f) * TS, (y + 0.5f) * TS);
    float w = ROCK[r].t.width * 0.2f; DrawEllipse((int)p.x, (int)p.y + 3, w * 0.45f, w * 0.17f, Color{0, 0, 0, 55});
    drawSprite(ROCK[r].t, p.x, p.y + 5, w, 0, hsh(x + 3, y + 8) > 0.5f);
}
void drawTree(int x, int y) {
    Vector2 p = iso((x + 0.5f) * TS, (y + 0.5f) * TS); DrawEllipse((int)p.x, (int)p.y, 9, 4.5f, Color{0, 0, 0, 60});
    {
        static const float TH[8] = {44, 54, 52, 46, 50, 42, 42, 30};
        int k = treeKind(x, y);
        if (TREE[k].ok) { drawSprite(TREE[k].t, p.x, p.y + 2, 0, TH[k] * (0.9f + hsh(x + 5, y + 9) * 0.25f), hsh(x + 1, y + 2) > 0.5f); return; }
    }
    if (ter[y][x] == 1) {
        DrawRectangle((int)p.x - 2, (int)p.y - 10, 4, 10, {100, 70, 40, 255});
        DrawCircleV({p.x, p.y - 20}, 10, {35, 95, 40, 255}); DrawCircleV({p.x - 7, p.y - 14}, 8, {45, 110, 45, 255}); DrawCircleV({p.x + 7, p.y - 14}, 8, {30, 85, 35, 255}); DrawCircleV({p.x - 2, p.y - 23}, 5, {70, 150, 60, 255});
    } else {  // palm
        Vector2 t = {p.x + 4, p.y - 22}; DrawLineEx(p, t, 3.5f, {125, 90, 55, 255});
        for (int i = 0; i < 6; i++) { float a = i * 1.047f + 0.3f; Vector2 e = {t.x + cosf(a) * 13, t.y + sinf(a) * 6 + 5}; DrawLineEx(t, e, 3, {55, 125, 50, 255}); DrawLineEx(t, lerpV(t, e, 0.6f), 2, {90, 160, 70, 255}); }
    }
}

void drawIdlePeasants() {
    if (B.empty() || B[0].dead) return;
    int shown = min(max(0, pop - employed), 12);
    Vector2 keep = center(B[0]);
    Vector2 fire = iso(keep.x + 34.f, keep.y + 44.f);
    DrawCircleV({fire.x, fire.y + 2}, 9, Color{55, 35, 20, 180});
    DrawLineEx({fire.x - 6, fire.y + 3}, {fire.x + 6, fire.y - 1}, 3, DARKBROWN);
    DrawLineEx({fire.x - 5, fire.y - 1}, {fire.x + 6, fire.y + 3}, 3, BROWN);
    float flicker = 0.8f + 0.2f * sinf(gtime * 9.f);
    DrawTriangle({fire.x, fire.y - 15.f * flicker}, {fire.x - 6, fire.y + 1}, {fire.x + 6, fire.y + 1}, ORANGE);
    DrawTriangle({fire.x, fire.y - 9.f * flicker}, {fire.x - 3, fire.y}, {fire.x + 3, fire.y}, YELLOW);
    for (int i = 0; i < shown; ++i) {
        float a = (float)i * 6.2831853f / max(1, shown) + gtime * 0.06f;
        float rx = 28.f + (i % 2) * 9.f, ry = 17.f + (i % 3) * 3.f;
        Vector2 q = iso(keep.x + 34.f + cosf(a) * rx, keep.y + 44.f + sinf(a) * ry);
        float bob = sinf(gtime * 2.5f + i * 1.7f) * 1.2f;
        DrawEllipse((int)q.x, (int)q.y + 1, 5, 2.5f, Color{0, 0, 0, 65});
        DrawRectangle((int)q.x - 3, (int)(q.y - 10 + bob), 6, 7, (i % 2) ? Color{120, 75, 45, 255} : Color{90, 110, 135, 255});
        DrawCircle((int)q.x, (int)(q.y - 13 + bob), 2.8f, Color{225, 180, 140, 255});
    }
}

void drawUnit(const Unit& u) {
    const UInfo& k = UINFO[u.type]; Vector2 p = iso(u.p.x, u.p.y);
    float fx = cosf(u.face), fy = sinf(u.face); Vector2 sd = {fx - fy, (fx + fy) * 0.5f}; float l = max(0.01f, hypotf(sd.x, sd.y)); sd = {sd.x / l, sd.y / l};
    Vector2 pr = {-sd.y, sd.x}; DrawEllipse((int)p.x, (int)p.y, 7, 3.5f, Color{0, 0, 0, 80});
    if (u.sel) DrawEllipseLines((int)p.x, (int)p.y, 11, 5.5f, GREEN);
    float hpy = p.y - 28;
    Color team = u.enemy ? Color{200, 40, 40, 255} : Color{50, 90, 210, 255};
    if (u.type == HORSE_ARCHER && HORSE_ARCHER_FR[0].id) {
        // The supplied transparent sprites contain three bow poses and one close-combat pose.
        // Cycle the bow poses while moving; briefly cycle them again when the attack cooldown starts.
        int frame = 0;
        if (u.cd > k.cd - 0.24f && u.cd > 0.f) frame = (int)(u.anim * 5.0f) % 3;
        else if (u.moving) frame = (int)(u.anim * 2.0f) % 3;
        const Texture2D& t = HORSE_ARCHER_FR[frame];
        float bob = u.moving ? fabsf(sinf(u.anim * 1.6f)) * 1.8f : 0.f;
        float w = 78.f, h = 58.f;
        DrawTexturePro(t, Rectangle{0, 0, sd.x < 0 ? -(float)t.width : (float)t.width, (float)t.height},
                       Rectangle{p.x - w * 0.5f, p.y - h - bob, w, h}, Vector2{0, 0}, 0, WHITE);
        hpy = p.y - 62 - bob;
    } else if (u.type == EHORSE || u.type == HORSE_ARCHER || u.type == CAMEL_ARCHER || u.type == KNIGHT) {
        float bob = fabsf(sinf(u.anim * 1.5f)) * 2, st = sinf(u.anim * 1.5f) * 4;
        DrawEllipse((int)p.x, (int)(p.y - 8 - bob), u.type == CAMEL_ARCHER ? 15 : 13, 6, u.type == CAMEL_ARCHER ? Color{174, 132, 75, 255} : Color{105, 72, 44, 255});
        for (int i = 0; i < 4; i++) { float lx = p.x + (i < 2 ? 8 : -8) * (sd.x >= 0 ? 1 : -1) + (i % 2 ? 3 : -3); DrawLineEx({lx, p.y - 5 - bob}, {lx + ((i % 2) ? st : -st), p.y}, 2, DARKBROWN); }
        DrawCircleV({p.x + sd.x * 13, p.y - 13 - bob}, 5, Color{120, 82, 50, 255}); DrawLineEx({p.x - sd.x * 12, p.y - 9 - bob}, {p.x - sd.x * 17, p.y - 4 - bob + st * 0.3f}, 2, DARKBROWN);
        DrawRectangle((int)p.x - 3, (int)(p.y - 22 - bob), 6, 9, k.col); DrawCircleV({p.x, p.y - 25 - bob}, 3.5f, Color{235, 190, 150, 255}); DrawCircleV({p.x, p.y - 27 - bob}, 4, Color{235, 225, 200, 255});
        DrawLineEx({p.x + 4, p.y - 18 - bob}, {p.x + 4 + sd.x * 14, p.y - 26 - bob}, 2, BROWN);
        if (u.type == KNIGHT) {   // heavy cavalry: plate armour, horse barding in the team colour, long lance, plume
            DrawEllipse((int)p.x, (int)(p.y - 8 - bob), 11, 5, Color{team.r, team.g, team.b, 170});
            DrawRectangle((int)p.x - 4, (int)(p.y - 22 - bob), 8, 9, Color{175, 180, 195, 255}); DrawRectangle((int)p.x - 4, (int)(p.y - 17 - bob), 8, 2, team);
            DrawCircleV({p.x, p.y - 27 - bob}, 4.2f, Color{190, 195, 205, 255}); DrawRectangle((int)p.x - 1, (int)(p.y - 29 - bob), 2, 3, BLACK); DrawLineEx({p.x, p.y - 31 - bob}, {p.x - sd.x * 4, p.y - 36 - bob}, 2, team);
            DrawLineEx({p.x + 3, p.y - 20 - bob}, {p.x + 3 + sd.x * 22, p.y - 31 - bob}, 2, DARKBROWN); DrawCircleV({p.x + 3 + sd.x * 22, p.y - 31 - bob}, 1.8f, LIGHTGRAY);
        }
        if (u.type == HORSE_ARCHER || u.type == CAMEL_ARCHER) {
            DrawLineEx({p.x + sd.x * 5, p.y - 19 - bob}, {p.x + sd.x * 14, p.y - 24 - bob}, 2, BROWN);
            DrawLineEx({p.x + sd.x * 7, p.y - 28 - bob}, {p.x + sd.x * 7, p.y - 17 - bob}, 1, GOLD);
        }
        hpy = p.y - 38;
    } else if ((u.type == RAM || u.type == DEMOLISHER)) {  // battering ram / demolisher: roofed frame on wheels with a swinging log
        float sw = (u.cd > k.cd - 0.5f) ? 9.f : -3.f;
        DrawRectangle((int)p.x - 15, (int)p.y - 17, 30, 3, {95, 65, 40, 255}); DrawRectangle((int)p.x - 15, (int)p.y - 17, 30, 1, {150, 105, 60, 255});
        DrawTriangle({p.x - 16, p.y - 17}, {p.x, p.y - 28}, {p.x + 16, p.y - 17}, Color{team.r, team.g, team.b, 255});
        DrawLineEx({p.x - sd.x * 14, p.y - 12}, {p.x + sd.x * (12 + sw), p.y - 12}, 5, {120, 80, 50, 255}); DrawCircleV({p.x + sd.x * (14 + sw), p.y - 12}, 4, DARKGRAY);
        DrawCircleV({p.x - 10, p.y - 3}, 4, DARKBROWN); DrawCircleV({p.x + 10, p.y - 3}, 4, DARKBROWN); hpy = p.y - 34;
    } else if (u.type == CATA) {
        DrawRectangle((int)p.x - 13, (int)p.y - 11, 26, 8, k.col); DrawRectangle((int)p.x - 13, (int)p.y - 11, 26, 2, {150, 105, 60, 255});
        DrawCircleV({p.x - 9, p.y - 2}, 5, DARKBROWN); DrawCircleV({p.x + 9, p.y - 2}, 5, DARKBROWN); DrawCircleV({p.x - 9, p.y - 2}, 2, GRAY); DrawCircleV({p.x + 9, p.y - 2}, 2, GRAY);
        Vector2 tip = (u.cd > k.cd - 0.4f) ? Vector2{p.x + sd.x * 16, p.y - 24} : Vector2{p.x - sd.x * 12, p.y - 22};
        DrawLineEx({p.x, p.y - 10}, tip, 3, DARKBROWN); DrawCircleV(tip, 4, DARKGRAY); DrawRectangle((int)p.x - 2, (int)p.y - 16, 4, 4, team); hpy = p.y - 32;
    } else {
        float bob = fabsf(sinf(u.anim)) * 1.5f, st = sinf(u.anim) * 3;
        DrawLineEx({p.x - 2, p.y - 5}, {p.x - 2 + st * 0.6f, p.y}, 2, DARKBROWN); DrawLineEx({p.x + 2, p.y - 5}, {p.x + 2 - st * 0.6f, p.y}, 2, DARKBROWN);
        Color skin = {235, 190, 150, 255};
        DrawRectangle((int)p.x - 4, (int)(p.y - 15 - bob), 8, 11, k.col); DrawRectangle((int)p.x - 4, (int)(p.y - 9 - bob), 8, 2, DARKBROWN);
        DrawCircleV({p.x, p.y - 18 - bob}, 3.5f, skin);
        float hx = p.x + pr.x * 5, hy = p.y - 11 - bob + pr.y * 2.5f, th = (u.cd > k.cd - 0.2f) ? 1.6f : 1.f;
        switch (u.type) {
        case SPEAR: DrawRectangle((int)p.x - 4, (int)(p.y - 23 - bob), 8, 4, GRAY); DrawLineEx({hx - sd.x * 5, hy + 3}, {hx + sd.x * 13 * th, hy - 10}, 2, BROWN); DrawCircleV({hx + sd.x * 13 * th, hy - 10}, 2, LIGHTGRAY); DrawCircleV({p.x - pr.x * 5, p.y - 10 - bob}, 5, GOLD); break;
        case SWORD: DrawRectangle((int)p.x - 4, (int)(p.y - 23 - bob), 8, 5, LIGHTGRAY); DrawRectangle((int)p.x - 1, (int)(p.y - 26 - bob), 2, 4, RED); DrawLineEx({hx, hy}, {hx + sd.x * 10 * th, hy - 7}, 2, RAYWHITE); DrawCircleV({p.x - pr.x * 5, p.y - 10 - bob}, 5, SKYBLUE); DrawCircleLines((int)(p.x - pr.x * 5), (int)(p.y - 10 - bob), 5, DARKBLUE); break;
        case ARCHER: DrawCircleV({p.x, p.y - 21 - bob}, 4, DARKGREEN); DrawRectangle((int)(p.x - sd.x * 5), (int)(p.y - 15 - bob), 3, 8, BROWN);
            DrawLineEx({hx + sd.x * 4, hy - 8}, {hx + sd.x * 9, hy}, 1.5f, BROWN); DrawLineEx({hx + sd.x * 9, hy}, {hx + sd.x * 4, hy + 8}, 1.5f, BROWN); DrawLineV({hx + sd.x * 4, hy - 8}, {hx + sd.x * 4, hy + 8}, LIGHTGRAY); break;
        case SLAVE: DrawLineEx({hx, hy}, {hx + sd.x * 8, hy - 6}, 3, DARKBROWN); break;
        case MACE: DrawRectangle((int)p.x - 4, (int)(p.y - 23 - bob), 8, 5, Color{150, 155, 170, 255}); DrawRectangle((int)p.x - 4, (int)(p.y - 12 - bob), 8, 3, team);
            DrawLineEx({hx, hy + 2}, {hx + sd.x * 9 * th, hy - 8}, 2, DARKBROWN); DrawCircleV({hx + sd.x * 10 * th, hy - 9}, 3.2f, DARKGRAY); DrawCircleV({p.x - pr.x * 5, p.y - 10 - bob}, 5, team); break;
        case CROSSBOW: DrawRectangle((int)p.x - 4, (int)(p.y - 23 - bob), 8, 4, Color{50, 110, 90, 255}); DrawRectangle((int)p.x - 4, (int)(p.y - 12 - bob), 8, 2, team);
            DrawLineEx({hx, hy}, {hx + sd.x * 9, hy - 2}, 2, BROWN); DrawLineEx({hx + sd.x * 5, hy - 6}, {hx + sd.x * 5, hy + 4}, 2, DARKBROWN); DrawLineEx({hx + sd.x * 5, hy - 6}, {hx + sd.x * 9, hy - 2}, 1, LIGHTGRAY); DrawLineEx({hx + sd.x * 5, hy + 4}, {hx + sd.x * 9, hy - 2}, 1, LIGHTGRAY); break;
        case ENGINEER: DrawRectangle((int)p.x - 4, (int)(p.y - 23 - bob), 8, 4, YELLOW); DrawRectangle((int)p.x - 5, (int)(p.y - 19 - bob), 10, 1, GOLD); DrawRectangle((int)p.x - 4, (int)(p.y - 9 - bob), 8, 2, BROWN);
            { float sw = (u.anim * 3.f - floorf(u.anim * 3.f)) > 0.5f ? 5.f : 0.f; DrawLineEx({hx, hy}, {hx + sd.x * 6, hy - 6 - sw * 0.4f}, 2, DARKBROWN); DrawRectangle((int)(hx + sd.x * 6) - 3, (int)(hy - 8 - sw * 0.4f), 6, 4, GRAY); } break;
        case EARCHER: DrawCircleV({p.x, p.y - 21 - bob}, 4.5f, {235, 225, 200, 255});
            DrawLineEx({hx + sd.x * 4, hy - 8}, {hx + sd.x * 9, hy}, 1.5f, BROWN); DrawLineEx({hx + sd.x * 9, hy}, {hx + sd.x * 4, hy + 8}, 1.5f, BROWN); DrawLineV({hx + sd.x * 4, hy - 8}, {hx + sd.x * 4, hy + 8}, LIGHTGRAY); break;
        case ARAB_SWORD: case ARAB_GUARD: DrawRectangle((int)p.x - 4, (int)(p.y - 23 - bob), 8, 5, Color{185, 145, 85, 255}); DrawLineEx({hx, hy}, {hx + sd.x * (u.type == ARAB_GUARD ? 13 : 10) * th, hy - 8}, 2.5f, LIGHTGRAY); DrawCircleV({p.x - pr.x * 5, p.y - 10 - bob}, 5, GOLD); break;
        case LONGBOW: case PIKEMAN: DrawRectangle((int)p.x - 4, (int)(p.y - 23 - bob), 8, 4, LIGHTGRAY); DrawLineEx({hx, hy}, {hx + sd.x * 12, hy - 9}, 2, BROWN); break;
        case ESWORD: DrawCircleV({p.x, p.y - 21 - bob}, 4.5f, {40, 30, 30, 255}); DrawRectangle((int)p.x - 3, (int)(p.y - 19 - bob), 6, 2, BLACK);
            DrawLineEx({hx, hy}, {hx + sd.x * 10 * th, hy - 7}, 2, RAYWHITE); DrawCircleV({p.x - pr.x * 5, p.y - 10 - bob}, 5, MAROON); break;
        }
        if (u.enemy && u.type != ESWORD && u.type != SLAVE) DrawRectangle((int)p.x - 4, (int)(p.y - 15 - bob), 8, 2, {240, 230, 200, 255});
    }
    if (u.hp < k.hp || u.sel) { DrawRectangle((int)p.x - 9, (int)hpy, 18, 3, DARKGRAY); DrawRectangle((int)p.x - 9, (int)hpy, (int)(18 * min(1.f, u.hp / k.hp)), 3, u.enemy ? RED : LIME); }
    if (u.sel && !u.enemy) {
        int rank = 0; for (const auto& other : U) if (!other.enemy && other.sel) { rank++; if (&other == &u) break; }
        DrawCircle((int)p.x, (int)(hpy - 10), 8, Color{20, 35, 20, 230});
        DrawCircleLines((int)p.x, (int)(hpy - 10), 8, LIME);
        const char* label = TextFormat("%d", rank);
        DrawText(label, (int)p.x - MeasureText(label, 9) / 2, (int)(hpy - 14), 9, WHITE);
    }
}

void hullShadow(vector<Vector2> p, Color c) {  // fills the convex hull of the points (used for cast shadows)
    sort(p.begin(), p.end(), [](Vector2 a, Vector2 b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
    auto cr = [](Vector2 o, Vector2 a, Vector2 b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); };
    vector<Vector2> h(2 * p.size()); int k = 0;
    for (size_t i = 0; i < p.size(); i++) { while (k >= 2 && cr(h[k - 2], h[k - 1], p[i]) <= 0) k--; h[k++] = p[i]; }
    for (int i = (int)p.size() - 2, t = k + 1; i >= 0; i--) { while (k >= t && cr(h[k - 2], h[k - 1], p[i]) <= 0) k--; h[k++] = p[i]; }
    for (int i = 1; i + 2 < k; i++) tri(h[0], h[i], h[i + 1], c);
}

void drawGround() {
    for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) {
        Vector2 cc0 = iso((x + 0.5f) * TS, (y + 0.5f) * TS); if (!inView(cc0, 50)) continue;
        Vector2 t = iso(x * TS, y * TS), r = iso((x + 1) * TS, y * TS), b = iso((x + 1) * TS, (y + 1) * TS), l = iso(x * TS, (y + 1) * TS), c = lerpV(t, b, 0.5f);
        float h = hsh(x, y), sn = 0.5f + 0.25f * sinf(x * 0.45f + 1.3f) * cosf(y * 0.37f) + 0.25f * sinf((x + y) * 0.21f);
        int me = ter[y][x], wn = 0; float R = 0, G = 0, Bb = 0, W = 0;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {  // blend biomes with their neighbours for smooth borders
            int nx = x + dx, ny = y + dy; if (nx < 0 || ny < 0 || nx >= MW || ny >= MH) continue;
            int nt = ter[ny][nx]; if (nt == 3) wn++;
            if ((nt == 3) != (me == 3)) continue;
            float w = (dx == 0 && dy == 0) ? 3.f : 1.f; Color bc = terBase(nt); R += bc.r * w; G += bc.g * w; Bb += bc.b * w; W += w;
        }
        Color col = {(unsigned char)(R / W), (unsigned char)(G / W), (unsigned char)(Bb / W), 255};
        if (me == 3) col = shade(col, 0.72f + 0.035f * wn + 0.06f * sinf(gtime * 1.6f + x * 0.7f + y * 0.5f));
        else { col = shade(col, 0.88f + 0.24f * sn + 0.04f * h); if (wn) col = shade(col, 0.86f); }
        quad(t, r, b, l, col);
        if (!cfg.detail) continue;
        if (me == 3) {
            if (wn < 9) quad(t, r, b, l, Color{210, 240, 250, (unsigned char)min(110, (9 - wn) * 28)});  // shore foam
            if (h > 0.5f) { float yy = c.y + sinf(gtime * 2 + x) * 2; DrawLineV({c.x - 7, yy}, {c.x + 6, yy}, Color{200, 235, 250, 90}); }
        } else if (me == 0 && h > 0.88f) DrawEllipse((int)c.x, (int)c.y, 9, 3, Color{240, 215, 160, 120});
        else if (me == 1 && h > 0.25f) {
            // denser layered clumps of tiny grass blades, deterministic per tile
            int blades = 4 + (int)(h * 7);
            for (int g = 0; g < blades; g++) {
                float gx = c.x + (hsh(x * 17 + g * 3, y * 11 + g) - 0.5f) * 20.f;
                float gy = c.y + (hsh(x * 7 + g, y * 19 + g * 5) - 0.5f) * 8.f;
                float gh = 3.f + hsh(x + g * 13, y + g * 7) * 5.f;
                Color blade = (g % 3 == 0) ? Color{48, 105, 38, 230} : (g % 3 == 1) ? Color{76, 135, 48, 230} : Color{105, 150, 58, 210};
                DrawLineEx({gx, gy}, {gx - 2.f, gy - gh}, 1.25f, blade);
                DrawLineEx({gx, gy}, {gx + 2.f, gy - gh * 0.8f}, 1.1f, blade);
            }
            if (h > 0.9f) DrawCircleV({c.x + 4, c.y - 3}, 1.6f, Color{240, 220, 90, 255});
        }
        else if (me != 1 && h < 0.06f && occ[y][x] == -1 && !rocksOk) { DrawEllipse((int)c.x, (int)c.y, 6, 3, Color{0, 0, 0, 50}); DrawCircleV({c.x, c.y - 3}, 5, {150, 145, 140, 255}); DrawCircleV({c.x - 1, c.y - 4}, 3, {185, 180, 172, 255}); }
    }
}

void drawParts() {  // dust, sparks, smoke, debris
    for (auto& q : PT) {
        Vector2 g = iso(q.x, q.y), s = {g.x, g.y - q.z}; float f = max(0.f, q.life / q.max);
        if (q.kind == 0) DrawCircleV(s, 3 + (1 - f) * 4, Color{200, 175, 125, (unsigned char)(90 * f)});
        else if (q.kind == 1) DrawCircleV(s, 1.6f, Color{255, (unsigned char)(200 * f + 40), 60, (unsigned char)(255 * f)});
        else if (q.kind == 2) DrawCircleV(s, 4 + (1 - f) * 8, Color{70, 70, 70, (unsigned char)(120 * f)});
        else if (q.kind == 4) { Color c = {90, 255, 130, (unsigned char)(230 * f)}; DrawLineEx({s.x - 3, s.y}, {s.x + 3, s.y}, 2, c); DrawLineEx({s.x, s.y - 3}, {s.x, s.y + 3}, 2, c); }
        else DrawRectangle((int)s.x, (int)s.y, 3, 3, Color{120, 90, 60, (unsigned char)(255 * f)});
    }
}

void drawClouds() {  // soft cloud shadows drifting over the land
    for (int i = 0; i < 5; i++) {
        float cx = fmodf(gtime * 14 + i * 420, 2100) - 600, cy = 60 + i * 150 + sinf(gtime * 0.2f + i) * 20;
        DrawEllipse((int)cx, (int)cy, 170, 62, Color{0, 0, 0, 20}); DrawEllipse((int)cx + 60, (int)cy - 18, 110, 45, Color{0, 0, 0, 16});
    }
}

struct Item { float d; int k, i; };

int pickB(Vector2 w, int side) {  // building under the mouse (side: 0 = ours, 1 = enemy)
    int best = -1; float bd = -1e9f;
    for (size_t i = 0; i < B.size(); i++) {
        if (B[i].dead || B[i].enemy != (side == 1)) continue;
        float s = sizeOf(B[i].type) * TS, x0 = B[i].x * TS, y0 = B[i].y * TS, ph = pickH(B[i]), mg = ph > 0 ? 8.f : 0.f, bh = max(BH[B[i].type], ph);
        if (w.x >= iso(x0, y0 + s).x - mg && w.x <= iso(x0 + s, y0).x + mg && w.y >= iso(x0, y0).y - bh && w.y <= iso(x0 + s, y0 + s).y && x0 + y0 > bd) { bd = x0 + y0; best = (int)i; }
    }
    return best;
}

// ---------------- window / display ----------------
const int WIN_W[3] = {1920, 2560, 3840}, WIN_H[3] = {1080, 1440, 2160};
bool isFs = false; Vector2 savedPos = {40, 40};
void applyDisplay() {
    if (cfg.vsync) SetWindowState(FLAG_VSYNC_HINT); else ClearWindowState(FLAG_VSYNC_HINT);
    static const int CAPS[4] = {30, 60, 120, 0}; SetTargetFPS(CAPS[cfg.fpsCap]);
    if (cfg.fullscreen && !isFs) {
        savedPos = GetWindowPosition(); int mon = GetCurrentMonitor(); Vector2 mp = GetMonitorPosition(mon);
        SetWindowState(FLAG_WINDOW_UNDECORATED); SetWindowPosition((int)mp.x, (int)mp.y); SetWindowSize(GetMonitorWidth(mon), GetMonitorHeight(mon)); isFs = true;
    } else if (!cfg.fullscreen) {
        if (isFs) { ClearWindowState(FLAG_WINDOW_UNDECORATED); isFs = false; SetWindowPosition((int)savedPos.x, (int)savedPos.y); }
        SetWindowSize(WIN_W[cfg.winScale], WIN_H[cfg.winScale]);
    }
}
void applyFilters() {
    auto f = [](Texture2D& t) { if (t.id) SetTextureFilter(t, cfg.smooth ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT); };
    for (int i = 0; i < NSPR; i++) f(SP[i].t); for (int i = 0; i < 8; i++) f(TREE[i].t); for (int i = 0; i < 14; i++) f(ROCK[i].t); for (int k = 0; k < 5; k++) for (int j = 0; j < 5; j++) f(WTEX[k][j]);
}

// ---------------- UI helpers ----------------
bool btn(Rectangle r, const char* label, bool sel, Vector2 m, bool click, int fs = 14) {  // draws a button, returns true when clicked
    bool h = CheckCollisionPointRec(m, r);
    DrawRectangleRec(r, sel ? Color{125, 100, 40, 255} : (h ? Color{100, 78, 46, 255} : Color{68, 52, 36, 255}));
    DrawRectangleLinesEx(r, 1, sel ? GOLD : Color{150, 125, 90, 255});
    DrawText(label, (int)(r.x + r.width / 2 - MeasureText(label, fs) / 2), (int)(r.y + r.height / 2 - fs / 2), fs, RAYWHITE);
    if (h && click) sfx(SFX_CLICK, 0.5f, 0.03f);
    return h && click;
}
int activeSlider = -1;
void slider(int id, Rectangle r, float& v, Vector2 m, bool pressed, bool down) {
    DrawRectangleRec(r, Color{30, 22, 14, 255}); DrawRectangle((int)r.x, (int)r.y, (int)(r.width * v), (int)r.height, Color{190, 150, 50, 255}); DrawRectangleLinesEx(r, 1, GOLD);
    DrawCircle((int)(r.x + r.width * v), (int)(r.y + r.height / 2), 8, RAYWHITE);
    Rectangle hit = {r.x - 8, r.y - 8, r.width + 16, r.height + 16};
    if (pressed && CheckCollisionPointRec(m, hit)) activeSlider = id;
    if (!down) { if (activeSlider == id) saveSettings(); if (activeSlider == id) activeSlider = -1; }
    if (activeSlider == id) v = max(0.f, min(1.f, (m.x - r.x) / r.width));
}
bool optOpen = false, advOpen = false, helpOpen = false, quitMenu = false;
void optionsUI(Vector2 m, bool click, bool down) {
    Rectangle pr = {190, 74, 580, 540};
    DrawRectangle(0, 0, SW, SH, Color{0, 0, 0, 150}); DrawRectangleRec(pr, Color{35, 25, 15, 248}); DrawRectangleLinesEx(pr, 3, GOLD);
    DrawText("OPTIONS", (int)pr.x + 20, (int)pr.y + 12, 24, GOLD);
    int lx = (int)pr.x + 20; float cx = pr.x + 205; bool changed = false;
    auto lab = [&](const char* t, float y) { DrawText(t, lx, (int)y + 3, 13, RAYWHITE); };
    auto head = [&](const char* t, float y) { DrawText(t, lx, (int)y, 14, GOLD); DrawLine(lx, (int)y + 18, (int)(pr.x + pr.width - 20), (int)y + 18, Color{120, 95, 60, 255}); };
    auto tog = [&](const char* name, bool& v, float y) { lab(name, y); if (btn({cx, y - 2, 115, 22}, v ? "ON" : "OFF", v, m, click, 12)) { v = !v; changed = true; } };
    head("SOUND", pr.y + 42);
    lab("Master", pr.y + 66); slider(1, {cx, pr.y + 72, 330, 10}, cfg.master, m, click, down);
    lab("Music", pr.y + 90); slider(2, {cx, pr.y + 96, 330, 10}, cfg.music, m, click, down);
    lab("Effects", pr.y + 114); slider(3, {cx, pr.y + 120, 330, 10}, cfg.sfx, m, click, down);
    head("DISPLAY", pr.y + 144);
    lab("Screen mode", pr.y + 168);
    if (btn({cx, pr.y + 166, 145, 23}, "Windowed", !cfg.fullscreen, m, click, 12) && cfg.fullscreen) { cfg.fullscreen = false; applyDisplay(); changed = true; }
    if (btn({cx + 153, pr.y + 166, 145, 23}, "Full screen", cfg.fullscreen, m, click, 12) && !cfg.fullscreen) { cfg.fullscreen = true; applyDisplay(); changed = true; }
    lab("Window size", pr.y + 194);
    static const char* WN[3] = {"Full HD", "QHD 1440p", "4K UHD"}; static const char* WR[3] = {"1080p", "1440p", "4K"};
    for (int i = 0; i < 3; i++) if (btn({cx + i * 83.f, pr.y + 192, 79, 23}, WR[i], cfg.winScale == i, m, click, 11)) { cfg.winScale = i; if (!cfg.fullscreen) applyDisplay(); changed = true; }
    DrawText(WN[cfg.winScale], (int)cx + 260, (int)pr.y + 198, 10, LIGHTGRAY);
    lab("VSync", pr.y + 220); if (btn({cx, pr.y + 218, 90, 22}, cfg.vsync ? "ON" : "OFF", cfg.vsync, m, click, 12)) { cfg.vsync = !cfg.vsync; applyDisplay(); changed = true; }
    lab("FPS cap", pr.y + 246); static const char* FN[4] = {"30", "60", "120", "None"};
    for (int i = 0; i < 4; i++) if (btn({cx + i * 62.f, pr.y + 244, 58, 22}, FN[i], cfg.fpsCap == i, m, click, 11)) { cfg.fpsCap = i; applyDisplay(); changed = true; }
    lab("Show FPS", pr.y + 272); if (btn({cx, pr.y + 270, 70, 22}, cfg.showFps ? "ON" : "OFF", cfg.showFps, m, click, 12)) { cfg.showFps = !cfg.showFps; changed = true; }
    DrawText("Cheats", (int)cx + 90, (int)pr.y + 274, 12, cfg.cheats ? GOLD : RAYWHITE);
    if (btn({cx + 140, pr.y + 270, 70, 22}, cfg.cheats ? "ON" : "OFF", cfg.cheats, m, click, 12)) { cfg.cheats = !cfg.cheats; if (!cfg.cheats) { cheatInstant = cheatGod = false; cheatOpen = false; } changed = true; }
    if (cfg.cheats && btn({cx + 216, pr.y + 270, 82, 22}, "Menu F9", false, m, click, 11) && !inMenu) { cheatOpen = true; optOpen = false; }
    head("GRAPHICS", pr.y + 300); lab("Quality", pr.y + 324); static const char* QN[3] = {"Low", "Medium", "High"};
    for (int i = 0; i < 3; i++) if (btn({cx + i * 83.f, pr.y + 322, 79, 22}, QN[i], cfg.quality == i, m, click, 11)) { applyQuality(i); applyFilters(); changed = true; }
    tog("Shadows", cfg.shadows, pr.y + 350); tog("Clouds", cfg.clouds, pr.y + 374); tog("Vignette", cfg.vignette, pr.y + 398); tog("Ground detail", cfg.detail, pr.y + 422);
    { bool before = cfg.smooth; tog("Smooth textures", cfg.smooth, pr.y + 446); if (before != cfg.smooth) applyFilters(); }
    lab("Particles", pr.y + 470); static const char* PN[3] = {"Off", "Few", "All"};
    for (int i = 0; i < 3; i++) if (btn({cx + i * 83.f, pr.y + 468, 79, 22}, PN[i], cfg.particles == i, m, click, 11)) { cfg.particles = i; changed = true; }
    if (!inMenu && btn({pr.x + 20, pr.y + pr.height - 38, 180, 28}, "Quit to main menu", false, m, click, 12)) { quitMenu = true; optOpen = false; }
    if (btn({pr.x + pr.width - 112, pr.y + pr.height - 38, 92, 28}, "Close", false, m, click, 13)) { optOpen = false; changed = true; }
    if (changed) saveSettings();
}
// ---------------- shop & training popups ----------------
int openB = -1; // -1 = closed, -2 = market opened via HUD/hotkey, >=0 = building index
const int T_QTYS[5] = {1, 10, 20, 50, 100};
const int A_LIMS[4] = {10, 20, 50, 100};
bool isMarketOpen() { return openB == -2 || (openB >= 0 && B[openB].type == MARKET); }
bool isPopupOpen() { return openB == -2 || openB >= 0; }
const int* rosterOf(int bt, int& n) {
    n = bt == BARRACKS ? 8 : bt == TENT ? 8 : bt == SIEGE ? 3 : bt == ENGGUILD ? 1 : 0;
    return bt == BARRACKS ? ROSTER_BARRACKS : bt == TENT ? ROSTER_TENT : bt == SIEGE ? ROSTER_SIEGE : ROSTER_ENG;
}
const float MROW = 28;
Rectangle popupRect() {
    if (isMarketOpen()) return {SW - 644.f, 32.f, 636.f, 76 + NG * MROW + 8};
    int n; rosterOf(B[openB].type, n);
    return {SW - 400.f, 32.f, 392.f, 44.f + n * 30};
}
bool popupClick(Vector2 m) {
    Rectangle pr = popupRect(); if (!CheckCollisionPointRec(m, pr)) return false;
    bool mk = isMarketOpen(), sh = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (mk) {
        if (CheckCollisionPointRec(m, Rectangle{pr.x + 262, pr.y + 5, 118, 22})) { autoTrade = !autoTrade; return true; }
        if (CheckCollisionPointRec(m, Rectangle{pr.x + 386, pr.y + 5, 208, 22})) { autoSellUpTo = !autoSellUpTo; return true; }
        if (CheckCollisionPointRec(m, Rectangle{pr.x + 600, pr.y + 5, 28, 22})) { openB = -1; return true; }
        for (int k = 0; k < 5; k++) if (CheckCollisionPointRec(m, Rectangle{pr.x + 96 + k * 34.f, pr.y + 32, 32, 20})) {
            for (int i = 0; i < NG; i++) { if (k == 0) autoBuyOn[i] = false; else { autoBuyOn[i] = true; autoBuyLim[i] = A_LIMS[k - 1]; autoTrade = true; } }
            return true; }
        for (int k = 0; k < 6; k++) if (CheckCollisionPointRec(m, Rectangle{pr.x + 364 + k * 36.f, pr.y + 32, 34, 20})) {
            for (int i = 0; i < NG; i++) {
                if (k == 0) autoSellOn[i] = false;
                else if (k < 5) { autoSellOn[i] = true; autoSellLim[i] = A_LIMS[k - 1]; autoTrade = true; }
                else { autoSellOn[i] = true; autoSellLim[i] = 0; autoTrade = true; } }
            return true; }
        for (int i = 0; i < NG; i++) {
            float y = pr.y + 76 + i * MROW;
            for (int k = 0; k < 5; k++) {
                int q = T_QTYS[k];
                if (CheckCollisionPointRec(m, Rectangle{pr.x + 130 + k * 35.f, y + 2, 33, 24})) { int n = min(q, gold / BUY[i]); if (n > 0) { gold -= n * BUY[i]; *goodPtr(i) += n; sfx(SFX_COIN, 0.6f); } return true; }
                if (CheckCollisionPointRec(m, Rectangle{pr.x + 308 + k * 35.f, y + 2, 33, 24})) { int n = min(q, *goodPtr(i)); if (n > 0) { *goodPtr(i) -= n; gold += n * SELL[i]; sfx(SFX_COIN, 0.6f); } return true; }
            }
            if (CheckCollisionPointRec(m, Rectangle{pr.x + 486, y + 2, 70, 24})) {
                if (!autoBuyOn[i]) { autoBuyOn[i] = true; autoBuyLim[i] = 10; autoTrade = true; }
                else if (autoBuyLim[i] == 10) autoBuyLim[i] = 20; else if (autoBuyLim[i] == 20) autoBuyLim[i] = 50; else if (autoBuyLim[i] == 50) autoBuyLim[i] = 100; else autoBuyOn[i] = false;
                return true; }
            if (CheckCollisionPointRec(m, Rectangle{pr.x + 560, y + 2, 68, 24})) {
                if (!autoSellOn[i]) { autoSellOn[i] = true; autoSellLim[i] = 10; autoTrade = true; }
                else if (autoSellLim[i] == 10) autoSellLim[i] = 20; else if (autoSellLim[i] == 20) autoSellLim[i] = 50; else if (autoSellLim[i] == 50) autoSellLim[i] = 100; else if (autoSellLim[i] == 100) autoSellLim[i] = 0; else autoSellOn[i] = false;
                return true; }
        }
        return true;
    }
    int n; const int* r = rosterOf(B[openB].type, n);
    for (int i = 0; i < n; i++) {
        float y = pr.y + 34 + i * 30;
        for (int k = 0; k < 3; k++) if (CheckCollisionPointRec(m, Rectangle{pr.x + 218 + k * 56.f, y, 52, 24})) trainMany(r[i], (k == 2 || sh) ? -1 : (k == 1 ? 5 : 1), openB);
    }
    return true;
}
void drawPopup(Vector2 m) {
    Rectangle pr = popupRect(); bool mk = isMarketOpen();
    DrawRectangleRec(pr, Color{35, 25, 15, 242}); DrawRectangleLinesEx(pr, 2, GOLD);
    auto mbtn = [&](Rectangle r, const char* t, bool en, bool active = false, int fs = 12) {
        bool h = CheckCollisionPointRec(m, r);
        Color bg = !en ? Color{45, 40, 35, 255} : active ? (h ? Color{60, 145, 60, 255} : Color{45, 115, 45, 255}) : (h ? Color{150, 120, 40, 255} : Color{95, 75, 35, 255});
        DrawRectangleRec(r, bg); DrawRectangleLinesEx(r, 1, active ? GOLD : (en ? RAYWHITE : GRAY));
        DrawText(t, (int)(r.x + r.width / 2 - MeasureText(t, fs) / 2), (int)(r.y + r.height / 2 - fs / 2), fs, en ? RAYWHITE : GRAY);
    };
    if (mk) {
        DrawText("Market - Buy, Sell & Auto-Trade", (int)pr.x + 10, (int)pr.y + 8, 15, GOLD);
        mbtn({pr.x + 262, pr.y + 5, 118, 22}, autoTrade ? "Auto-Trade: ON" : "Auto-Trade: OFF", true, autoTrade, 12);
        mbtn({pr.x + 386, pr.y + 5, 208, 22}, autoSellUpTo ? "Sell Mode: Up To 10/20/50/100" : "Sell Mode: Keep 10/20/50/100", true, autoSellUpTo, 11);
        mbtn({pr.x + 600, pr.y + 5, 28, 22}, "X", true, false, 12);
        DrawText("All Auto-Buy:", (int)pr.x + 10, (int)pr.y + 36, 12, LIGHTGRAY);
        const char* abLbl[5] = {"OFF", "10", "20", "50", "100"};
        for (int k = 0; k < 5; k++) {
            bool allMatch = true;
            for (int i = 0; i < NG; i++) { if (k == 0 && autoBuyOn[i]) allMatch = false; if (k > 0 && (!autoBuyOn[i] || autoBuyLim[i] != A_LIMS[k - 1])) allMatch = false; }
            mbtn({pr.x + 96 + k * 34.f, pr.y + 32, 32, 20}, abLbl[k], true, k > 0 && autoTrade && allMatch, 11);
        }
        DrawText("All Auto-Sell:", (int)pr.x + 276, (int)pr.y + 36, 12, LIGHTGRAY);
        const char* asLbl[6] = {"OFF", "10", "20", "50", "100", "All"};
        for (int k = 0; k < 6; k++) {
            bool allMatch = true;
            for (int i = 0; i < NG; i++) {
                if (k == 0 && autoSellOn[i]) allMatch = false;
                else if (k > 0 && k < 5 && (!autoSellOn[i] || autoSellLim[i] != A_LIMS[k - 1])) allMatch = false;
                else if (k == 5 && (!autoSellOn[i] || autoSellLim[i] != 0)) allMatch = false;
            }
            mbtn({pr.x + 364 + k * 36.f, pr.y + 32, 34, 20}, asLbl[k], true, k > 0 && autoTrade && allMatch, 11);
        }
        DrawLine((int)pr.x + 6, (int)pr.y + 55, (int)(pr.x + pr.width - 6), (int)pr.y + 55, Color{120, 95, 60, 255});
        DrawText("Resource / Stock", (int)pr.x + 10, (int)pr.y + 58, 12, GOLD); DrawText("BUY (1 / 10 / 20 / 50 / 100)", (int)pr.x + 130, (int)pr.y + 58, 12, GOLD);
        DrawText("SELL (1 / 10 / 20 / 50 / 100)", (int)pr.x + 308, (int)pr.y + 58, 12, GOLD); DrawText("AUTO-BUY", (int)pr.x + 488, (int)pr.y + 58, 12, GOLD); DrawText("AUTO-SELL", (int)pr.x + 560, (int)pr.y + 58, 12, GOLD);
        for (int i = 0; i < NG; i++) {
            float y = pr.y + 76 + i * MROW;
            DrawRectangle((int)pr.x + 6, (int)y, (int)pr.width - 12, 27, i % 2 == 0 ? Color{48, 35, 22, 180} : Color{38, 28, 18, 180});
            DrawText(TextFormat("%s: %d", GN[i], *goodPtr(i)), (int)pr.x + 10, (int)y + 2, 13, RAYWHITE);
            DrawText(TextFormat("B:%dg  S:%dg", BUY[i], SELL[i]), (int)pr.x + 10, (int)y + 15, 10, Color{210, 190, 140, 255});
            for (int k = 0; k < 5; k++) {
                int q = T_QTYS[k];
                mbtn({pr.x + 130 + k * 35.f, y + 2, 33, 24}, TextFormat("+%d", q), gold >= BUY[i], false, 11);
                mbtn({pr.x + 308 + k * 35.f, y + 2, 33, 24}, TextFormat("-%d", q), *goodPtr(i) > 0, false, 11);
            }
            const char* abText = !autoBuyOn[i] ? "OFF" : TextFormat("Buy<%d", autoBuyLim[i]);
            mbtn({pr.x + 486, y + 2, 70, 24}, abText, true, autoTrade && autoBuyOn[i], 11);
            const char* asText = !autoSellOn[i] ? "OFF" : (autoSellLim[i] == 0 ? "Sell All" : TextFormat(autoSellUpTo ? "<=%d" : ">%d", autoSellLim[i]));
            mbtn({pr.x + 560, y + 2, 68, 24}, asText, true, autoTrade && autoSellOn[i], 11);
        }
        return;
    }
    int n; const int* r = rosterOf(B[openB].type, n);
    DrawText(TextFormat("%s - Recruit", INFO[B[openB].type].name), (int)pr.x + 10, (int)pr.y + 8, 16, GOLD);
    DrawText("x1", (int)pr.x + 236, (int)pr.y + 14, 10, LIGHTGRAY); DrawText("x5", (int)pr.x + 292, (int)pr.y + 14, 10, LIGHTGRAY); DrawText("ALL", (int)pr.x + 345, (int)pr.y + 14, 10, LIGHTGRAY);
    for (int i = 0; i < n; i++) {
        const TInfo& t = TR[r[i]]; float y = pr.y + 34 + i * 30;
        int st = t.res == 0 ? bows : t.res == 1 ? spears : t.res == 2 ? swords : 999999; bool can = st > 0 && gold >= t.gold && wood >= t.wood && pop > 0;
        if (t.res >= 0) DrawText(TextFormat("%s (stock %d, %dg)", t.name, st, t.gold), (int)pr.x + 10, (int)y + 5, 13, RAYWHITE);
        else DrawText(TextFormat("%s (%dg%s)", t.name, t.gold, t.wood ? TextFormat(" %dw", t.wood) : ""), (int)pr.x + 10, (int)y + 5, 13, RAYWHITE);
        mbtn({pr.x + 218, y, 52, 24}, "x1", can, false, 13); mbtn({pr.x + 274, y, 52, 24}, "x5", can, false, 13); mbtn({pr.x + 330, y, 52, 24}, "All", can, false, 13);
    }
}

// ---------------- HUD ----------------
const char* RN[4] = {"None", "Half", "Full", "Double"};
int hudTab = 0;
// Six classic RTS building pages. Every entry maps to an existing gameplay building type.
const int TABS[6][9] = {
    {HOUSE, INN, MARKET, CHURCH, -1, -1, -1, -1, -1},                         // Popular
    {GRANARY, APPLE, WHEAT, GRAPE, DATE, MILL, BAKERY, HUNTER, GARDEN},       // Food
    {WOOD, QUARRY, IRON, STORAGE, -1, -1, -1, -1, -1},                       // Resources
    {FLETCHER, POLE, SMITH, WINERY, CANDLE, -1, -1, -1, -1},                 // Industry
    {BARRACKS, TENT, SIEGE, ENGGUILD, HEALER, -1, -1, -1, -1},               // Military
    {WALL, TOWER, RTOWER, GATE, -1, -1, -1, -1, -1}                         // Castle
};
const char* TABN[6] = {"Popular", "Food", "Resources", "Industry", "Military", "Castle"};
const char* DESC[NB] = {"Your main castle.", "Produces wood.", "Produces stone.", "Produces iron.",
    "Grows wheat for the Grain Mill.", "Increases population capacity.", "Makes bows from wood.", "Makes spears from wood.",
    "Makes swords from iron.", "Recruits eight medieval infantry using weapon stocks.", "Arabic War Tent: recruits desert troops for gold only.", "A tower that fires arrows.", "A wall that blocks enemies.",
    "A gate for your troops.", "Needs candles. Raises popularity.", "Needs wine. Raises popularity.", "Buy and sell resources.", "Grinds grain into flour.", "A stronger tower with longer range.",
    "Stores resources.", "Stores food, +2 peasants.", "Grows apples (food).", "Grows grapes for the Winery.", "Grows dates (food).", "Bakes flour into bread.",
    "Presses grapes into wine for Inns.", "Makes candles from wood for Churches.", "Builds catapults and battering rams.", "Trains engineers who repair buildings.", "Heals your soldiers inside the green circle.", "A hunter shoots deer, rabbits and boars and brings meat.", "A peaceful garden that improves popularity."};
string costText(int t) {
    string s; const BInfo& in = INFO[t];
    if (in.wood) s += "W" + to_string(in.wood) + " "; if (in.stone) s += "S" + to_string(in.stone) + " "; if (in.gold) s += "G" + to_string(in.gold);
    if (!s.empty() && s.back() == ' ') s.pop_back(); return s.empty() ? "Free" : s;
}
bool affordable(int t) { return wood >= INFO[t].wood && stone >= INFO[t].stone && gold >= INFO[t].gold; }
Vector2 homeCam() { return iso((PKX + 1) * TS, (PKY + 1) * TS); }
int viewEnemy = 0;
Vector2 enemyCam() { if (EN.empty()) return homeCam(); viewEnemy = (viewEnemy + 1) % EN.size(); return iso((EN[viewEnemy].kx + 1) * TS, (EN[viewEnemy].ky + 1) * TS); }
const float MMS = 2.5f;

void hud(Vector2 m, bool click, int& build, Camera2D& cam) {
    int iy = MAPH; float y0 = (float)MAPH;
    DrawRectangleGradientV(0, iy, SW, HUD, Color{62, 45, 30, 255}, Color{30, 22, 14, 255}); DrawRectangle(0, iy, SW, 2, GOLD);
    for (int t = 0; t < 6; t++) if (btn({6.f + t * 72, y0 + 6, 70, 24}, TABN[t], hudTab == t, m, click, 11)) hudTab = t;
    const char* tip = "Pick a building, then click the map. Right-click cancels.";
    for (int i = 0; i < 9 && TABS[hudTab][i] >= 0; i++) {
        int bt = TABS[hudTab][i]; Rectangle r = {6.f + i * 62, y0 + 34, 60, 62}; bool aff = affordable(bt), h = CheckCollisionPointRec(m, r);
        DrawRectangleRec(r, build == bt ? Color{125, 100, 40, 255} : (h ? Color{100, 78, 46, 255} : Color{68, 52, 36, 255}));
        DrawRectangleLinesEx(r, build == bt ? 2.f : 1.f, build == bt ? GOLD : Color{150, 125, 90, 255});
        int id; float w, dy; Building fake = {bt, 0, 0, 1, 0, 0, false, false, false, 0};
        if (sprFor(fake, id, w, dy) && bt != KEEP) { float ih = 30, iw = ih * SP[id].t.width / SP[id].t.height; if (iw > 54) { iw = 54; ih = iw * SP[id].t.height / SP[id].t.width; } drawSprite(SP[id].t, r.x + 30, r.y + 34, iw, ih, false, aff ? WHITE : Color{150, 150, 150, 255}); }
        else { DrawRectangle((int)r.x + 14, (int)r.y + 14, 32, 18, INFO[bt].col); DrawRectangleLines((int)r.x + 14, (int)r.y + 14, 32, 18, BLACK); }
        const char* nm = INFO[bt].name; int fs = MeasureText(nm, 10) > 56 ? 8 : 10;
        DrawText(nm, (int)(r.x + 30 - MeasureText(nm, fs) / 2), (int)r.y + 36, fs, aff ? RAYWHITE : GRAY);
        string ct = costText(bt); DrawText(ct.c_str(), (int)(r.x + 30 - MeasureText(ct.c_str(), 9) / 2), (int)r.y + 49, 9, aff ? GOLD : RED);
        if (h) { tip = DESC[bt]; if (click) { build = (build == bt ? 0 : bt); openB = -1; } }
    }
    DrawText(tip, 8, iy + 100, 12, Color{235, 220, 175, 255});
    int cnt[NU] = {0}, sel = 0; for (auto& u : U) if (!u.enemy) { cnt[u.type]++; if (u.sel) sel++; }
    DrawText(TextFormat("Army: Ar %d Cb %d Sp %d Mc %d Sw %d Kn %d HA %d Eng %d Cat %d Ram %d | Selected %d", cnt[ARCHER], cnt[CROSSBOW], cnt[SPEAR], cnt[MACE], cnt[SWORD], cnt[KNIGHT], cnt[HORSE_ARCHER], cnt[ENGINEER], cnt[CATA], cnt[RAM], sel), 8, iy + 118, 12, RAYWHITE);
    DrawText("Train: Q archer Z crossbow W spear D mace E sword K knight  |  Shift = train ALL, Ctrl = train 5", 8, iy + 134, 11, Color{255, 225, 150, 255});
    DrawText("Arrows/edges scroll, wheel zoom, C home, V next enemy, A select all, O options, H help, F1 advisor", 8, iy + 148, 10, LIGHTGRAY);
    // economy panel
    Rectangle ep = {572.f, y0 + 4, 218, 152}; DrawRectangleRec(ep, Color{0, 0, 0, 70}); DrawRectangleLinesEx(ep, 1, Color{150, 125, 90, 255});
    DrawText("Tax", 582, iy + 10, 13, RAYWHITE);
    if (btn({650.f, y0 + 7, 24, 20}, "-", false, m, click)) tax = max(-2, tax - 1);
    DrawText(TextFormat("%+d", tax), 700, iy + 10, 13, GOLD);
    if (btn({748.f, y0 + 7, 24, 20}, "+", false, m, click)) tax = min(4, tax + 1);
    DrawText(TextFormat("(%+d gold every 5 s)", (int)(pop * tax * 0.5f)), 582, iy + 29, 11, LIGHTGRAY);
    DrawText("Rations", 582, iy + 45, 13, RAYWHITE);
    if (btn({650.f, y0 + 42, 24, 20}, "-", false, m, click)) ration = max(0, ration - 1);
    DrawText(RN[ration], 686, iy + 45, 13, GOLD);
    if (btn({748.f, y0 + 42, 24, 20}, "+", false, m, click)) ration = min(3, ration + 1);
    DrawText("Popularity", 582, iy + 68, 12, RAYWHITE); DrawRectangle(660, iy + 67, 120, 14, DARKGRAY);
    DrawRectangle(660, iy + 67, popularity * 120 / 100, 14, popularity >= 50 ? LIME : (popularity >= 30 ? YELLOW : RED)); DrawText(TextFormat("%d%%", popularity), 705, iy + 68, 11, BLACK);
    DrawText(TextFormat("Peasants %d / %d  (working %d, idle %d)", pop, cap, employed, max(0, pop - employed)), 582, iy + 86, 12, RAYWHITE);
    if (btn({580.f, y0 + 104, 64, 22}, paused ? "Resume" : "Pause(P)", paused, m, click, 11)) paused = !paused;
    if (btn({648.f, y0 + 104, 64, 22}, TextFormat("Speed x%d", speedMul), false, m, click, 11)) speedMul = speedMul % 3 + 1;
    if (btn({716.f, y0 + 104, 68, 22}, "Market(T)", isMarketOpen(), m, click, 11)) openB = isMarketOpen() ? -1 : -2;
    if (btn({580.f, y0 + 130, 96, 22}, autoTrade ? "Auto Trade:ON" : "Auto Trade:OFF", autoTrade, m, click, 11)) autoTrade = !autoTrade;
    DrawText(TextFormat("Raids %d  Enemy army %d", wave, totalEnemyArmy()), 682, iy + 135, 11, Color{255, 200, 150, 255});
    // minimap
    Rectangle mm = {806.f, y0 + 4, MW * MMS, MH * MMS}; DrawRectangle(796, (int)y0 + 2, 160, 90, BLACK);
    for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) DrawRectangleV({mm.x + x * MMS, mm.y + y * MMS}, {MMS + 0.3f, MMS + 0.3f}, ter[y][x] == 3 ? Color{40, 100, 150, 255} : ter[y][x] == 1 ? Color{70, 115, 50, 255} : Color{190, 160, 105, 255});
    for (auto& b : B) if (!b.dead) DrawRectangleV({mm.x + b.x * MMS, mm.y + b.y * MMS}, {sizeOf(b.type) * MMS, sizeOf(b.type) * MMS}, b.enemy ? Color{210, 40, 40, 255} : Color{60, 110, 235, 255});
    for (auto& u : U) DrawRectangle((int)(mm.x + u.p.x / TS * MMS) - 1, (int)(mm.y + u.p.y / TS * MMS) - 1, 2, 2, u.enemy ? RED : SKYBLUE);
    Vector2 cw = {cam.target.x / 2 + cam.target.y, cam.target.y - cam.target.x / 2};
    DrawRectangleLines((int)(mm.x + cw.x / TS * MMS - 11), (int)(mm.y + cw.y / TS * MMS - 7), 22, 14, RAYWHITE); DrawRectangleLinesEx({796, y0 + 2, 160, 90}, 1, GOLD);
    if (click && CheckCollisionPointRec(m, mm)) cam.target = iso((m.x - mm.x) / MMS * TS, (m.y - mm.y) / MMS * TS);
    for (size_t i = 0; i < EN.size(); i++) {   // one health bar per enemy lord
        float ry = y0 + 95 + i * 11.f; const Enemy& e = EN[i];
        DrawText(LORDS[e.lord].name, 798, (int)ry, 9, e.defeated ? GRAY : RAYWHITE);
        DrawRectangle(868, (int)ry + 1, 88, 8, DARKGRAY);
        if (!e.defeated && e.ek >= 0) DrawRectangle(868, (int)ry + 1, (int)(88 * max(0.f, B[e.ek].hp) / INFO[KEEP].hp), 8, Color{200, 50, 50, 255});
        else DrawText("DEFEATED", 884, (int)ry, 8, GRAY);
    }
    if (btn({798.f, y0 + 130, 54, 22}, "Opt(O)", false, m, click, 10)) optOpen = true;
    if (btn({856.f, y0 + 130, 52, 22}, "Scribe", false, m, click, 10)) advOpen = !advOpen;
    if (btn({912.f, y0 + 130, 44, 22}, "Help", false, m, click, 10)) helpOpen = !helpOpen;
}

void drawSelectionInfo(Vector2 m) {
    if (selectedBuilding < 0 || selectedBuilding >= (int)B.size() || B[selectedBuilding].dead) return;
    Building& b = B[selectedBuilding]; Rectangle r = {8, 34, 470, 92};
    DrawRectangleRec(r, Color{25, 18, 10, 225}); DrawRectangleLinesEx(r, 2, GOLD);
    DrawText(INFO[b.type].name, 18, 42, 18, GOLD);
    const char* st = "";
    if (b.prog < 1.f) st = TextFormat("Under construction %d%%", (int)(b.prog * 100));
    else if (b.type == HEALER) st = "Heals soldiers inside the green circle";
    else if (b.type == HUNTER) st = b.working ? "Hunting - brings meat to the keep" : "Waiting for a worker";
    else if (WORKS[b.type]) st = b.working ? "Working" : (employed < pop ? "Missing materials" : "Waiting for a worker");
    else if (b.type == INN) st = b.fed ? "Serving wine - popularity up" : "No wine! Build a Winery";
    else if (b.type == CHURCH) st = b.fed ? "Candles lit - popularity up" : "No candles! Build a Candle Maker";
    else if (b.type == ENGGUILD) st = "Recruit engineers - they repair buildings";
    DrawText(st, 18, 66, 14, RAYWHITE);
    DrawText(DESC[b.type], 18, 86, 11, Color{220, 205, 165, 255});
    DrawText(TextFormat("Cost: %d wood  %d stone  %d gold   Build: %.1fs", INFO[b.type].wood, INFO[b.type].stone, INFO[b.type].gold, BUILD_TIME[b.type]), 18, 103, 10, GOLD);
    DrawText(TextFormat("HP: %.0f / %d", b.hp, INFO[b.type].hp), 18, 116, 11, LIGHTGRAY);
}

void topBar() {
    DrawRectangle(0, 0, SW, 26, Color{25, 18, 10, 220}); DrawRectangle(0, 26, SW, 1, GOLD);
    struct R { const char* n; int v; Color c; };
    R rs[13] = {{"Wood", wood, BROWN}, {"Stone", stone, GRAY}, {"Iron", iron, {130, 110, 150, 255}}, {"Gold", gold, GOLD}, {"Bread", bread, {230, 190, 120, 255}}, {"Apples", apples, {210, 60, 50, 255}},
                {"Dates", dates, {190, 110, 40, 255}}, {"Meat", meat, {205, 95, 75, 255}}, {"Wine", wine, {140, 50, 120, 255}}, {"Candles", candles, {250, 235, 170, 255}}, {"Bows", bows, {200, 170, 110, 255}},
                {"Spears", spears, {110, 160, 210, 255}}, {"Swords", swords, LIGHTGRAY}};
    int x = 6;
    for (auto& r : rs) { DrawRectangle(x, 8, 9, 9, r.c); DrawRectangleLines(x, 8, 9, 9, BLACK); const char* t = TextFormat("%s %d", r.n, r.v); DrawText(t, x + 12, 6, 13, RAYWHITE); x += 12 + MeasureText(t, 13) + 6; }
}

void drawAdvisor(Vector2 m, bool click) {
    Rectangle pr = {120, 40, 720, 440};
    DrawRectangle(0, 0, SW, SH, Color{0, 0, 0, 140});
    if (SP[S_BOOK].ok) DrawTexturePro(SP[S_BOOK].t, Rectangle{0, 0, (float)SP[S_BOOK].t.width, (float)SP[S_BOOK].t.height}, Rectangle{pr.x - 40, pr.y - 20, 800, 480}, Vector2{0, 0}, 0, WHITE);
    DrawRectangleRec(pr, Color{225, 205, 160, 185}); DrawRectangleLinesEx(pr, 4, Color{90, 60, 30, 255});
    Color ink = {50, 30, 15, 255};
    DrawText("Scribe's Report", (int)pr.x + 220, (int)pr.y + 14, 30, ink);
    if (SP[S_ADVISOR].ok) drawSprite(SP[S_ADVISOR].t, pr.x + 100, pr.y + 330, 0, 290);
    string line = popularity < 30 ? "The people are leaving the castle, my lord!" : gold < 20 ? "Your treasury is nearly empty, sire!" : popularity >= 75 ? "The people sing your praises!" : "All is well, my lord.";
    DrawText(line.c_str(), (int)pr.x + 20, (int)pr.y + 345, 12, ink);
    DrawText(TextFormat("Popularity %d%%", popularity), (int)pr.x + 20, (int)pr.y + 372, 18, ink);
    float x = pr.x + 220, y = pr.y + 60;
    DrawText("Treasury", (int)x, (int)y, 20, ink); DrawText(TextFormat("%d Gold   (%+d every 5 s)", gold, (int)(pop * tax * 0.5f)), (int)x + 120, (int)y + 2, 16, ink); y += 34;
    DrawText(hasGranary() ? "Granary - food storage" : "GRANARY MISSING - people cannot eat!", (int)x, (int)y, 20, hasGranary() ? ink : RED); y += 26;
    struct Row { const char* n; int v; Color c; }; int animalsAlive = 0; for (auto& an : AN) if (an.state == 0) animalsAlive++;
    Row rows[] = {{"Bread", bread, {60, 140, 50, 255}}, {"Meat", meat, {205, 95, 75, 255}}, {"Apples", apples, {200, 60, 50, 255}}, {"Dates", dates, {200, 120, 40, 255}}, {"Wine", wine, {140, 50, 120, 255}}, {"Candles", candles, {220, 190, 60, 255}},
                                                                 {"Grain", grain, {190, 160, 60, 255}}, {"Flour", flour, {200, 190, 170, 255}}, {"Grapes", grapes, {100, 60, 150, 255}}, {"Game animals", animalsAlive, {120, 150, 80, 255}}};
    for (int i = 0; i < 10; i++) { float cx = x + (i % 2) * 250, cy = y + (i / 2) * 26; DrawText(TextFormat("%s: %d", rows[i].n, rows[i].v), (int)cx, (int)cy, 15, ink); DrawRectangle((int)cx + 100, (int)cy + 3, 110, 12, Color{150, 130, 100, 255}); DrawRectangle((int)cx + 100, (int)cy + 3, min(110, rows[i].v), 12, rows[i].c); }
    y += 5 * 26 + 14;
    int troops = 0; for (auto& u : U) if (!u.enemy) troops++;
    DrawText("Castle Garrison", (int)x, (int)y, 20, ink); DrawText(TextFormat("%d Troops   Kills %d   Raids %d", troops, kills, wave), (int)x + 190, (int)y + 2, 16, ink); y += 34;
    DrawText(TextFormat("Inns served %d   Churches served %d   Peasants %d/%d", innServed, churchServed, pop, cap), (int)x, (int)y, 14, ink);
    if (btn({pr.x + pr.width - 120, pr.y + pr.height - 40, 100, 30}, "Close", false, m, click, 16)) advOpen = false;
}

void drawShadows() {
    for (auto& b : B) if (!b.dead) {
        Vector2 c = iso(center(b).x, center(b).y); if (!inView(c, 100)) continue;
        float s = sizeOf(b.type) * TS, x0 = b.x * TS, y0 = b.y * TS, h = BH[b.type] * 0.55f; Vector2 off = {h * 0.9f, h * 0.35f};
        vector<Vector2> pts = {iso(x0, y0), iso(x0 + s, y0), iso(x0 + s, y0 + s), iso(x0, y0 + s)};
        for (int i = 0; i < 4; i++) pts.push_back({pts[i].x + off.x, pts[i].y + off.y});
        hullShadow(pts, Color{0, 0, 0, 55});
    }
    for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) if (occ[y][x] == -2) { Vector2 p = iso((x + 0.5f) * TS, (y + 0.5f) * TS); if (!inView(p, 40)) continue; DrawEllipse((int)p.x + 9, (int)p.y + 1, 13, 5, Color{0, 0, 0, 50}); }
}

void presentFrame() {   // fill the complete display; the render texture is stretched to the current output size
    BeginDrawing(); ClearBackground(BLACK);
    DrawTexturePro(screenRT.texture, Rectangle{0, 0, (float)SW, -(float)SH}, Rectangle{0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()}, Vector2{0, 0}, 0, WHITE);
    EndDrawing();
}

// ---------------- text helpers ----------------
void drawLines(const char* text, float x, float y, int fs, Color c, int gap = 4) {   // text with \n line breaks
    string s = text; size_t p = 0;
    while (p <= s.size()) { size_t q = s.find('\n', p); if (q == string::npos) q = s.size(); DrawText(s.substr(p, q - p).c_str(), (int)x, (int)y, fs, c); y += fs + gap; p = q + 1; }
}
float drawWrapped(const string& s, float x, float y, float w, int fs, Color c) {   // word-wrapped paragraph, returns the y below it
    string line, word; size_t i = 0;
    while (i <= s.size()) {
        if (i == s.size() || s[i] == ' ') {
            string t = line.empty() ? word : line + " " + word;
            if (MeasureText(t.c_str(), fs) > w && !line.empty()) { DrawText(line.c_str(), (int)x, (int)y, fs, c); y += fs + 4; line = word; } else line = t;
            word.clear();
        } else word += s[i];
        i++;
    }
    if (!line.empty()) { DrawText(line.c_str(), (int)x, (int)y, fs, c); y += fs + 4; }
    return y;
}
void lordFace(int l, Rectangle r) {
    if (l < 3 && lordsOk) DrawTexturePro(LORD_FACE[l], Rectangle{0, 0, (float)LORD_FACE[l].width, (float)LORD_FACE[l].height}, r, Vector2{0, 0}, 0, WHITE);
    else if (l == 11 || l == 12) {
        Texture2D t;
        if (l == 11) { int phase = (int)gtime % 8; t = phase >= 6 ? ARAB_LORD_ATTACK[(int)(gtime * 4.f) % 4] : ARAB_LORD_WALK[(int)(gtime * 7.f) % 7]; }
        else { int phase = (int)gtime % 7; t = phase >= 5 ? ENGLISH_LORD_ATTACK[(int)(gtime * 5.f) % 7] : ENGLISH_LORD_WALK[(int)(gtime * 6.f) % 6]; }
        if (t.id) DrawTexturePro(t, Rectangle{0, 0, (float)t.width, (float)t.height}, r, Vector2{0, 0}, 0, WHITE);
    } else {
        DrawRectangleRec(r, LORDS[l].tint); DrawRectangleRec({r.x + r.width * 0.2f, r.y + r.height * 0.55f, r.width * 0.6f, r.height * 0.45f}, LORDS[l].stoneCol);
        DrawCircle((int)(r.x + r.width / 2), (int)(r.y + r.height * 0.36f), r.width * 0.2f, Color{70, 50, 35, 255});
    }
    DrawRectangleLinesEx(r, 2, GOLD);
}

// ---------------- help pages ----------------
const int NHELP = 6;
const char* HELP_T[NHELP] = {"Basics", "Economy", "Food and Faith", "Army", "Castle and Enemy", "Keys"};
const char* HELP_TXT[NHELP] = {
"GOAL\nDestroy the keep of every enemy lord. If your own keep falls, you lose.\n\nCAMERA\nArrow keys or move the mouse to the screen edge to scroll.\nMouse wheel zooms. C = back to your keep, V = look at the next enemy castle.\n\nSOLDIERS\nDrag a box (or click) to select soldiers. A = select all.\nRight-click the ground to move. Right-click an enemy building to attack it.\n\nBUILDING\nPick a tab at the bottom (Farms, Industry, Town, Military, Defense), click a building,\nthen click the map. Green = free, red = blocked. Right-click or Esc cancels.\n\nOTHER\nP = pause, Tab = game speed, O = options, F1 = advisor report, F11 = full screen.",
"PEASANTS\nPeasants come to your keep when people like you (popularity 50+). Houses give room for 8 more.\nEvery farm, mine and workshop needs one free peasant to work (green dot = working).\n\nGOLD AND TAX\nTax (+/- keys or the HUD) gives gold every 5 seconds but lowers popularity.\nGold pays for soldiers and some buildings (Inn, Church, Winery, siege...).\n\nRESOURCES\nWoodcutter = wood, Quarry = stone, Iron Mine = iron.\nFletcher turns wood into bows, Poleturner into spears, Blacksmith turns iron into swords.\n\nMARKET (T)\nBuy and sell goods. Auto-Trade can keep your stocks topped up for you.\n\nPOPULARITY\nGood food, a happy Inn and a lit Church raise it. Hunger and high tax lower it.\nBelow 30% peasants leave - above 75% they arrive quickly.",
"FOOD CHAINS (you need food or your people starve!)\nWheat Farm -> Grain Mill (grain to flour) -> Bakery (flour to bread).\nApple Orchard -> apples.   Date Farm -> dates.\nEating several kinds of food gives a small popularity bonus. Use [ ] to change rations.\n\nINN\nGrape Farm -> Winery (grapes to wine) -> Inn.\nEach Inn drinks 1 wine per 5 seconds and then raises popularity (+6).\nNo wine = no bonus (the dot above the Inn turns red).\n\nCHURCH\nCandle Maker (burns wood) -> candles -> Church.\nEach Church burns 1 candle per 5 seconds and raises popularity (+7).\n\nHUNTER'S HUT: a hunter shoots deer, rabbits and boars and butchers them into meat (food).\nThe Advisor (F1) shows all your stocks, popularity and army at a glance.",
"BARRACKS: 8 medieval troops. Weapon stock is required; prices are cheaper.\nWAR TENT: 8 Arabic-only troops. Recruit with gold only, no weapon stock.\nCamel Archer costs 100 gold and is stronger than a Horse Archer.\nSIEGE WORKSHOP: Catapult, Battering Ram and Demolisher.\nENGINEER GUILD: Engineers repair damaged buildings.\nHEALER: heals soldiers standing inside its green circle.\n\nTRAINING\nClick a Barracks or War Tent to open its recruit menu. Use x1, x5 or All.\nEach soldier needs a free peasant. Building prices are capped at 20 wood and 50 stone.\n\nTIP: archers behind walls and towers are very strong. Engineers keep the walls alive.",
"YOUR CASTLE\nWalls block enemies. Gates let your own soldiers through. Towers and Round Towers shoot arrows.\nEngineers (Engineer Guild) repair walls and towers between attacks.\n\nTHE ENEMY LORDS\nEach lord starts with only a keep and BUILDS his own castle - every lord builds a different one.\nHe earns his own gold, trains soldiers in his barracks and tents, and gathers them in front of\nhis gate. When his army is big enough he sends it to attack you (a horn warns you!).\nIf his army or castle is destroyed he rebuilds, so hit his keep before he gets too strong.\n\nHARD MISSIONS\nLater missions have 2 or 3 enemy lords at once; the final mission has ELEVEN. Defeating a lord destroys his castle.\nPress V to look at each castle. Attack the weakest first, and keep a strong defence at home.",
"MOUSE\nLeft click: select / place building / press buttons    Right click: move or attack\nDrag: box select\n\nKEYBOARD\nArrows: scroll   Wheel: zoom   C: home   V: next enemy castle\nA: select all   Q W E D Z K: train (Shift = all, Ctrl = 5)\nT: market   P: pause   Tab: speed   - and =: tax   [ and ]: rations\nO: options   F1: advisor   H: this help   F11: full screen   Esc: close / cancel\nF9: cheat menu (only when Options > Cheats is ON)"};
int helpPage = 0;
void drawHelp(Vector2 m, bool click, bool fromMenu) {
    Rectangle pr = {40, 24, 880, 640};
    DrawRectangle(0, 0, SW, SH, Color{0, 0, 0, 150}); DrawRectangleRec(pr, Color{225, 205, 160, 250}); DrawRectangleLinesEx(pr, 4, Color{90, 60, 30, 255});
    Color ink = {50, 30, 15, 255};
    DrawText("HOW TO PLAY", (int)pr.x + 24, (int)pr.y + 14, 30, ink);
    for (int i = 0; i < NHELP; i++) if (btn({pr.x + 24 + i * 140.f, pr.y + 56, 134, 28}, HELP_T[i], helpPage == i, m, click, 14)) helpPage = i;
    DrawLine((int)pr.x + 20, (int)pr.y + 94, (int)(pr.x + pr.width - 20), (int)pr.y + 94, ink);
    drawLines(HELP_TXT[helpPage], pr.x + 28, pr.y + 106, 15, ink, 5);
    if (btn({pr.x + 24, pr.y + pr.height - 46, 100, 32}, "< Prev", false, m, click, 15)) helpPage = (helpPage + NHELP - 1) % NHELP;
    if (btn({pr.x + 134, pr.y + pr.height - 46, 100, 32}, "Next >", false, m, click, 15)) helpPage = (helpPage + 1) % NHELP;
    DrawText("Tip: the Tutorial mission teaches all of this step by step.", (int)pr.x + 260, (int)pr.y + pr.height - 38, 14, ink);
    if (btn({pr.x + pr.width - 130, pr.y + pr.height - 46, 110, 32}, fromMenu ? "Back" : "Close", false, m, click, 16)) helpOpen = false;
}

// ---------------- campaign progress ----------------
void saveCampaign() { FILE* f = fopen("campaign.cfg", "w"); if (f) { fprintf(f, "%d %d\n", campDone, tutDone ? 1 : 0); fclose(f); } }
void loadCampaign() { FILE* f = fopen("campaign.cfg", "r"); if (!f) return; int a = 0, b = 0; if (fscanf(f, "%d %d", &a, &b) == 2) { campDone = max(0, min(10, a)); tutDone = b != 0; } fclose(f); }

// ---------------- tutorial mission ----------------
const int NTUT = 15;
const char* TUT[NTUT] = {
"Welcome, my lord! First learn to look around.\nScroll with the arrow keys (or the screen edge) and zoom with the mouse wheel.",
"Wood first. Open the INDUSTRY tab at the bottom,\nclick Woodcutter, then click the map near your keep.",
"Now build a QUARRY (Industry tab) to get stone.",
"Peasants need homes. Open the TOWN tab and build a HOUSE.",
"Time for food. Open the FARMS tab and build a WHEAT FARM.",
"Build a GRAIN MILL (Farms tab) - it grinds the wheat into flour.",
"Build a BAKERY (Farms tab) - it bakes the flour into bread.",
"Build an APPLE ORCHARD (Farms tab). More kinds of food = happier people.",
"Press F1 (or the Adv button) to read the Advisor's report. Press F1 again to close it.",
"Now the army. Open the MILITARY tab and build a BARRACKS.",
"Click your Barracks to open its recruit menu and train 5 soldiers:\nclick the x5 button next to Archer (or press Ctrl+Q). Shift-click trains ALL.",
"Press A to select all your soldiers, then RIGHT-click on the ground to move them.",
"Open the DEFENSE tab and build a TOWER or a WALL near your keep.",
"Press T to open the MARKET panel. You can buy and sell goods there.",
"FINAL: destroy the Old Steward's keep! Press V to look at his castle,\nselect your soldiers (A) and right-click his buildings to attack them."};
int tutStep = 0; bool tutMoved = false, tutMarket = false, tutAdv = false; Vector2 tutCam0 = {0, 0};
int myCount(int type) { int n = 0; for (auto& b : B) if (!b.enemy && !b.dead && b.type == type) n++; return n; }
bool tutCheck(int step, const Camera2D& cam) {
    switch (step) {
        case 0: return dist(cam.target, tutCam0) > 150 || fabsf(cam.zoom - 0.9f) > 0.15f;
        case 1: return myCount(WOOD) >= 1; case 2: return myCount(QUARRY) >= 1; case 3: return myCount(HOUSE) >= 1; case 4: return myCount(WHEAT) >= 1;
        case 5: return myCount(MILL) >= 1; case 6: return myCount(BAKERY) >= 1; case 7: return myCount(APPLE) >= 1; case 8: return tutAdv;
        case 9: return myCount(BARRACKS) >= 1;
        case 10: { int n = 0; for (auto& u : U) if (!u.enemy && u.type != ENGINEER) n++; return n >= 5; }
        case 11: return tutMoved; case 12: return myCount(TOWER) + myCount(RTOWER) + myCount(WALL) + myCount(GATE) >= 1; case 13: return tutMarket;
    }
    return false;
}

void startMission(int mi, Camera2D& cam) {
    curMission = mi;
    if (mi >= 0) { cur = MISSIONS[mi]; mapStyle = cur.map; } else cur = makeCustom();
    if (cur.n > 1) cur.trainInt *= 1 + 0.4f * (cur.n - 1);      // with several lords each one trains a bit slower
    reset(); inMenu = false; openB = -1; optOpen = advOpen = helpOpen = false; cam.target = homeCam(); cam.zoom = 0.9f; viewEnemy = 0;
    tutStep = 0; tutMoved = tutMarket = tutAdv = false; tutCam0 = cam.target;
}

// ---------------- on-map enemy message (animated portrait "clip" + voice + typed text) ----------------
void drawEnemyMsg() {
    if (!msgActive) return;
    float t = curMsg.t, fade = max(0.f, min(1.f, min(t / 0.25f, (curMsg.dur - t) / 0.4f))); unsigned char A = (unsigned char)(255 * fade);
    float slide = (1 - min(1.f, t / 0.25f)) * 50;
    Rectangle mr = {SW - 392.f + slide, MAPH - 114.f, 384, 106};
    Color border = curMsg.kind == 1 ? Color{235, 75, 55, A} : (curMsg.kind == 3 ? Color{190, 190, 200, A} : Color{225, 180, 60, A});
    DrawRectangleRec(mr, Color{30, 20, 12, (unsigned char)(232 * fade)}); DrawRectangleLinesEx(mr, 2, border);
    Rectangle pr = {mr.x + 10, mr.y + 9, 84, 88}; bool talking = msgShown < (int)curMsg.text.size();
    DrawRectangleRec({pr.x - 3, pr.y - 3, pr.width + 6, pr.height + 6}, Color{12, 8, 5, A});
    float bob = talking ? sinf(gtime * 16) * 1.6f : sinf(gtime * 2) * 0.8f;
    lordFace(curMsg.lord, {pr.x, pr.y + bob, pr.width, pr.height});          // portrait (the Arabic / English lords play their animation)
    for (int y = 0; y < (int)pr.height; y += 3) DrawRectangle((int)pr.x, (int)pr.y + y, (int)pr.width, 1, Color{0, 0, 0, 38});   // old film scanlines
    DrawCircle((int)pr.x + 9, (int)pr.y + 9, 3, fmodf(gtime, 1.f) < 0.5f ? Color{255, 50, 40, A} : Color{110, 20, 20, A});
    if (talking) for (int k = 0; k < 3; k++) { unsigned char aa = (unsigned char)(A * (1.f - fmodf(gtime * 2.f + k * 0.33f, 1.f))); DrawCircleLines((int)pr.x + (int)pr.width + 6, (int)pr.y + 44, 4.f + k * 5.f, Color{255, 220, 120, aa}); }
    DrawText(TextFormat("Message from %s", LORDS[curMsg.lord].name), (int)mr.x + 104, (int)mr.y + 8, 14, Color{255, 215, 110, A});
    drawWrapped(curMsg.text.substr(0, msgShown), mr.x + 104, mr.y + 30, 270, 15, Color{245, 238, 220, A});
}

// ---------------- Victory! logo ----------------
void tri2(Vector2 a, Vector2 b, Vector2 c, Color col) { DrawTriangle(a, b, c, col); DrawTriangle(a, c, b, col); }
void drawStar(Vector2 c, float R, float r, float rot, Color col) {
    for (int i = 0; i < 5; i++) {
        float a0 = rot + i * 1.2566f; Vector2 p0 = {c.x + cosf(a0) * R, c.y + sinf(a0) * R}, p1 = {c.x + cosf(a0 + 0.6283f) * r, c.y + sinf(a0 + 0.6283f) * r}, p2 = {c.x + cosf(a0 + 1.2566f) * R, c.y + sinf(a0 + 1.2566f) * R}, pm = {c.x + cosf(a0 - 0.6283f) * r, c.y + sinf(a0 - 0.6283f) * r};
        tri2(c, p0, p1, col); tri2(c, pm, p0, col); (void)p2;
    }
}
float hrand(int i, int k) { return fabsf(fmodf(sinf(i * 12.9898f + k * 78.233f) * 43758.5453f, 1.f)); }
void drawVictoryLogo(float t, bool tutorial) {
    float a = min(1.f, t / 0.5f), s = a < 1 ? 1 + 2.70158f * powf(a - 1, 3) + 1.70158f * powf(a - 1, 2) : 1 + 0.015f * sinf(t * 3); float k = min(1.f, t * 2);
    DrawRectangle(0, 0, SW, MAPH, Color{0, 0, 0, (unsigned char)(150 * k)});
    Vector2 c = {SW / 2.f, 168};
    for (int i = 0; i < 18; i++) { float ang = i * 0.349f + t * 0.25f, w = 0.075f; tri2(c, {c.x + cosf(ang - w) * 340 * s, c.y + sinf(ang - w) * 340 * s}, {c.x + cosf(ang + w) * 340 * s, c.y + sinf(ang + w) * 340 * s}, Color{255, 215, 90, (unsigned char)(34 * k)}); }
    for (int sg = -1; sg <= 1; sg += 2) {   // crossed swords
        Vector2 top = {c.x - sg * 112 * s, c.y - 78 * s}, bot = {c.x + sg * 112 * s, c.y + 86 * s};
        DrawLineEx(top, bot, 11 * s, Color{196, 202, 214, 255}); DrawLineEx(top, bot, 3 * s, Color{240, 244, 252, 255});
        Vector2 g0 = {c.x + sg * 74 * s, c.y + 52 * s}; DrawLineEx({g0.x - sg * 14 * s, g0.y - 18 * s}, {g0.x + sg * 14 * s, g0.y + 18 * s}, 6 * s, Color{210, 160, 50, 255}); DrawCircleV(bot, 7 * s, Color{210, 160, 50, 255});
    }
    for (int sd = -1; sd <= 1; sd += 2) for (int i = 0; i < 12; i++) {   // laurel wreath
        float th = 0.55f + i * 0.2f; Vector2 lp = {c.x + sd * sinf(th) * 94 * s, c.y + 6 * s - cosf(th) * 94 * s};
        DrawEllipse((int)lp.x, (int)lp.y, 11 * s, 5 * s, (i % 2) ? Color{70, 150, 60, 255} : Color{104, 182, 74, 255}); if (i % 4 == 2) DrawCircleV({lp.x + sd * 4 * s, lp.y - 5 * s}, 3 * s, Color{200, 40, 40, 255});
    }
    DrawCircleV(c, 78 * s, Color{110, 72, 18, 255}); DrawCircleV(c, 72 * s, Color{236, 192, 62, 255}); DrawCircleV(c, 60 * s, Color{252, 226, 116, 255}); DrawCircleV(c, 54 * s, Color{190, 134, 28, 255});
    drawStar(c, 44 * s, 18 * s, -1.5708f, Color{255, 246, 176, 255}); drawStar(c, 33 * s, 13 * s, -1.5708f, Color{255, 204, 66, 255});
    float rw = 372 * s, rh = 60 * s; Rectangle rb = {c.x - rw / 2, c.y + 80 * s, rw, rh};
    DrawRectangle((int)(rb.x - 36 * s), (int)(rb.y + 12 * s), (int)(44 * s), (int)rh, Color{116, 18, 18, 255}); DrawRectangle((int)(rb.x + rb.width - 8 * s), (int)(rb.y + 12 * s), (int)(44 * s), (int)rh, Color{116, 18, 18, 255});
    DrawRectangleRec(rb, Color{176, 28, 28, 255}); DrawRectangleRec({rb.x, rb.y, rb.width, 6 * s}, Color{214, 66, 54, 255}); DrawRectangleLinesEx(rb, 3 * s, GOLD);
    const char* txt = "VICTORY!"; int fs = (int)(48 * s), tw = MeasureText(txt, fs);
    DrawText(txt, (int)(c.x - tw / 2.f) + 3, (int)(rb.y + rh / 2 - fs / 2.f) + 3, fs, Color{60, 8, 8, 255}); DrawText(txt, (int)(c.x - tw / 2.f), (int)(rb.y + rh / 2 - fs / 2.f), fs, Color{255, 226, 110, 255});
    const char* sub = tutorial ? "Tutorial complete - well done, my lord!" : "All enemy lords have been defeated!"; DrawText(sub, (int)(c.x - MeasureText(sub, 20) / 2.f), (int)(rb.y + rh + 18 * s), 20, Color{255, 236, 170, 255});
    for (int i = 0; i < 70 && t > 0.3f; i++) {   // confetti
        float sp = 40 + hrand(i, 2) * 90, x = hrand(i, 1) * SW + sinf(t * 2 + i) * 14, y = fmodf(hrand(i, 3) * MAPH + (t - 0.3f) * sp, (float)MAPH); Color cc[5] = {{255, 215, 80, 255}, {230, 70, 60, 255}, {80, 170, 240, 255}, {110, 210, 110, 255}, {240, 240, 240, 255}};
        DrawRectangle((int)x, (int)y, 5, 3 + (i % 3), cc[i % 5]);
    }
    for (int i = 0; i < 22; i++) { float ph = fmodf(t * 1.3f + hrand(i, 5), 1.f), sz = sinf(ph * 3.1416f) * 7; Vector2 sp = {c.x + (hrand(i, 6) - 0.5f) * 520, c.y + (hrand(i, 7) - 0.5f) * 250}; DrawLineEx({sp.x - sz, sp.y}, {sp.x + sz, sp.y}, 2, Color{255, 246, 190, 220}); DrawLineEx({sp.x, sp.y - sz}, {sp.x, sp.y + sz}, 2, Color{255, 246, 190, 220}); }
}

// ---------------- cheat menu (F9, only when Options > Cheats is ON) ----------------
void drawCheatMenu(Vector2 m, bool click) {
    Rectangle pr = {150, 50, 660, 470};
    DrawRectangle(0, 0, SW, SH, Color{0, 0, 0, 150}); DrawRectangleRec(pr, Color{35, 25, 15, 248}); DrawRectangleLinesEx(pr, 3, GOLD);
    DrawText("CHEAT MENU", (int)pr.x + 20, (int)pr.y + 14, 26, GOLD);
    DrawText("Using a cheat means this game will not unlock the next mission.", (int)pr.x + 20, (int)pr.y + 48, 14, Color{255, 200, 140, 255});
    const char* lab[12] = {"+2000 Gold", "+500 Wood, Stone, Iron", "+Food and goods", "+50 Bows, Spears, Swords", "+30 Peasants", "Popularity 100%", "Heal everything",
                           cheatInstant ? "Instant build: ON" : "Instant build: OFF", cheatGod ? "Invincible army: ON" : "Invincible army: OFF", "Kill all enemy soldiers", "Spawn 10 Knights", "WIN the mission"};
    for (int i = 0; i < 12; i++) {
        Rectangle r = {pr.x + 24 + (i % 2) * 310.f, pr.y + 84 + (i / 2) * 54.f, 296, 42};
        if (!btn(r, lab[i], (i == 7 && cheatInstant) || (i == 8 && cheatGod), m, click, 17)) continue;
        cheatUsed = true; sfx(SFX_COIN, 0.5f, 0.05f);
        switch (i) {
            case 0: gold += 2000; break; case 1: wood += 500; stone += 500; iron += 500; break;
            case 2: bread += 200; meat += 100; apples += 100; dates += 100; wine += 50; candles += 50; break;
            case 3: bows += 50; spears += 50; swords += 50; break;
            case 4: pop += 30; cheatCap += 30; break; case 5: popularity = 100; break;
            case 6: for (auto& b : B) if (!b.dead && !b.enemy) b.hp = (float)INFO[b.type].hp; for (auto& u : U) if (!u.enemy) u.hp = UINFO[u.type].hp; break;
            case 7: cheatInstant = !cheatInstant; if (cheatInstant) for (auto& b : B) if (!b.enemy) b.prog = 1.f; break;
            case 8: cheatGod = !cheatGod; break;
            case 9: for (auto& u : U) if (u.enemy) u.hp = 0; break;
            case 10: { Vector2 kp = center(B[0]); for (int j = 0; j < 10; j++) { Vector2 sp = freeNear({kp.x + 40 + (j % 5) * 14.f, kp.y + 60 + (j / 5) * 14.f}); U.push_back({sp, {0, 0}, KNIGHT, false, false, false, UINFO[KNIGHT].hp, 0, 0}); } break; }
            case 11: for (auto& e : EN) if (!e.defeated && e.ek >= 0) killB(e.ek); break;
        }
    }
    if (btn({pr.x + pr.width - 150, pr.y + pr.height - 52, 130, 36}, "Close (F9)", false, m, click, 17)) cheatOpen = false;
}

int main() {
    loadSettings();
    // Always launch in full-screen mode. The player can switch to Windowed in Options for the current session.
    cfg.fullscreen = true;
    loadCampaign();
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | (cfg.vsync ? FLAG_VSYNC_HINT : 0));
    InitWindow(WIN_W[cfg.winScale], WIN_H[cfg.winScale], "Mini Stronghold - English Edition"); SetWindowMinSize(480, 344); SetWindowPosition(20, 30); SetExitKey(KEY_NULL);
    initAudio(); loadAssets();
    screenRT = LoadRenderTexture(SW, SH); SetTextureFilter(screenRT.texture, TEXTURE_FILTER_BILINEAR);
    applyDisplay();
    cur = MISSIONS[4]; reset();
    Camera2D cam = {{SW / 2.f, MAPH / 2.f}, homeCam(), 0, 0.9f};
    int build = 0; Vector2 ds = {0, 0}; bool drag = false; int screen = 0, selMission = 1; bool quitGame = false, winRecorded = false;
    while (!WindowShouldClose() && !quitGame) {
        float dt = min(GetFrameTime(), 0.1f);
        { float sx = GetScreenWidth() / (float)SW, sy = GetScreenHeight() / (float)SH; SetMouseOffset(0, 0); SetMouseScale(1 / sx, 1 / sy); }
        Vector2 m = GetMousePosition(); bool inMap = m.y < MAPH && m.y >= 0, click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON), down = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
        if (IsKeyPressed(KEY_F11)) { cfg.fullscreen = !cfg.fullscreen; applyDisplay(); saveSettings(); }
        musicTick(dt, inMenu ? 0 : (battleNear && !gameOver && !won ? 2 : 1));
        if (quitMenu) { quitMenu = false; inMenu = true; screen = 0; openB = -1; }

        if (inMenu) {  // ---------------- menus: main, campaign, mission briefing, custom game, help ----------------
            gtime += dt; Color ink = {240, 225, 190, 255};
            if (IsKeyPressed(KEY_ESCAPE)) { if (optOpen) optOpen = false; else if (screen == 4) screen = 0; else if (screen != 0) screen = (screen == 2 ? 1 : 0); }
            bool mc = click && !optOpen && !(screen == 4);
            BeginTextureMode(screenRT);
            if (screen == 0) {
                MainMenu::Action action = MainMenu::DrawMain(MENU_ART.ok ? MENU_ART.t : SP[S_MAPBG].t, MENU_ART.ok || SP[S_MAPBG].ok, m, mc, gtime);
                if (action != MainMenu::Action::None) sfx(SFX_CLICK, 0.65f);
                if (action == MainMenu::Action::NewGame) screen = 5;
                else if (action == MainMenu::Action::Campaign) screen = 1;
                else if (action == MainMenu::Action::CustomBattle) { if (lordStyle < 13 || lordStyle >= NLORD) lordStyle = 13; screen = 3; }
                else if (action == MainMenu::Action::Tutorial) { selMission = 0; screen = 2; }
                else if (action == MainMenu::Action::Options) optOpen = true;
                else if (action == MainMenu::Action::Help) { screen = 4; helpOpen = true; }
                else if (action == MainMenu::Action::Credits) screen = 6;
                else if (action == MainMenu::Action::Exit) quitGame = true;
            } else {
                if (SP[S_MAPBG].ok) {
                    DrawTexturePro(SP[S_MAPBG].t, Rectangle{0, 0, (float)SP[S_MAPBG].t.width, (float)SP[S_MAPBG].t.height}, Rectangle{0, 0, (float)SW, (float)SH}, Vector2{0, 0}, 0, WHITE);
                    DrawRectangle(0, 0, SW, SH, Color{16, 10, 5, 170});
                } else DrawRectangleGradientV(0, 0, SW, SH, Color{70, 52, 34, 255}, Color{30, 22, 14, 255});
                const char* title = "KINGS OF MESOPOTAMIA";
                DrawText(title, SW / 2 - MeasureText(title, 34) / 2 + 2, 16, 34, Color{0, 0, 0, 200});
                DrawText(title, SW / 2 - MeasureText(title, 34) / 2, 14, 34, GOLD);
                if (SP[S_CASTLE].ok) { drawSprite(SP[S_CASTLE].t, 90, 72, 0, 66); drawSprite(SP[S_CASTLE].t, SW - 90, 72, 0, 66, true); }
                if (screen == 5) {
                    MainMenu::Action action = MainMenu::DrawFaction(SP[S_MAPBG].t, SP[S_MAPBG].ok, m, mc, gtime);
                    if (action == MainMenu::Action::ArabFaction || action == MainMenu::Action::EnglishFaction) {
                        keepStyle = action == MainMenu::Action::ArabFaction ? 0 : 1;
                        sfx(SFX_CLICK, 0.75f);
                        startMission(-1, cam); winRecorded = false; screen = 0;
                    } else if (action == MainMenu::Action::Back) { sfx(SFX_CLICK, 0.55f); screen = 0; }
                } else if (screen == 6) {
                    bool goBack = false;
                    MainMenu::DrawCredits(SP[S_MAPBG].t, SP[S_MAPBG].ok, m, mc, gtime, goBack);
                    if (goBack) { sfx(SFX_CLICK, 0.55f); screen = 0; }
                } else if (screen == 1) {   // ---- campaign: 10 missions ----
                const char* sub = "CAMPAIGN - choose a mission"; DrawText(sub, SW / 2 - MeasureText(sub, 24) / 2, 70, 24, ink);
                for (int i = 1; i <= 10; i++) {
                    int c = (i - 1) % 5, r = (i - 1) / 5; Rectangle rc = {24.f + c * 186, 118.f + r * 190, 176, 176}; const Mission& ms = MISSIONS[i];
                    bool locked = i > campDone + 1, h = CheckCollisionPointRec(m, rc) && !optOpen && !locked, doneM = i <= campDone;
                    DrawRectangleRec(rc, locked ? Color{20, 15, 10, 220} : (h ? Color{60, 45, 28, 240} : Color{36, 26, 16, 235})); DrawRectangleLinesEx(rc, h ? 3.f : 1.f, locked ? DARKGRAY : GOLD);
                    DrawText(TextFormat("Mission %d", i), (int)rc.x + 10, (int)rc.y + 8, 18, locked ? GRAY : GOLD);
                    lordFace(ms.en[0].lord, {rc.x + 10, rc.y + 34, 64, 64});
                    DrawText(ms.n == 1 ? "1 enemy" : TextFormat("%d enemies", ms.n), (int)rc.x + 84, (int)rc.y + 40, 15, locked ? GRAY : RAYWHITE);
                    for (int s = 0; s < 5; s++) DrawCircle((int)rc.x + 90 + s * 16, (int)rc.y + 76, 5, s < ms.stars ? (locked ? GRAY : ORANGE) : Color{60, 50, 40, 255});
                    DrawText("difficulty", (int)rc.x + 84, (int)rc.y + 86, 11, LIGHTGRAY);
                    drawWrapped(LORDS[ms.en[0].lord].name, rc.x + 10, rc.y + 108, 156, 15, locked ? GRAY : RAYWHITE);
                    if (ms.n > 1) DrawText("+ allies", (int)rc.x + 10, (int)rc.y + 128, 12, locked ? GRAY : Color{255, 200, 150, 255});
                    DrawText(locked ? "LOCKED" : (doneM ? "WON" : "OPEN"), (int)rc.x + 10, (int)rc.y + 152, 15, locked ? RED : (doneM ? LIME : YELLOW));
                    if (h && mc) { selMission = i; screen = 2; sfx(SFX_CLICK, 0.5f); }
                }
                DrawText("Win a mission to unlock the next one. Hard missions have 2-3 enemy lords!", 24, 508, 15, ink);
                if (btn({24.f, 560, 200, 44}, tutDone ? "Tutorial (done)" : "TUTORIAL", false, m, mc, 18)) { selMission = 0; screen = 2; }
                if (btn({SW - 224.f, 560, 200, 44}, "Back", false, m, mc, 18)) screen = 0;
            } else if (screen == 2) {   // ---- mission briefing ----
                const Mission& ms = MISSIONS[selMission];
                DrawRectangle(40, 70, 880, 530, Color{22, 15, 9, 235}); DrawRectangleLinesEx({40, 70, 880, 530}, 3, GOLD);
                DrawText(ms.name, 60, 84, 28, GOLD);
                float y = drawWrapped(ms.brief, 60, 126, 840, 17, ink); y = drawWrapped(string("OBJECTIVE: ") + ms.goal, 60, y + 8, 840, 17, GOLD);
                DrawText(TextFormat("Enemy lords: %d      Difficulty:", ms.n), 60, (int)y + 14, 16, RAYWHITE);
                for (int s = 0; s < 5; s++) DrawCircle(320 + s * 18, (int)y + 22, 6, s < ms.stars ? ORANGE : Color{60, 50, 40, 255});
                y += 48;
                for (int i = 0; i < ms.n; i++) {
                    const Lord& L = LORDS[ms.en[i].lord]; float yy = y + i * 80;
                    lordFace(ms.en[i].lord, {60, yy, 66, 66});
                    DrawText(L.name, 140, (int)yy + 2, 20, GOLD); DrawText(L.title, 300, (int)yy + 6, 14, Color{L.tint.r, L.tint.g, L.tint.b, 255});
                    DrawText(L.d1, 140, (int)yy + 28, 14, LIGHTGRAY); DrawText(L.d2, 140, (int)yy + 46, 14, LIGHTGRAY);
                }
                if (btn({SW / 2.f - 150, 530, 300, 54}, selMission == 0 ? "START TUTORIAL" : "START MISSION", false, m, mc, 24)) { startMission(selMission, cam); winRecorded = false; screen = 0; }
                if (btn({60.f, 540, 140, 40}, "Back", false, m, mc, 18)) screen = selMission == 0 ? 0 : 1;
            } else if (screen == 3) {   // ---- skirmish setup ----
                const char* sub = "SKIRMISH SETUP - choose your rival and battlefield";
                DrawText(sub, SW / 2 - MeasureText(sub, 20) / 2, 60, 20, ink);
                for (int i = 0; i < 8; ++i) {
                    const Lord& L = LORDS[13 + i]; int col = i % 4, row = i / 4;
                    Rectangle r = {24.f + col * 230.f, 92.f + row * 112.f, 218, 102};
                    bool h = CheckCollisionPointRec(m, r) && !optOpen;
                    if (h && mc) { lordStyle = 13 + i; sfx(SFX_CLICK, 0.5f); }
                    bool sel = lordStyle == 13 + i;
                    DrawRectangleRec(r, Color{24, 18, 12, 238});
                    DrawRectangleRec({r.x + 5, r.y + 5, 40, 40}, L.tint);
                    DrawCircleLines((int)r.x + 25, (int)r.y + 25, 16, L.stoneCol);
                    DrawText(L.emblem, (int)r.x + 7, (int)r.y + 53, 9, L.stoneCol);
                    DrawText(L.name, (int)r.x + 52, (int)r.y + 9, 15, sel ? GOLD : RAYWHITE);
                    DrawText(L.title, (int)r.x + 52, (int)r.y + 30, 10, L.tint);
                    DrawText(L.d1, (int)r.x + 52, (int)r.y + 49, 9, LIGHTGRAY);
                    DrawText(L.d2, (int)r.x + 52, (int)r.y + 68, 9, LIGHTGRAY);
                    DrawRectangleLinesEx(r, sel ? 3.f : 1.f, sel ? GOLD : Color{105, 85, 60, 255});
                }
                DrawText("Map", 24, 326, 16, GOLD);
                static const char* MAPN[8] = {"Crossroads", "Great Oasis", "Kings Valley", "Burning Sands", "Highlands", "Twin Castles", "Ancient Ruins", "Last Fortress"};
                for (int mi = 0; mi < 8; ++mi) if (btn({145.f + mi * 98.f, 320, 94, 30}, MAPN[mi], mapStyle == mi, m, mc, 10)) mapStyle = mi;
                DrawText("Difficulty", 24, 366, 16, GOLD);
                static const char* DN[4] = {"Easy", "Normal", "Hard", "Expert"};
                for (int d = 0; d < 4; ++d) if (btn({145.f + d * 112.f, 360, 106, 30}, DN[d], difficulty == d, m, mc, 12)) difficulty = d;
                DrawText("Opponents", 24, 406, 16, GOLD);
                for (int n = 1; n <= 3; ++n) if (btn({145.f + (n - 1) * 112.f, 400, 106, 30}, to_string(n).c_str(), opponentCount == n, m, mc, 14)) opponentCount = n;
                DrawText("Current map supports up to 3 AI castles. More opponents need a larger world/map system.", 500, 407, 12, ink);
                DrawText("Enemy unit types", 24, 450, 16, GOLD);
                static const char* EN_[5] = {"Slaves", "Archers", "Assassins", "Horsemen", "Catapults"};
                int enabled = 0; for (int k = 0; k < 5; k++) enabled += enemyOn[k];
                for (int k = 0; k < 5; k++) if (btn({145.f + k * 155.f, 444, 150, 30}, TextFormat("%s %s", enemyOn[k] ? "ON" : "OFF", EN_[k]), enemyOn[k], m, mc, 12) && !(enemyOn[k] && enabled == 1)) enemyOn[k] = !enemyOn[k];
                if (btn({SW / 2.f - 140, 510, 280, 48}, "START BATTLE", false, m, mc, 22)) { startMission(-1, cam); winRecorded = false; screen = 0; }
                if (btn({24.f, 510, 140, 44}, "Back", false, m, mc, 18)) screen = 0;
            }
            }
            if (screen == 4) { if (!helpOpen) screen = 0; else drawHelp(m, click && !optOpen, true); }
            if (optOpen) optionsUI(m, click, down);
            EndTextureMode(); presentFrame();
            if (inMenu) continue;
        }

        tickMsg(dt); if (won) { victoryT += dt; if (!victoryVoice && victoryT > 0.7f) { victoryVoice = true; sfxP(SFX_VVOICE, 1.0f, 1.0f); } }
        bool ui = optOpen || advOpen || helpOpen || (cheatOpen && cfg.cheats);
        float cs = 520 * dt / cam.zoom; bool edge = IsCursorOnScreen() && !ui;
        if (!ui) {
            if (IsKeyDown(KEY_LEFT) || (edge && m.x < 6 && m.x > -60)) cam.target.x -= cs;
            if (IsKeyDown(KEY_RIGHT) || (edge && m.x > SW - 6 && m.x < SW + 60)) cam.target.x += cs;
            if (IsKeyDown(KEY_UP) || (edge && m.y < 6 && m.y > -60)) cam.target.y -= cs;
            if (IsKeyDown(KEY_DOWN) || (edge && m.y > MAPH - 6 && m.y < MAPH)) cam.target.y += cs;
            if (inMap) cam.zoom = max(0.5f, min(1.6f, cam.zoom + GetMouseWheelMove() * 0.08f));
            if (IsKeyPressed(KEY_C)) cam.target = homeCam();
            if (IsKeyPressed(KEY_V)) cam.target = enemyCam();
        }
        cam.target.x = max(-780.f, min(1300.f, cam.target.x)); cam.target.y = max(-20.f, min(1060.f, cam.target.y));
        gCam = cam; viewR = {cam.target.x - cam.offset.x / cam.zoom, cam.target.y - cam.offset.y / cam.zoom, SW / cam.zoom, MAPH / cam.zoom};
        Vector2 sw = GetScreenToWorld2D(m, cam), mw = {sw.x / 2 + sw.y, sw.y - sw.x / 2};
        int tx = (int)floorf(mw.x / TS), ty = (int)floorf(mw.y / TS);
        hoverB = (inMap && !optOpen && !advOpen && !helpOpen) ? pickB(sw, 0) : -1;

        if (IsKeyPressed(KEY_O)) optOpen = !optOpen;
        if (IsKeyPressed(KEY_F9) && cfg.cheats) cheatOpen = !cheatOpen;
        if (IsKeyPressed(KEY_F1)) advOpen = !advOpen;
        if (IsKeyPressed(KEY_H)) helpOpen = !helpOpen;
        if (IsKeyPressed(KEY_ESCAPE)) { if (cheatOpen) cheatOpen = false; else if (optOpen) optOpen = false; else if (advOpen) advOpen = false; else if (helpOpen) helpOpen = false; else { build = 0; openB = -1; } }
        if (advOpen) tutAdv = true;
        if (isMarketOpen()) tutMarket = true;
        if (!gameOver && !won) {
            if (openB >= 0 && B[openB].dead) openB = -1;
            if (!ui) {
                if (IsKeyPressed(KEY_P)) paused = !paused;
                if (IsKeyPressed(KEY_T)) openB = isMarketOpen() ? -1 : -2;
                if (IsKeyPressed(KEY_TAB)) speedMul = speedMul % 3 + 1;
                if (IsKeyPressed(KEY_MINUS)) tax = max(-2, tax - 1);
                if (IsKeyPressed(KEY_EQUAL)) tax = min(4, tax + 1);
                if (IsKeyPressed(KEY_LEFT_BRACKET)) ration = max(0, ration - 1);
                if (IsKeyPressed(KEY_RIGHT_BRACKET)) ration = min(3, ration + 1);
                int cntT = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? -1 : (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) ? 5 : 1;
                if (IsKeyPressed(KEY_Q)) trainMany(0, cntT);   // archer
                if (IsKeyPressed(KEY_Z)) trainMany(1, cntT);   // crossbowman
                if (IsKeyPressed(KEY_W)) trainMany(2, cntT);   // spearman
                if (IsKeyPressed(KEY_D)) trainMany(3, cntT);   // maceman
                if (IsKeyPressed(KEY_E)) trainMany(4, cntT);   // swordsman
                if (IsKeyPressed(KEY_K)) trainMany(5, cntT);   // knight
                if (IsKeyPressed(KEY_A)) for (auto& u : U) u.sel = !u.enemy;
                if (click) {
                    bool overUnit = false;
                    for (auto& u : U) { Vector2 sp = iso(u.p.x, u.p.y); sp.y -= 10; if (!u.enemy && dist(sp, sw) < 12) overUnit = true; }
                    if (isPopupOpen() && popupClick(m)) {}
                    else if (!inMap) {}  // handled by hud()
                    else if (build) {
                        if (affordable(build)) { if (place(build, tx, ty)) { wood -= INFO[build].wood; stone -= INFO[build].stone; gold -= INFO[build].gold; addPart(0, (tx + 0.5f) * TS, (ty + 0.5f) * TS, 2, 6); sfx(SFX_PLACE, 0.7f); } else sfx(SFX_DENY, 0.5f); }
                        else { say("Not enough resources!"); sfx(SFX_DENY, 0.6f); }
                    } else {
                        int o = overUnit ? -1 : pickB(sw, 0);
                        if (o >= 0) {
                            selectedBuilding = o;
                            int bt = B[o].type; if (B[o].prog >= 1.f && (bt == BARRACKS || bt == TENT || bt == MARKET || bt == SIEGE || bt == ENGGUILD)) openB = o; else openB = -1;
                        } else { selectedBuilding = -1; openB = -1; ds = m; drag = true; }
                    }
                }
                if (drag && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                    drag = false;
                    Rectangle r = {min(ds.x, m.x), min(ds.y, m.y), fabsf(m.x - ds.x), fabsf(m.y - ds.y)};
                    if (r.width < 5 && r.height < 5) r = {m.x - 12, m.y - 16, 24, 30};
                    for (auto& u : U) { Vector2 sp = GetWorldToScreen2D(iso(u.p.x, u.p.y), cam); sp.y -= 8 * cam.zoom; u.sel = !u.enemy && CheckCollisionPointRec(sp, r); }
                }
                if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                    if (build) build = 0;
                    else if (inMap) {
                        int eb = pickB(sw, 1); Vector2 tgt = eb >= 0 ? center(B[eb]) : mw; int n = 0;
                        if (eb >= 0) say("Attacking the enemy building!");
                        for (auto& u : U) if (u.sel) { u.dst = eb >= 0 ? tgt : Vector2{tgt.x + (n % 5 - 2) * 16.f, tgt.y + (n / 5) * 16.f}; u.moving = true; u.path.clear(); u.repT = 0; n++; tutMoved = true; }
                    }
                }
            }
            if (!paused && !ui) for (int s = 0; s < speedMul; s++) update(dt);
            if (curMission == 0 && tutStep < NTUT - 1 && tutCheck(tutStep, cam)) { tutStep++; sfx(SFX_COIN, 0.8f); sfx(SFX_BELL, 0.3f, 0.5f); say("Step complete!"); }
        } else if (!ui) {
            if (IsKeyPressed(KEY_R)) { startMission(curMission, cam); winRecorded = false; build = 0; }
            if (IsKeyPressed(KEY_N) && won && curMission >= 0 && curMission < 10) { startMission(curMission + 1, cam); winRecorded = false; build = 0; }
            if (IsKeyPressed(KEY_M)) { inMenu = true; screen = curMission >= 1 ? 1 : 0; openB = -1; }
        }
        if (won && !winRecorded) { winRecorded = true; if (cheatUsed) say("Cheats were used - this win does not unlock missions."); else { if (curMission == 0) tutDone = true; else if (curMission > 0) campDone = max(campDone, curMission); saveCampaign(); } }
        if (!inMenu) {   // (a pressed M above may have just sent us back to the menu)
        BeginTextureMode(screenRT);
        ClearBackground(BLACK); // the world outside the map ends in black
        BeginScissorMode(0, 0, SW, MAPH); BeginMode2D(cam);
        drawGround(); if (cfg.shadows) drawShadows();
        for (size_t i = 0; i < B.size(); i++) if (B[i].type == HEALER && !B[i].dead && !B[i].enemy && B[i].prog >= 1.f) { Vector2 c = center(B[i]); if (inView(iso(c.x, c.y), HEAL_RADIUS * 1.5f)) drawRangeRing(c, HEAL_RADIUS, ((int)i == hoverB || (int)i == selectedBuilding) ? 210.f : 80.f, Color{90, 255, 130, 255}); }
        if (build == HEALER && inMap && !ui) drawRangeRing({(tx + 0.5f) * TS, (ty + 0.5f) * TS}, HEAL_RADIUS, 210.f, canPlace(HEALER, tx, ty) ? Color{90, 255, 130, 255} : Color{255, 90, 90, 255});
        vector<Item> items;  // painter's algorithm: far objects first
        for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) if (occ[y][x] == -2 && inView(iso((x + 0.5f) * TS, (y + 0.5f) * TS), 60)) items.push_back({(x + y + 1.f) * TS, 0, y * MW + x});
        if (rocksOk) for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) if (rockAt(x, y) >= 0 && inView(iso((x + 0.5f) * TS, (y + 0.5f) * TS), 60)) items.push_back({(x + y + 1.f) * TS - 0.5f, 3, y * MW + x});
        for (size_t i = 0; i < B.size(); i++) if (!B[i].dead) { Vector2 c = center(B[i]); if (inView(iso(c.x, c.y), 110)) items.push_back({c.x + c.y, 1, (int)i}); }
        for (size_t i = 0; i < U.size(); i++) if (inView(iso(U[i].p.x, U[i].p.y), 50)) items.push_back({U[i].p.x + U[i].p.y, 2, (int)i});
        for (size_t i = 0; i < WK.size(); i++) if (inView(iso(WK[i].x, WK[i].y), 50)) { float key = WK[i].x + WK[i].y; if (sizeOf(B[WK[i].bi].type) == 2) key = max(key, center(B[WK[i].bi]).x + center(B[WK[i].bi]).y + 1.f); items.push_back({key, 4, (int)i}); }
        for (size_t i = 0; i < AN.size(); i++) if (AN[i].state != 2 && inView(iso(AN[i].x, AN[i].y), 40)) items.push_back({AN[i].x + AN[i].y, 5, (int)i});
        for (size_t i = 0; i < HT.size(); i++) if (inView(iso(HT[i].x, HT[i].y), 40)) items.push_back({HT[i].x + HT[i].y, 6, (int)i});
        sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.d < b.d; });
        for (auto& it : items) { if (it.k == 0) drawTree(it.i % MW, it.i / MW); else if (it.k == 1) drawBuilding(B[it.i]); else if (it.k == 3) drawRock(it.i % MW, it.i / MW); else if (it.k == 4) drawWorker(WK[it.i]); else if (it.k == 5) drawAnimal(AN[it.i]); else if (it.k == 6) drawHunter(HT[it.i]); else drawUnit(U[it.i]); }
        drawIdlePeasants();
        // Short fall-and-fade death animation for horse archers.
        for (const auto& fx : horseDeathFX) if (HORSE_ARCHER_FR[3].id) {
            float q = min(1.f, fx.age / 0.72f); Vector2 dp = iso(fx.p.x, fx.p.y);
            Color fade = {255, 255, 255, (unsigned char)(255.f * (1.f - q))};
            const Texture2D& dead = HORSE_ARCHER_FR[3];
            DrawTexturePro(dead, Rectangle{0, 0, (float)dead.width, (float)dead.height},
                Rectangle{dp.x - 34, dp.y - 34 + q * 9.f, 68, 40}, Vector2{34, 20}, 78.f * q, fade);
        }
        drawParts();
        for (auto& p : P) {
            float arc = p.siege ? 45.f : p.len * 0.12f, z = sinf(p.t * (float)PI) * arc;
            Vector2 g = iso(p.a.x + (p.b.x - p.a.x) * p.t, p.a.y + (p.b.y - p.a.y) * p.t), q = {g.x, g.y - z};
            DrawEllipse((int)g.x, (int)g.y, 3, 1.5f, Color{0, 0, 0, 80});
            if (p.siege) { DrawCircleV(q, 5, DARKGRAY); DrawCircleV({q.x - 1, q.y - 1}, 2, GRAY); }
            else { Vector2 d = iso(p.b.x - p.a.x, p.b.y - p.a.y); float dl = max(1.f, hypotf(d.x, d.y)); DrawLineEx(q, {q.x - d.x / dl * 9, q.y - d.y / dl * 9 + z * 0.2f}, 2, {60, 40, 20, 255}); DrawCircleV(q, 1.5f, LIGHTGRAY); }
        }
        if (openB >= 0) { Vector2 c = center(B[openB]); DrawEllipseLines((int)iso(c.x, c.y).x, (int)iso(c.x, c.y).y, 22, 11, GOLD); }
        if (build && inMap && !ui) {
            int sz = sizeOf(build); bool ok = canPlace(build, tx, ty) && affordable(build); Color gc = ok ? Color{80, 255, 80, 130} : Color{255, 70, 70, 130};
            quad(isoH(tx * TS, ty * TS, 0), isoH((tx + sz) * TS, ty * TS, 0), isoH((tx + sz) * TS, (ty + sz) * TS, 0), isoH(tx * TS, (ty + sz) * TS, 0), gc);
            box(tx * TS + 3, ty * TS + 3, (tx + sz) * TS - 3, (ty + sz) * TS - 3, 0, 14, gc);
        }
        if (cfg.clouds) drawClouds();
        EndMode2D();
        if (cfg.vignette) {
            DrawRectangle(0, 0, SW, MAPH, Color{255, 190, 110, 12});  // warm light + vignette
            DrawRectangleGradientV(0, MAPH - 70, SW, 70, Color{0, 0, 0, 0}, Color{0, 0, 0, 90});
            DrawRectangleGradientH(0, 0, 60, MAPH, Color{0, 0, 0, 80}, Color{0, 0, 0, 0}); DrawRectangleGradientH(SW - 60, 0, 60, MAPH, Color{0, 0, 0, 0}, Color{0, 0, 0, 80});
        }
        EndScissorMode();
        if (drag) DrawRectangleLinesEx({min(ds.x, m.x), min(ds.y, m.y), fabsf(m.x - ds.x), fabsf(m.y - ds.y)}, 1, GREEN);
        if (isPopupOpen()) drawPopup(m);
        topBar();
        { string lab = curMission >= 0 ? string(MISSIONS[curMission].name) : string("Custom game"); lab += "  |  Enemy lords left: " + to_string(liveEnemies());
          int w = MeasureText(lab.c_str(), 12); DrawRectangle(SW - w - 14, 28, w + 12, 16, Color{0, 0, 0, 140}); DrawText(lab.c_str(), SW - w - 8, 30, 12, Color{255, 225, 160, 255}); }
        if (bannerT > 0) { int w = MeasureText(banner.c_str(), 20); DrawRectangle(SW / 2 - w / 2 - 10, 46, w + 20, 30, {0, 0, 0, 170}); DrawText(banner.c_str(), SW / 2 - w / 2, 51, 20, GOLD); }
        drawEnemyMsg();   // messages from the enemy lords
        drawSelectionInfo(m);
        if (curMission == 0 && !gameOver && !won) {   // tutorial panel
            Rectangle tr = {8, MAPH - 112.f, 500, 106}; DrawRectangleRec(tr, Color{25, 18, 10, 235}); DrawRectangleLinesEx(tr, 2, GOLD);
            DrawText(TextFormat("TUTORIAL  step %d / %d", tutStep + 1, NTUT), (int)tr.x + 10, (int)tr.y + 6, 16, GOLD);
            for (int i = 0; i < NTUT; i++) DrawRectangle((int)tr.x + 220 + i * 18, (int)tr.y + 10, 14, 8, i < tutStep ? LIME : (i == tutStep ? GOLD : Color{70, 60, 50, 255}));
            drawLines(TUT[tutStep], tr.x + 10, tr.y + 32, 14, RAYWHITE, 5);
            DrawText("H = full help", (int)tr.x + 410, (int)tr.y + 88, 11, LIGHTGRAY);
        }
        hud(m, click && !gameOver && !won && !ui, build, cam);
        if (cfg.showFps) DrawText(TextFormat("%d FPS", GetFPS()), SW - 70, 48, 14, LIME);
        if (gameOver || won) {
            const char* t = won ? (curMission == 0 ? "TUTORIAL COMPLETE!" : "VICTORY! All enemy lords are defeated.") : "DEFEAT - your keep has fallen.";
            if (won) { drawVictoryLogo(victoryT, curMission == 0); DrawRectangle(0, SH / 2 - 14, SW, 66, {0, 0, 0, 170}); }
            else { DrawRectangle(0, SH / 2 - 70, SW, 140, {0, 0, 0, 200}); DrawText(t, SW / 2 - MeasureText(t, 30) / 2, SH / 2 - 56, 30, GOLD); }
            bool nextOk = won && curMission >= 0 && curMission < 10;
            if (btn({SW / 2.f - 330, SH / 2.f - 4, 200, 40}, "Restart (R)", false, m, click, 18)) { startMission(curMission, cam); winRecorded = false; build = 0; }
            if (nextOk && btn({SW / 2.f - 100, SH / 2.f - 4, 200, 40}, "Next mission (N)", false, m, click, 18)) { startMission(curMission + 1, cam); winRecorded = false; build = 0; }
            if (btn({SW / 2.f + 130, SH / 2.f - 4, 200, 40}, "Menu (M)", false, m, click, 18)) { inMenu = true; screen = curMission >= 1 ? 1 : 0; openB = -1; }
            if (won && curMission == 10) DrawText("You have beaten the whole campaign! Congratulations, great lord!", SW / 2 - 270, SH / 2 + 62, 18, RAYWHITE);
        }
        if (advOpen) drawAdvisor(m, click);
        if (helpOpen) drawHelp(m, click, false);
        if (cheatOpen && cfg.cheats) drawCheatMenu(m, click);
        if (optOpen) optionsUI(m, click, down);
        EndTextureMode(); presentFrame();
        }
    }
    saveSettings(); saveCampaign();
    UnloadRenderTexture(screenRT);
    CloseAudioDevice(); CloseWindow();
    return 0;
}
