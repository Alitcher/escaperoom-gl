#include "PlaySession.h"
#include "Icons.h"
#include "Renderer.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

static constexpr float kPlayerRadius = 0.25f;   // half-width of the player's box
static constexpr float kPlayerHeight = 1.8f;
static constexpr float kReach = 2.5f;           // how far away things can be used

PlaySession::PlaySession(const Level& level)
    : m_level(level), m_scene(buildScene(level)), m_game(level) {
    respawn();
}

void PlaySession::respawn() {
    m_camera.position = m_level.spawnPosition;
    m_camera.yawDeg = m_level.spawnYawDeg;
    m_camera.pitchDeg = 0.0f;
}

// True if a player with eyes at `eye` would be inside something.
bool PlaySession::playerBlocked(const glm::vec3& eye) const {
    // Feet start a little above 0 so the floor itself does not count.
    const Aabb player{{eye.x - kPlayerRadius, 0.05f, eye.z - kPlayerRadius},
                      {eye.x + kPlayerRadius, kPlayerHeight, eye.z + kPlayerRadius}};
    return overlapsAny(m_scene, player, [this](size_t i) { return m_game.isVisible(i); });
}

// Moves one axis at a time, so walking into a wall at an angle slides along it.
void PlaySession::moveWithCollision(const glm::vec3& delta) {
    // If we somehow start inside something, let the player walk out.
    if (playerBlocked(m_camera.position)) {
        m_camera.position += delta;
        return;
    }
    glm::vec3 next = m_camera.position;
    next.x += delta.x;
    if (!playerBlocked(next))
        m_camera.position.x = next.x;
    next = m_camera.position;
    next.z += delta.z;
    if (!playerBlocked(next))
        m_camera.position.z = next.z;
}

// The usable entity under the crosshair, or -1. Solid boxes in front block it.
int PlaySession::pickEntity() const {
    const int hit = raycast(m_scene, m_camera.position, glm::normalize(m_camera.forward()), kReach,
                            [this](size_t i) { return m_game.isVisible(i); });
    if (hit >= 0 && m_game.actionLabel((size_t)hit).empty())
        return -1;
    return hit;
}

bool PlaySession::frame(GLFWwindow* window, float dt, IconCache& icons) {
    ImGuiIO& io = ImGui::GetIO();
    bool wantQuit = false;

    // Walking: cursor hidden and locked, mouse turns the camera.
    // Popups: normal cursor so you can click buttons.
    const bool walking = m_game.mode() == Game::Mode::Walking;
    bool skipLook = false;
    if (walking != m_cursorLocked) {
        m_cursorLocked = walking;
        glfwSetInputMode(window, GLFW_CURSOR, walking ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        if (walking) {
            io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        } else {
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
            int w = 0, h = 0;
            glfwGetWindowSize(window, &w, &h);
            glfwSetCursorPos(window, w * 0.5, h * 0.5);
        }
        skipLook = true;   // the cursor jumps when the mode changes
    }

    double mouseX = 0.0, mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    if (walking && !skipLook)
        m_camera.look((float)(mouseX - m_lastMouseX), (float)(mouseY - m_lastMouseY));
    m_lastMouseX = mouseX;
    m_lastMouseY = mouseY;

    if (walking)
        moveWithCollision(m_camera.wantedMove(window, dt));

    m_hovered = walking ? pickEntity() : -1;

    // Use: E key or left click (click only counts on the press, not while held).
    const bool mouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool clicked = mouseDown && !m_mouseWasDown;
    m_mouseWasDown = mouseDown;
    if (walking && m_hovered >= 0 && (clicked || ImGui::IsKeyPressed(ImGuiKey_E, false)))
        m_game.interact((size_t)m_hovered);

    // 1-9: hold the item in that pocket slot (same key again puts it away).
    if (m_game.mode() == Game::Mode::Walking || m_game.mode() == Game::Mode::Inventory) {
        for (int slot = 0; slot < 9; ++slot)
            if (ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_1 + slot), false))
                m_game.selectSlot(slot);
    }

    bool restart = ImGui::IsKeyPressed(ImGuiKey_R, false) &&
                   (m_game.mode() == Game::Mode::Walking || m_game.mode() == Game::Mode::Won);
    if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
        if (m_game.mode() == Game::Mode::Walking)
            m_game.openInventory();
        else if (m_game.mode() == Game::Mode::Inventory)
            m_game.closePopup();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        if (m_game.mode() == Game::Mode::Walking || m_game.mode() == Game::Mode::Won)
            wantQuit = true;
        else
            m_game.closePopup();
    }

    m_game.update(dt);

    const UiAction action = drawGameUi(m_game, m_level, m_hovered, m_ui, icons);
    if (action == UiAction::Quit)
        wantQuit = true;
    if (action == UiAction::Restart)
        restart = true;
    if (restart) {
        m_game.reset();
        m_ui = UiState{};
        respawn();
    }
    return wantQuit;
}

void PlaySession::render(Renderer& renderer, GLFWwindow* window) {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    const float aspect = height > 0 ? (float)width / (float)height : 1.0f;

    glViewport(0, 0, width, height);
    glClearColor(0.08f, 0.10f, 0.16f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // The projection matrix also fixes the aspect ratio (no stretching).
    renderer.begin(m_camera.view(), glm::perspective(glm::radians(70.0f), aspect, 0.05f, 100.0f), m_level);
    renderer.drawScene(
        m_scene, [this](size_t i) { return m_game.isVisible(i); },
        [this](size_t i, glm::vec3 color) {
            if ((int)i == m_hovered)
                return glm::min(color * 1.3f + glm::vec3(0.1f), glm::vec3(1.0f));   // highlight
            if (m_game.isOpened(i))
                return color * 0.6f;   // opened containers look darker
            return color;
        });
}

void PlaySession::releaseMouse(GLFWwindow* window) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    m_cursorLocked = false;
}
