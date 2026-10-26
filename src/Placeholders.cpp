#include "Placeholders.h"
#include <algorithm>
#include <unordered_map>

namespace {

const glm::vec3 kWood(0.55f, 0.35f, 0.20f);
const glm::vec3 kDarkWood(0.30f, 0.22f, 0.18f);
const glm::vec3 kMetal(0.75f, 0.75f, 0.70f);
const glm::vec3 kGold(0.95f, 0.75f, 0.20f);

// Sizes in meters. The room is 6 x 3 x 6 with its floor at y = 0.
const std::unordered_map<std::string, std::vector<PlaceholderBox>>& table() {
    static const std::unordered_map<std::string, std::vector<PlaceholderBox>> t = {
        // 1 x 1 floor tile with its top at y = 0; the level scales it up.
        {"floor.glb", {{{0, -0.05f, 0}, {1, 0.1f, 1}, {0.35f, 0.28f, 0.22f}}}},

        // Four walls + ceiling around a 6 x 6 floor.
        {"room.glb", {
            {{0, 1.5f, -3.05f}, {6.2f, 3, 0.1f}, {0.55f, 0.50f, 0.60f}},
            {{0, 1.5f,  3.05f}, {6.2f, 3, 0.1f}, {0.55f, 0.50f, 0.60f}},
            {{-3.05f, 1.5f, 0}, {0.1f, 3, 6},    {0.50f, 0.47f, 0.58f}},
            {{ 3.05f, 1.5f, 0}, {0.1f, 3, 6},    {0.50f, 0.47f, 0.58f}},
            {{0, 3.05f, 0},     {6.2f, 0.1f, 6.2f}, {0.70f, 0.70f, 0.72f}},
        }},

        // Mannequin origin is at its feet.
        {"mannequin.glb", {
            {{0, 0.75f, 0}, {0.40f, 1.50f, 0.25f}, {0.60f, 0.20f, 0.40f}},
            {{0, 1.65f, 0}, {0.22f, 0.25f, 0.22f}, {0.90f, 0.85f, 0.80f}},
        }},

        {"poster.glb", {{{0, 0, 0}, {0.7f, 1.0f, 0.02f}, {0.90f, 0.55f, 0.20f}}}},

        {"drawer.glb", {
            {{0, 0, 0},           {0.8f, 1.0f, 0.5f},   kWood},
            {{0, 0.25f, 0.26f},   {0.3f, 0.05f, 0.03f}, kMetal},
        }},

        {"prop_key.glb", {{{0, 0, 0}, {0.08f, 0.02f, 0.2f}, kGold}}},

        {"cabinet.glb", {
            {{0, 0, 0},           {0.8f, 2.0f, 0.5f},   kDarkWood},
            {{0.3f, 0, 0.26f},    {0.04f, 0.3f, 0.03f}, kMetal},
        }},

        {"note.glb", {{{0, 0, 0}, {0.2f, 0.01f, 0.28f}, {0.95f, 0.95f, 0.90f}}}},

        {"keypad.glb", {
            {{0, 0, 0},           {0.15f, 0.22f, 0.06f}, {0.15f, 0.15f, 0.18f}},
            {{0, 0.06f, 0.031f},  {0.10f, 0.04f, 0.01f}, {0.30f, 0.90f, 0.50f}},
        }},

        // NPC origin is at its feet: body, head, hair.
        {"npc.glb", {
            {{0, 0.60f, 0}, {0.45f, 1.20f, 0.28f}, {0.20f, 0.60f, 0.60f}},
            {{0, 1.38f, 0}, {0.24f, 0.26f, 0.24f}, {0.90f, 0.80f, 0.70f}},
            {{0, 1.54f, 0}, {0.30f, 0.10f, 0.30f}, {0.95f, 0.45f, 0.70f}},
        }},

        // Three small dice side by side; the origin is their center.
        {"dice.glb", {
            {{-0.12f, 0, 0}, {0.08f, 0.08f, 0.08f}, {0.95f, 0.95f, 0.92f}},
            {{ 0.00f, 0, 0}, {0.08f, 0.08f, 0.08f}, {0.95f, 0.95f, 0.92f}},
            {{ 0.12f, 0, 0}, {0.08f, 0.08f, 0.08f}, {0.95f, 0.95f, 0.92f}},
        }},

        // Generic small item lying around (a pickup without its own model).
        {"item.glb", {{{0, 0, 0}, {0.2f, 0.1f, 0.2f}, {0.95f, 0.55f, 0.15f}}}},

        {"door.glb", {
            {{0, 0, 0},           {1.0f, 2.0f, 0.1f},    {0.45f, 0.30f, 0.20f}},
            {{0.38f, 0, 0.07f},   {0.06f, 0.06f, 0.06f}, kGold},
        }},
    };
    return t;
}

} // namespace

const std::vector<PlaceholderBox>& placeholderFor(const std::string& model) {
    static const std::vector<PlaceholderBox> unknown = {
        {{0, 0, 0}, {0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 1.0f}},
    };
    const auto& t = table();
    auto it = t.find(model);
    return it != t.end() ? it->second : unknown;
}

bool hasPlaceholder(const std::string& model) {
    return table().count(model) > 0;
}

std::vector<std::string> placeholderModels() {
    std::vector<std::string> names;
    for (const auto& entry : table())
        names.push_back(entry.first);
    std::sort(names.begin(), names.end());
    return names;
}
