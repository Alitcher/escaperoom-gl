#pragma once
#include "Camera.h"
#include "Game.h"
#include "Level.h"
#include "Scene.h"
#include "Ui.h"

struct GLFWwindow;
class IconCache;
class Renderer;

// Playing one level: first-person walking with collision, picking what is
// under the crosshair, game rules and game UI. Made fresh for every playtest.
class PlaySession {
public:
    explicit PlaySession(const Level& level);   // `level` must outlive the session

    // Input, rules and UI for one frame (between ImGui::NewFrame and Render).
    // Returns true when the player asks to quit.
    bool frame(GLFWwindow* window, float dt, IconCache& icons);

    // Draws the 3D room into the whole window.
    void render(Renderer& renderer, GLFWwindow* window);

    // Gives the mouse back (call when leaving play mode).
    void releaseMouse(GLFWwindow* window);

    const FpsCamera& camera() const { return m_camera; }

private:
    void respawn();
    bool playerBlocked(const glm::vec3& eye) const;
    void moveWithCollision(const glm::vec3& delta);
    int pickEntity() const;

    const Level& m_level;
    Scene m_scene;
    Game m_game;
    UiState m_ui;
    FpsCamera m_camera;
    int m_hovered = -1;

    double m_lastMouseX = 0.0, m_lastMouseY = 0.0;
    bool m_cursorLocked = false;
    bool m_mouseWasDown = false;
};
