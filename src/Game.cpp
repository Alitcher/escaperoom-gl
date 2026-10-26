#include "Game.h"
#include <algorithm>

static constexpr float kMessageSeconds = 3.5f;
static constexpr float kSpeechSeconds = 5.0f;

Game::Game(const Level& level) : m_level(level) {
    for (size_t i = 0; i < level.entities.size(); ++i)
        m_index[level.entities[i].id] = i;
    reset();
}

void Game::reset() {
    m_state.assign(m_level.entities.size(), EntityState{});
    for (size_t i = 0; i < m_level.entities.size(); ++i) {
        const Interaction& in = m_level.entities[i].interaction;
        if (in.type == "container") {
            m_state[i].unlocked = !in.locked;
            const int inside = findEntity(in.contains);
            if (inside >= 0)
                m_state[inside].hidden = true;
        }
        // Exits always start locked; a keypad or the right item opens them.
    }
    m_inventory.clear();
    m_readables.clear();
    m_selected = -1;
    m_mode = Mode::Walking;
    m_activeKeypad = -1;
    m_activeDice = -1;
    m_rollCount = 0;
    m_readingText.clear();
    m_message.clear();
    m_messageTimeLeft = 0.0f;
    m_speaker.clear();
    m_speech.clear();
    m_speechTimeLeft = 0.0f;
    m_elapsed = 0.0f;
}

void Game::update(float dt) {
    if (m_mode != Mode::Won)
        m_elapsed += dt;
    m_messageTimeLeft = std::max(0.0f, m_messageTimeLeft - dt);
    m_speechTimeLeft = std::max(0.0f, m_speechTimeLeft - dt);
}

void Game::closePopup() {
    if (m_mode != Mode::Won)
        m_mode = Mode::Walking;
    m_activeKeypad = -1;
    m_activeDice = -1;
}

void Game::openInventory() {
    if (m_mode == Mode::Walking)
        m_mode = Mode::Inventory;
}

std::string Game::actionLabel(size_t entity) const {
    const Entity& e = m_level.entities[entity];
    const Interaction& in = e.interaction;
    const EntityState& s = m_state[entity];
    if (s.hidden || in.type.empty())
        return "";

    const std::string held = heldItem();
    const bool needsItem = (in.type == "container" || in.type == "exit") && !s.unlocked;
    if (needsItem && !held.empty())
        return "Use " + m_level.itemName(held) + " on " + e.id;

    if (in.type == "pickup")
        return "Pick up " + m_level.itemName(in.item);
    if (in.type == "container") {
        if (s.opened)
            return "Search " + e.id;
        return s.unlocked ? "Open " + e.id : "Open " + e.id + " (locked)";
    }
    if (in.type == "readable")
        return "Read " + e.id;
    if (in.type == "keypad")
        return "Use " + e.id;
    if (in.type == "exit")
        return s.unlocked ? "Open " + e.id : "Open " + e.id + " (locked)";
    if (in.type == "npc")
        return "Talk to " + in.name;
    if (in.type == "dice")
        return "Roll " + e.id;
    return "";
}

bool Game::tryUnlock(size_t entity, std::string& unlockMessage) {
    const Entity& e = m_level.entities[entity];
    const Interaction& in = e.interaction;
    EntityState& s = m_state[entity];
    if (s.unlocked)
        return true;

    const std::string held = heldItem();
    if (!in.unlockedBy.empty() && held == in.unlockedBy) {
        s.unlocked = true;
        unlockMessage = "You unlock the " + e.id + " with the " + m_level.itemName(held) + ". ";
        return true;
    }
    if (!held.empty())
        showMessage("The " + m_level.itemName(held) + " doesn't work on the " + e.id + ".");
    else if (!in.unlockedBy.empty())
        showMessage("The " + e.id + " is locked. Hold an item (1-9) to use it here.");
    else
        showMessage("The " + e.id + " is locked.");
    return false;
}

void Game::interact(size_t entity) {
    if (m_mode != Mode::Walking || m_state[entity].hidden)
        return;
    const Entity& e = m_level.entities[entity];
    const Interaction& in = e.interaction;
    EntityState& s = m_state[entity];

    if (in.type == "pickup") {
        s.hidden = true;
        addItem(in.item);
        showMessage("Picked up: " + m_level.itemName(in.item));
    } else if (in.type == "container") {
        if (s.opened) {
            showMessage("The " + e.id + " is empty.");
            return;
        }
        std::string prefix;
        if (!tryUnlock(entity, prefix))
            return;
        s.opened = true;
        showMessage(prefix + takeContents(e));
    } else if (in.type == "readable") {
        read(in.text);
    } else if (in.type == "keypad") {
        m_activeKeypad = (int)entity;
        m_mode = Mode::Keypad;
    } else if (in.type == "exit") {
        std::string prefix;
        if (!tryUnlock(entity, prefix))
            return;
        if (m_level.winExit.empty() || m_level.winExit == e.id)
            m_mode = Mode::Won;
        else
            showMessage(prefix + "The " + e.id + " opens.");
    } else if (in.type == "npc") {
        if (!in.lines.empty())
            say(in.name, in.lines[s.talks % in.lines.size()]);
        if (s.talks == 0 && !in.gives.empty()) {
            addItem(in.gives);
            showMessage(in.name + " gave you: " + m_level.itemName(in.gives));
        }
        ++s.talks;
    } else if (in.type == "dice") {
        m_activeDice = (int)entity;
        m_mode = Mode::Dice;
        rollDice();
    }
}

// Gives the player what is inside an opened container. Returns the message.
std::string Game::takeContents(const Entity& container) {
    const std::string& contents = container.interaction.contains;
    if (contents.empty())
        return "The " + container.id + " is empty.";

    const int inside = findEntity(contents);
    if (inside < 0) {
        addItem(contents);
        return "You found: " + m_level.itemName(contents);
    }

    const Entity& found = m_level.entities[inside];
    if (found.interaction.type == "pickup") {
        addItem(found.interaction.item);
        return "You found: " + m_level.itemName(found.interaction.item);
    }
    if (found.interaction.type == "readable") {
        addItem(found.id);
        m_readables[found.id] = found.interaction.text;
        read(found.interaction.text);
        return "You found: " + m_level.itemName(found.id) + " (Tab to read it again)";
    }
    // Anything else is placed in the room for the player to use.
    m_state[inside].hidden = false;
    return "Something is revealed: " + found.id;
}

// The code a keypad wants right now. For a dice keypad that is the total of
// the last roll, or "" if the dice were never rolled (no code matches then).
std::string Game::expectedCode(const Interaction& keypad) const {
    if (keypad.codeFrom.empty())
        return keypad.code;
    const int dice = findEntity(keypad.codeFrom);
    if (dice < 0 || m_state[dice].dice.empty())
        return "";
    int total = 0;
    for (int v : m_state[dice].dice)
        total += v;
    return std::to_string(total);
}

bool Game::submitCode(const std::string& code) {
    if (m_mode != Mode::Keypad || m_activeKeypad < 0)
        return false;
    const Interaction& in = m_level.entities[m_activeKeypad].interaction;
    const std::string expected = expectedCode(in);
    if (expected.empty() || code != expected) {
        showMessage("Wrong code.");
        return false;
    }
    const int target = findEntity(in.opens);
    if (target >= 0)
        m_state[target].unlocked = true;
    showMessage("Beep! The " + in.opens + " is unlocked.");
    closePopup();
    return true;
}

void Game::selectSlot(int slot) {
    if (slot < 0 || slot >= (int)m_inventory.size())
        return;
    m_selected = (m_selected == slot) ? -1 : slot;
}

std::string Game::heldItem() const {
    return m_selected >= 0 ? m_inventory[m_selected] : "";
}

void Game::rollDice() {
    if (m_mode != Mode::Dice || m_activeDice < 0)
        return;
    const Interaction& in = m_level.entities[m_activeDice].interaction;
    std::uniform_int_distribution<int> face(1, in.sides);
    std::vector<int>& values = m_state[m_activeDice].dice;
    values.assign(in.count, 0);
    for (int& v : values)
        v = face(m_rng);
    ++m_rollCount;
}

const std::vector<int>& Game::diceValues() const {
    static const std::vector<int> none;
    return m_activeDice >= 0 ? m_state[m_activeDice].dice : none;
}

int Game::diceSides() const {
    return m_activeDice >= 0 ? m_level.entities[m_activeDice].interaction.sides : 6;
}

std::string Game::readableText(const std::string& item) const {
    auto it = m_readables.find(item);
    return it != m_readables.end() ? it->second : "";
}

void Game::read(const std::string& text) {
    m_readingText = text;
    m_mode = Mode::Reading;
}

int Game::findEntity(const std::string& id) const {
    auto it = m_index.find(id);
    return it != m_index.end() ? (int)it->second : -1;
}

void Game::addItem(const std::string& item) {
    if (!item.empty() && std::find(m_inventory.begin(), m_inventory.end(), item) == m_inventory.end())
        m_inventory.push_back(item);
}

void Game::showMessage(const std::string& text) {
    m_message = text;
    m_messageTimeLeft = kMessageSeconds;
}

void Game::say(const std::string& speaker, const std::string& line) {
    m_speaker = speaker;
    m_speech = line;
    m_speechTimeLeft = kSpeechSeconds;
}
