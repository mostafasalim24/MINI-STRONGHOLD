MINI STRONGHOLD - HORSE ARCHER UPDATE

CHANGES
- Horse archer PNG frames use RGBA transparency (the original transparent alpha channel is preserved).
- Horse archers cycle their bow poses while moving and while shooting.
- Horse archers gallop on long movement orders and slow to a walk near their destination.
- A short fall-and-fade death animation is shown when a horse archer is defeated.
- Custom Game now shows only the original three lord choices. The Arabic Lord and English Lord remain in the game's lord data and campaign missions; they are only hidden from the Custom Game selection screen.

CONTROLS
1. Start the game.
2. Select a horse archer (or a group containing horse archers) with the left mouse button.
3. Right-click a destination once. The horse archer moves automatically: gallops over longer distances and slows near the destination.
4. When an enemy is in range, it automatically shoots. Its shooting animation plays without another command.
5. Defeated horse archers play the short death animation.

BUILD / RUN
- Windows with MSYS2 UCRT64 and Raylib installed at C:\\msys64\\ucrt64:
  Double-click RUN_GAME_ENGLISH.bat
- The batch file compiles stronghold_game_english.cpp and launches StrongholdGame.exe.
- This package contains source and assets; it does not include a precompiled Windows EXE.
