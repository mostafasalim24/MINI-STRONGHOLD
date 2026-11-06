#include "MainMenu.h"
#include <cmath>

namespace MainMenu {
namespace {
const Color parchment = {218, 190, 137, 255};
const Color parchmentLight = {242, 220, 174, 255};
const Color ink = {52, 35, 20, 255};
const Color stone = {54, 48, 39, 255};
const Color gold = {211, 166, 78, 255};

void drawBackground(Texture2D bg, bool hasBg, float time) {
    const float w = 960.0f, h = 688.0f;
    if (hasBg && bg.id) {
        const float drift = std::sin(time * 0.12f) * 5.0f;
        DrawTexturePro(bg, {0, 0, (float)bg.width, (float)bg.height},
                       {-drift, 0, w + 2 * drift, h}, {0, 0}, 0, WHITE);
        DrawRectangle(0, 0, 960, 688, {10, 12, 17, 118});
    } else {
        DrawRectangleGradientV(0, 0, 960, 688, {105, 78, 49, 255}, {25, 25, 24, 255});
    }
    // Atmospheric dust and drifting battlefield haze.
    for (int i = 0; i < 34; ++i) {
        float x = std::fmod(i * 83.7f + time * (5.0f + (i % 4)), 1000.0f) - 20.0f;
        float y = 70.0f + std::fmod(i * 47.3f + std::sin(time * 0.35f + i) * 13.0f, 590.0f);
        unsigned char a = (unsigned char)(18 + (i % 5) * 5);
        DrawCircle((int)x, (int)y, 1.0f + (i % 3) * 0.45f, {245, 210, 151, a});
    }
    DrawRectangleGradientV(0, 0, 960, 105, {0, 0, 0, 115}, {0, 0, 0, 0});
    DrawRectangleGradientV(0, 570, 960, 118, {0, 0, 0, 0}, {0, 0, 0, 145});
}

void drawBanner(float x, float y, Color fabric, float time, bool mirrored) {
    float sway = std::sin(time * 2.1f + x * 0.01f) * 5.0f;
    DrawLineEx({x, y - 13}, {x, y + 90}, 5, {105, 82, 52, 255});
    Vector2 a{x, y}, b{x + (mirrored ? -62.0f : 62.0f), y + sway * 0.25f};
    Vector2 c{x + (mirrored ? -57.0f : 57.0f), y + 70 + sway}, d{x, y + 67};
    DrawTriangle(a, b, c, fabric);
    DrawTriangle(a, c, d, fabric);
    DrawTriangleLines(a, b, c, gold);
    DrawTriangleLines(a, c, d, gold);
    DrawLineEx({x, y + 70 + sway}, {x + (mirrored ? -57.0f : 57.0f), y + 70 + sway}, 1.2f, {245, 211, 145, 190});
}

void drawStonePanel(Rectangle r) {
    DrawRectangleRec({r.x + 8, r.y + 9, r.width, r.height}, {0, 0, 0, 125});
    DrawRectangleRec(r, stone);
    DrawRectangleRec({r.x + 7, r.y + 7, r.width - 14, r.height - 14}, {91, 78, 59, 255});
    DrawRectangleLinesEx(r, 5, {28, 25, 21, 255});
    DrawRectangleLinesEx({r.x + 5, r.y + 5, r.width - 10, r.height - 10}, 2, gold);
    DrawRectangleLinesEx({r.x + 11, r.y + 11, r.width - 22, r.height - 22}, 1, {189, 157, 103, 255});
    for (int i = 0; i < 4; ++i) {
        float cx = (i % 2) ? r.x + r.width - 17 : r.x + 17;
        float cy = (i / 2) ? r.y + r.height - 17 : r.y + 17;
        DrawCircle((int)cx, (int)cy, 4, {34, 30, 24, 255});
        DrawCircleLines((int)cx, (int)cy, 4, {197, 161, 96, 255});
    }
}

bool parchmentButton(Rectangle r, const char* label, Vector2 mouse, bool click, int fontSize = 20) {
    bool hover = CheckCollisionPointRec(mouse, r);
    DrawRectangleRec({r.x + 3, r.y + 4, r.width, r.height}, {0, 0, 0, 100});
    DrawRectangleRec(r, hover ? parchmentLight : parchment);
    DrawRectangleLinesEx(r, hover ? 3.0f : 2.0f, hover ? {255, 220, 135, 255} : {93, 63, 35, 255});
    DrawRectangleLinesEx({r.x + 5, r.y + 4, r.width - 10, r.height - 8}, 1, {166, 127, 72, 255});
    int tw = MeasureText(label, fontSize);
    DrawText(label, (int)(r.x + (r.width - tw) * 0.5f), (int)(r.y + (r.height - fontSize) * 0.5f),
             fontSize, hover ? {91, 48, 20, 255} : ink);
    if (hover) DrawRectangle((int)r.x + 9, (int)r.y + 8, 3, (int)r.height - 16, gold);
    return hover && click;
}

void drawHeading(const char* subtitle, float time) {
    const char* title = "KINGS OF MESOPOTAMIA";
    int tw = MeasureText(title, 37);
    DrawText(title, 480 - tw / 2 + 2, 43, 37, {0, 0, 0, 220});
    DrawText(title, 480 - tw / 2, 40, 37, {239, 205, 132, 255});
    int sw = MeasureText(subtitle, 15);
    DrawText(subtitle, 480 - sw / 2, 91, 15, {235, 222, 193, 255});
    DrawLineEx({420, 119}, {540, 119}, 2, gold);
    drawBanner(425, 31, {130, 43, 30, 255}, time, true);
    drawBanner(535, 31, {177, 151, 86, 255}, time + 0.8f, false);
}
}

Action DrawMain(Texture2D battleBackground, bool hasBackground, Vector2 mouse, bool click, float time) {
    drawBackground(battleBackground, hasBackground, time);
    drawHeading("A MEDIEVAL KINGDOMS REAL-TIME STRATEGY", time);
    Rectangle panel{42, 135, 350, 500};
    drawStonePanel(panel);
    const char* labels[8] = {"NEW GAME", "CAMPAIGN", "CUSTOM BATTLE", "TUTORIAL", "OPTIONS", "HELP", "CREDITS", "EXIT GAME"};
    const Action actions[8] = {Action::NewGame, Action::Campaign, Action::CustomBattle, Action::Tutorial,
                               Action::Options, Action::Help, Action::Credits, Action::Exit};
    for (int i = 0; i < 8; ++i) {
        Rectangle r{72, 157.0f + i * 58.0f, 290, 48};
        if (parchmentButton(r, labels[i], mouse, click, 19)) return actions[i];
    }
    // Decorative battlefield labels and subtle light pulse on the right.
    DrawText("THE FATE OF TWO KINGDOMS", 465, 565, 18, {238, 214, 170, 230});
    DrawText("ARAB KINGDOM  |  ENGLISH KINGDOM", 465, 593, 13, {222, 202, 168, 220});
    float glow = 18.0f + 8.0f * (0.5f + 0.5f * std::sin(time * 1.4f));
    DrawCircleGradient(790, 310, glow, {255, 209, 128, 28}, {255, 209, 128, 0});
    return Action::None;
}

Action DrawFaction(Texture2D battleBackground, bool hasBackground, Vector2 mouse, bool click, float time) {
    drawBackground(battleBackground, hasBackground, time);
    drawHeading("CHOOSE YOUR KINGDOM", time);
    Rectangle panel{220, 170, 520, 370};
    drawStonePanel(panel);
    DrawText("SELECT YOUR FACTION", 347, 205, 24, {242, 215, 164, 255});
    Rectangle arab{265, 270, 205, 145}, english{490, 270, 205, 145};
    bool ah = CheckCollisionPointRec(mouse, arab), eh = CheckCollisionPointRec(mouse, english);
    DrawRectangleRec(arab, ah ? Color{119, 72, 39, 255} : Color{88, 55, 33, 255});
    DrawRectangleLinesEx(arab, ah ? 4 : 2, gold);
    DrawText("ARAB", 326, 300, 27, {248, 220, 164, 255});
    DrawText("KINGDOM", 317, 337, 18, RAYWHITE);
    DrawText("Desert warriors", 305, 375, 14, {230, 205, 160, 255});
    DrawRectangleRec(english, eh ? Color{75, 87, 105, 255} : Color{49, 59, 73, 255});
    DrawRectangleLinesEx(english, eh ? 4 : 2, gold);
    DrawText("ENGLISH", 535, 300, 25, {231, 224, 207, 255});
    DrawText("KINGDOM", 542, 337, 18, RAYWHITE);
    DrawText("Northern armies", 525, 375, 14, {210, 214, 224, 255});
    if (ah && click) return Action::ArabFaction;
    if (eh && click) return Action::EnglishFaction;
    if (parchmentButton({390, 462, 180, 42}, "BACK", mouse, click, 17)) return Action::Back;
    return Action::None;
}

void DrawCredits(Texture2D battleBackground, bool hasBackground, Vector2 mouse, bool click, float time, bool& goBack) {
    drawBackground(battleBackground, hasBackground, time);
    drawHeading("THE PEOPLE BEHIND THE KINGDOMS", time);
    Rectangle panel{210, 175, 540, 350};
    drawStonePanel(panel);
    DrawText("KINGS OF MESOPOTAMIA", 326, 230, 26, {242, 215, 164, 255});
    DrawText("An independent medieval RTS project", 320, 280, 17, RAYWHITE);
    DrawText("Inspired by classic castle-building strategy games", 278, 315, 15, {225, 211, 183, 255});
    DrawText("Created by Mostafa Al-Jumaily", 335, 370, 18, gold);
    if (parchmentButton({390, 445, 180, 44}, "BACK", mouse, click, 18)) goBack = true;
}
}
