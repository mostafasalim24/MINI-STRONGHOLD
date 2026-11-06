#pragma once
#include "raylib.h"

namespace MainMenu {
enum class Action {
    None, NewGame, Campaign, CustomBattle, Tutorial, Options, Help, Credits, Exit,
    ArabFaction, EnglishFaction, Back
};

Action DrawMain(Texture2D battleBackground, bool hasBackground, Vector2 mouse, bool click, float time);
Action DrawFaction(Texture2D battleBackground, bool hasBackground, Vector2 mouse, bool click, float time);
void DrawCredits(Texture2D battleBackground, bool hasBackground, Vector2 mouse, bool click, float time, bool& goBack);
}
