#pragma once
#include "Level.h"
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

// Puzzle rules and play state for one level: inventory, what is hidden or
// opened, which popup is showing, and the win state. No rendering in here.
//
// Rule for `container.contains`: if an entity with that id exists, it starts
// hidden "inside" the container. Opening the container gives it to the player
// (a pickup adds its item, a readable is shown and kept in the inventory).
// Otherwise `contains` is a plain item id that goes straight to the inventory.
//
// Items are used by holding them: select an inventory slot, then interact.
class Game {
public:
    enum class Mode { Walking, Reading, Keypad, Inventory, Dice, Won };

    explicit Game(const Level& level);   // `level` must outlive the Game

    void reset();
    void update(float dt);

    Mode mode() const { return m_mode; }
    void closePopup();                   // back to Walking (not from Won)
    void openInventory();

    bool isVisible(size_t entity) const { return !m_state[entity].hidden; }
    bool isOpened(size_t entity) const { return m_state[entity].opened; }

    // Short hint like "Open drawer", or "" if the entity can't be used.
    std::string actionLabel(size_t entity) const;
    void interact(size_t entity);

    // Keypad mode: checks the typed code. Returns true if it was right.
    bool submitCode(const std::string& code);

    // --- Inventory ---
    const std::vector<std::string>& inventory() const { return m_inventory; }
    // Selects the item in `slot` to hold; selecting the held slot puts it away.
    void selectSlot(int slot);
    int selectedSlot() const { return m_selected; }
    std::string heldItem() const;        // "" if nothing is held
    // Text of a readable inventory item, or "" if the item is not readable.
    std::string readableText(const std::string& item) const;
    void read(const std::string& text);  // shows a text popup

    // --- Dice (Mode::Dice) ---
    void rollDice();                     // rolls the open dice again
    const std::vector<int>& diceValues() const;
    int diceSides() const;
    int rollCount() const { return m_rollCount; }   // goes up on every roll

    const std::string& readingText() const { return m_readingText; }
    const std::string& message() const { return m_message; }
    float messageTimeLeft() const { return m_messageTimeLeft; }
    const std::string& speaker() const { return m_speaker; }
    const std::string& speech() const { return m_speech; }
    float speechTimeLeft() const { return m_speechTimeLeft; }
    float elapsedSeconds() const { return m_elapsed; }

private:
    struct EntityState {
        bool hidden = false;
        bool opened = false;     // containers
        bool unlocked = false;   // containers and exits
        size_t talks = 0;        // npcs: how many times talked to
        std::vector<int> dice;   // dice: last roll (empty = not rolled yet)
    };

    int findEntity(const std::string& id) const;   // -1 if missing
    void addItem(const std::string& item);
    void showMessage(const std::string& text);
    void say(const std::string& speaker, const std::string& line);
    std::string takeContents(const Entity& container);
    // Lock check for containers and exits. Unlocks with the held item if it
    // fits; otherwise shows why not. Returns true if now unlocked.
    bool tryUnlock(size_t entity, std::string& unlockMessage);
    std::string expectedCode(const Interaction& keypad) const;

    const Level& m_level;
    std::unordered_map<std::string, size_t> m_index;
    std::vector<EntityState> m_state;
    std::vector<std::string> m_inventory;
    std::unordered_map<std::string, std::string> m_readables;   // item -> text
    int m_selected = -1;

    Mode m_mode = Mode::Walking;
    int m_activeKeypad = -1;
    int m_activeDice = -1;
    int m_rollCount = 0;
    std::mt19937 m_rng{std::random_device{}()};

    std::string m_readingText;
    std::string m_message;
    float m_messageTimeLeft = 0.0f;
    std::string m_speaker;
    std::string m_speech;
    float m_speechTimeLeft = 0.0f;
    float m_elapsed = 0.0f;
};
