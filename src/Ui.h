#pragma once
#include "Game.h"
#include "Icons.h"
#include "Level.h"
#include <string>

// On-screen game UI (Dear ImGui): crosshair, hints, messages, NPC speech,
// pocket bar, inventory, note popup, keypad, dice and win screen.
// Call between ImGui::NewFrame and ImGui::Render.

struct UiState {
    std::string keypadEntry;   // digits typed on the open keypad
    int seenRollCount = 0;     // last dice roll the UI has animated
    float rollAnimLeft = 0.0f; // seconds of dice tumbling still to show
};

enum class UiAction { None, Restart, Quit };

// `hovered` = entity under the crosshair, or -1.
UiAction drawGameUi(Game& game, const Level& level, int hovered, UiState& ui, IconCache& icons);

struct ImDrawList;
struct ImVec2;

// One inventory square at `min`: the item's icon (or a colored tile with its
// letters), a yellow frame when `held`, and `slotNumber` in the corner if >= 1.
// Shared by the game's pocket bar and the editor's Project panel.
void drawItemTile(ImDrawList* draw, ImVec2 min, float size, const std::string& item,
                  const Level& level, IconCache& icons, bool held, int slotNumber);
