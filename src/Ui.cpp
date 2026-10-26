#include "Ui.h"
#include "Theme.h"
#include <imgui.h>
#include <cctype>
#include <cstdlib>
#include <functional>

static constexpr size_t kMaxCodeLength = 8;
static constexpr int kPocketSlots = 9;
static constexpr float kSlotSize = 64.0f;
static constexpr float kRollAnimSeconds = 0.8f;

static constexpr ImGuiWindowFlags kPopupFlags =
    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
static constexpr ImGuiWindowFlags kOverlayFlags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;

static const ImU32 kYellow = IM_COL32(255, 220, 80, 255);

static ImVec2 screenCenter() {
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    return ImVec2(size.x * 0.5f, size.y * 0.5f);
}

// Text centered horizontally at height y, drawn on top of everything.
static void centeredText(float y, const char* text, ImU32 color) {
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 size = ImGui::CalcTextSize(text);
    const ImVec2 pos(screenCenter().x - size.x * 0.5f, y);
    draw->AddText(ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0, 0, 0, 200), text);   // shadow
    draw->AddText(pos, color, text);
}

// A small overlay box with one line of text, its bottom edge centered at y.
static void bottomBox(const char* id, float y, const std::string& text, float alpha) {
    ImGui::SetNextWindowPos(ImVec2(screenCenter().x, y), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowBgAlpha(alpha);
    if (ImGui::Begin(id, nullptr, kOverlayFlags))
        ImGui::TextUnformatted(text.c_str());
    ImGui::End();
}

// Up to 2 letters for an item without an icon: "Prop key" -> "PK", "note" -> "NO".
static std::string initials(const std::string& name) {
    std::string out;
    bool wordStart = true;
    for (char c : name) {
        if (std::isspace((unsigned char)c)) {
            wordStart = true;
        } else if (wordStart && out.size() < 2) {
            out += (char)std::toupper((unsigned char)c);
            wordStart = false;
        }
    }
    if (out.size() == 1 && name.size() > 1)
        out += (char)std::toupper((unsigned char)name[1]);
    return out;
}

void drawItemTile(ImDrawList* draw, ImVec2 min, float size, const std::string& item,
                  const Level& level, IconCache& icons, bool held, int slotNumber) {
    const ImVec2 max(min.x + size, min.y + size);
    draw->AddRectFilled(min, max, IM_COL32(20, 22, 30, 200), 6.0f);

    if (!item.empty()) {
        const ItemDef* def = level.findItem(item);
        const unsigned int tex = (def && !def->icon.empty()) ? icons.get(level.baseDir + "/" + def->icon) : 0;
        const float pad = size * 0.12f;
        const ImVec2 a(min.x + pad, min.y + pad), b(max.x - pad, max.y - pad);
        if (tex != 0) {
            draw->AddImage((ImTextureID)tex, a, b);
        } else {
            // Same item -> same color every time.
            const float hue = (float)(std::hash<std::string>{}(item) % 360) / 360.0f;
            draw->AddRectFilled(a, b, ImColor::HSV(hue, 0.45f, 0.75f), 6.0f);
            const std::string letters = initials(level.itemName(item));
            const ImVec2 ts = ImGui::CalcTextSize(letters.c_str());
            draw->AddText(ImVec2((a.x + b.x - ts.x) * 0.5f, (a.y + b.y - ts.y) * 0.5f),
                          IM_COL32(255, 255, 255, 255), letters.c_str());
        }
    }

    if (slotNumber >= 1) {
        const std::string n = std::to_string(slotNumber);
        draw->AddText(ImVec2(min.x + 4, min.y + 2), IM_COL32(255, 255, 255, 160), n.c_str());
    }
    draw->AddRect(min, max, held ? kYellow : IM_COL32(255, 255, 255, 60), 6.0f, 0, held ? 3.0f : 1.0f);
}

static void drawCrosshair(const Game& game, int hovered) {
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 c = screenCenter();
    const bool active = hovered >= 0;
    draw->AddCircle(c, active ? 7.0f : 4.0f, active ? kYellow : IM_COL32(255, 255, 255, 200), 16, 2.0f);

    if (active) {
        const std::string hint = "[E / click] " + game.actionLabel((size_t)hovered);
        centeredText(c.y + 18.0f, hint.c_str(), kYellow);
    }
}

// Bottom of the screen: 9 pocket slots and the name of the held item.
static void drawPocketBar(const Game& game, const Level& level, IconCache& icons) {
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float gap = 6.0f;
    const float width = kPocketSlots * kSlotSize + (kPocketSlots - 1) * gap;
    const float x0 = (display.x - width) * 0.5f;
    const float y0 = display.y - kSlotSize - 16.0f;

    const auto& items = game.inventory();
    for (int i = 0; i < kPocketSlots; ++i) {
        const std::string item = i < (int)items.size() ? items[i] : "";
        drawItemTile(draw, ImVec2(x0 + i * (kSlotSize + gap), y0), kSlotSize, item, level, icons,
                     i == game.selectedSlot(), i + 1);
    }

    const std::string held = game.heldItem();
    if (!held.empty()) {
        const std::string label = "Holding: " + level.itemName(held);
        centeredText(y0 - 26.0f, label.c_str(), kYellow);
    }
}

static void drawHud(const Game& game) {
    // Top-left: timer.
    ImGui::SetNextWindowPos(ImVec2(12, 12));
    ImGui::SetNextWindowBgAlpha(0.85f);
    if (ImGui::Begin("##timer", nullptr, kOverlayFlags)) {
        const int secs = (int)game.elapsedSeconds();
        ImGui::Text("Time: %d:%02d", secs / 60, secs % 60);
    }
    ImGui::End();

    // Top-right: controls.
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(display.x - 12, 12), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.85f);
    if (ImGui::Begin("##controls", nullptr, kOverlayFlags)) {
        ImGui::TextUnformatted("WASD / arrows: walk   Shift: run   Mouse: look");
        ImGui::TextUnformatted("E / click: use   1-9: hold item   Tab: inventory");
        ImGui::TextUnformatted("R: restart   F5: editor   Esc: quit");
    }
    ImGui::End();
}

// NPC speech and short messages, stacked above the pocket bar.
static void drawTexts(const Game& game) {
    const float bottom = ImGui::GetIO().DisplaySize.y - kSlotSize - 56.0f;
    float y = bottom;
    if (game.speechTimeLeft() > 0.0f && !game.speech().empty()) {
        bottomBox("##speech", y, game.speaker() + ": " + game.speech(), 0.95f);
        y -= 44.0f;
    }
    if (game.messageTimeLeft() > 0.0f && !game.message().empty())
        bottomBox("##message", y, game.message(), 0.9f);
}

static void drawReading(Game& game) {
    ImGui::SetNextWindowPos(screenCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (beginRetroWindow("Note", nullptr, kPopupFlags)) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
        ImGui::TextUnformatted(game.readingText().c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        if (ImGui::Button("Close (Esc)"))
            game.closePopup();
    }
    ImGui::End();
}

// Big inventory: click an item to hold it (click again to put it away).
static void drawInventory(Game& game, const Level& level, IconCache& icons) {
    ImGui::SetNextWindowPos(screenCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (beginRetroWindow("Inventory", nullptr, kPopupFlags)) {
        const auto& items = game.inventory();
        if (items.empty())
            ImGui::TextUnformatted("Your pocket is empty.");
        else
            ImGui::TextDisabled("Click an item to hold it.");

        for (int i = 0; i < (int)items.size(); ++i) {
            ImGui::PushID(i);
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("slot", ImVec2(kSlotSize, kSlotSize)))
                game.selectSlot(i);
            drawItemTile(ImGui::GetWindowDrawList(), pos, kSlotSize, items[i], level, icons,
                         i == game.selectedSlot(), i < kPocketSlots ? i + 1 : 0);
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::TextUnformatted(level.itemName(items[i]).c_str());
            const std::string text = game.readableText(items[i]);
            if (!text.empty() && ImGui::SmallButton("Read"))
                game.read(text);
            ImGui::EndGroup();
            ImGui::PopID();
        }
        ImGui::Spacing();
        if (ImGui::Button("Close (Tab / Esc)"))
            game.closePopup();
    }
    ImGui::End();
}

static void drawKeypad(Game& game, UiState& ui) {
    std::string& entry = ui.keypadEntry;
    bool submit = false;

    // Keyboard: digits, Backspace, Enter.
    for (int d = 0; d <= 9; ++d) {
        const bool pressed = ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_0 + d), false) ||
                             ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_Keypad0 + d), false);
        if (pressed && entry.size() < kMaxCodeLength)
            entry += (char)('0' + d);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false) && !entry.empty())
        entry.pop_back();
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
        submit = true;

    ImGui::SetNextWindowPos(screenCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (beginRetroWindow("Keypad", nullptr, kPopupFlags)) {
        ImGui::Text("Code:  %s_", entry.c_str());
        ImGui::Separator();

        const ImVec2 button(56, 44);
        const char* labels[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "C", "0", "OK"};
        for (int i = 0; i < 12; ++i) {
            if (i % 3 != 0)
                ImGui::SameLine();
            if (ImGui::Button(labels[i], button)) {
                if (i == 9)
                    entry.clear();
                else if (i == 11)
                    submit = true;
                else if (entry.size() < kMaxCodeLength)
                    entry += labels[i];
            }
        }
        ImGui::Spacing();
        ImGui::TextDisabled("Type digits, Enter = OK, Esc = close");
        if (ImGui::Button("Close"))
            game.closePopup();
    }
    ImGui::End();

    if (submit) {
        game.submitCode(entry);
        entry.clear();
    }
}

// One die face: a rounded white square with 1..6 pips. Other values (dice
// with more sides) are shown as a number.
static void drawDieFace(ImDrawList* draw, ImVec2 min, float size, int value) {
    const ImVec2 max(min.x + size, min.y + size);
    draw->AddRectFilled(min, max, IM_COL32(245, 245, 240, 255), size * 0.18f);
    draw->AddRect(min, max, IM_COL32(60, 60, 70, 255), size * 0.18f, 0, 2.0f);

    if (value < 1 || value > 6) {
        const std::string n = std::to_string(value);
        const ImVec2 ts = ImGui::CalcTextSize(n.c_str());
        draw->AddText(ImVec2(min.x + (size - ts.x) * 0.5f, min.y + (size - ts.y) * 0.5f),
                      IM_COL32(30, 30, 40, 255), n.c_str());
        return;
    }
    // Pip spots on a 3 x 3 grid: 0 = left/top, 1 = middle, 2 = right/bottom.
    static const int pips[6][6][2] = {
        {{1, 1}},
        {{0, 0}, {2, 2}},
        {{0, 0}, {1, 1}, {2, 2}},
        {{0, 0}, {2, 0}, {0, 2}, {2, 2}},
        {{0, 0}, {2, 0}, {1, 1}, {0, 2}, {2, 2}},
        {{0, 0}, {2, 0}, {0, 1}, {2, 1}, {0, 2}, {2, 2}},
    };
    const float step = size * 0.28f;
    for (int p = 0; p < value; ++p) {
        const ImVec2 c(min.x + size * 0.5f + (pips[value - 1][p][0] - 1) * step,
                       min.y + size * 0.5f + (pips[value - 1][p][1] - 1) * step);
        draw->AddCircleFilled(c, size * 0.09f, IM_COL32(30, 30, 40, 255));
    }
}

static void drawDice(Game& game, UiState& ui) {
    // A new roll starts a short tumble animation before the result shows.
    if (game.rollCount() != ui.seenRollCount) {
        ui.seenRollCount = game.rollCount();
        ui.rollAnimLeft = kRollAnimSeconds;
    }
    ui.rollAnimLeft -= ImGui::GetIO().DeltaTime;
    const bool rolling = ui.rollAnimLeft > 0.0f;

    if (!rolling && ImGui::IsKeyPressed(ImGuiKey_Space, false))
        game.rollDice();

    ImGui::SetNextWindowPos(screenCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (beginRetroWindow("Dice", nullptr, kPopupFlags)) {
        const std::vector<int>& values = game.diceValues();
        const float size = 72.0f, gap = 14.0f;
        const ImVec2 start = ImGui::GetCursorScreenPos();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        int total = 0;
        for (size_t i = 0; i < values.size(); ++i) {
            const int shown = rolling ? 1 + std::rand() % game.diceSides() : values[i];
            drawDieFace(draw, ImVec2(start.x + i * (size + gap), start.y), size, shown);
            total += values[i];
        }
        ImGui::Dummy(ImVec2(values.size() * (size + gap) - gap, size));
        ImGui::Spacing();

        if (rolling)
            ImGui::TextUnformatted("Rolling...");
        else
            ImGui::Text("Total: %d", total);
        ImGui::Spacing();

        ImGui::BeginDisabled(rolling);
        if (ImGui::Button("Roll again (Space)"))
            game.rollDice();
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Close (Esc)"))
            game.closePopup();
    }
    ImGui::End();
}

static UiAction drawWin(const Game& game, const Level& level) {
    UiAction action = UiAction::None;
    ImGui::SetNextWindowPos(screenCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (beginRetroWindow("You escaped!", nullptr, kPopupFlags)) {
        ImGui::TextUnformatted(level.winMessage.empty() ? "You escaped!" : level.winMessage.c_str());
        const int secs = (int)game.elapsedSeconds();
        ImGui::Text("Level: %s", level.name.c_str());
        ImGui::Text("Time: %d:%02d", secs / 60, secs % 60);
        ImGui::Spacing();
        if (ImGui::Button("Play again (R)"))
            action = UiAction::Restart;
        ImGui::SameLine();
        if (ImGui::Button("Quit (Esc)"))
            action = UiAction::Quit;
    }
    ImGui::End();
    return action;
}

UiAction drawGameUi(Game& game, const Level& level, int hovered, UiState& ui, IconCache& icons) {
    drawHud(game);
    drawTexts(game);
    drawPocketBar(game, level, icons);

    switch (game.mode()) {
    case Game::Mode::Walking:   drawCrosshair(game, hovered); break;
    case Game::Mode::Reading:   drawReading(game); break;
    case Game::Mode::Inventory: drawInventory(game, level, icons); break;
    case Game::Mode::Keypad:    drawKeypad(game, ui); break;
    case Game::Mode::Dice:      drawDice(game, ui); break;
    case Game::Mode::Won:       return drawWin(game, level);
    }
    return UiAction::None;
}
