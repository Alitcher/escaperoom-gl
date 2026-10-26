#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

// In-memory form of a level JSON file. Field meanings: see LEVEL_FORMAT.md.

struct Transform {
    glm::vec3 position{0.0f};
    glm::vec3 rotationDeg{0.0f};   // Euler angles in degrees, matrix = Rx * Ry * Rz
    glm::vec3 scale{1.0f};
};

// What happens when the player clicks an entity. Only the fields that belong
// to `type` are used; the rest stay empty.
struct Interaction {
    std::string type;          // "" = not interactive; else pickup/container/readable/keypad/exit/npc/dice
    std::string item;          // pickup
    bool locked = false;       // container
    std::string unlockedBy;    // container, exit: item the player must hold
    std::string contains;      // container
    std::string text;          // readable
    std::string code;          // keypad
    std::string codeFrom;      // keypad: id of a dice entity; its total is the code
    std::string opens;         // keypad
    std::string name;          // npc: name shown when talking
    std::vector<std::string> lines;   // npc: one line per talk, in order, then repeats
    std::string gives;         // npc: item given on the first talk
    int count = 3;             // dice: how many dice
    int sides = 6;             // dice: faces per die
};

// Optional display info for an inventory item.
struct ItemDef {
    std::string id;
    std::string name;          // shown in the UI; defaults to the id
    std::string icon;          // PNG path relative to the level file; empty = drawn tile
};

struct Entity {
    std::string id;
    std::string model;         // "" = invisible, logic-only entity
    std::string parent;        // id of the parent entity; "" = top level
    Transform transform;       // relative to the parent (world if no parent)
    Interaction interaction;
};

struct Light {
    enum class Type { Point, Directional };
    Type type = Type::Point;
    glm::vec3 position{0.0f};            // point lights
    glm::vec3 direction{0.0f, -1.0f, 0.0f}; // directional lights
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
};

struct Level {
    int version = 1;
    std::string name;
    std::string author;
    glm::vec3 spawnPosition{0.0f, 1.6f, 0.0f};
    float spawnYawDeg = 0.0f;
    std::vector<Light> lights;
    std::vector<ItemDef> items;
    std::vector<Entity> entities;
    std::string winExit;       // id of the exit entity that wins the level
    std::string winMessage;    // shown on the win screen (optional)
    std::string baseDir;       // folder of the level file (not in the JSON)

    // Index of the entity with this id, or -1.
    int indexOf(const std::string& id) const;

    // Display name of an item: its "name" from "items", else the id itself.
    std::string itemName(const std::string& id) const;
    const ItemDef* findItem(const std::string& id) const;
};

// Reads a level file. On failure returns false and puts the reason in `error`.
bool loadLevel(const std::string& path, Level& out, std::string& error);

// Parses level JSON text. `baseDir` = folder that icon paths are relative to.
bool parseLevel(const std::string& text, const std::string& baseDir, Level& out, std::string& error);

// The level as readable JSON text, the same format loadLevel reads.
std::string levelToJson(const Level& level);

// Same data as levelToJson but on one line and much faster to make; used by
// the editor's undo history, which takes one every frame.
std::string levelSnapshot(const Level& level);

bool saveLevel(const std::string& path, const Level& level, std::string& error);

// "levels/room.json" -> "levels" ("." if there is no folder part).
std::string folderOf(const std::string& path);
