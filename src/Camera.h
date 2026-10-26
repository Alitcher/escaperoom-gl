#pragma once
#include <GL/glew.h>       // must come before GLFW
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

// First-person camera: WASD walks on the floor plane, the mouse looks around.
// yaw 0 looks toward +z, yaw 180 toward -z (same meaning as "spawn.yaw" in a level).
class FpsCamera {
public:
    glm::vec3 position{0.0f, 1.6f, 0.0f};
    float yawDeg = 0.0f;
    float pitchDeg = 0.0f;
    float moveSpeed = 2.5f;          // meters per second
    float mouseSensitivity = 0.1f;   // degrees per pixel

    glm::vec3 forward() const {
        const float yaw = glm::radians(yawDeg);
        const float pitch = glm::radians(pitchDeg);
        return {std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw)};
    }

    glm::mat4 view() const {
        return glm::lookAt(position, position + forward(), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    // dx, dy = how many pixels the mouse moved since last frame.
    void look(float dx, float dy) {
        yawDeg -= dx * mouseSensitivity;
        pitchDeg -= dy * mouseSensitivity;   // screen y grows downward
        pitchDeg = std::clamp(pitchDeg, -89.0f, 89.0f);
    }

    // Reads WASD / arrow keys (+ Shift to run) and returns how far the player
    // wants to move this frame, along the floor only. The caller applies it,
    // so walls can block it.
    glm::vec3 wantedMove(GLFWwindow* window, float dt) const {
        const float yaw = glm::radians(yawDeg);
        const glm::vec3 flatForward(std::sin(yaw), 0.0f, std::cos(yaw));
        const glm::vec3 right = glm::normalize(glm::cross(flatForward, glm::vec3(0.0f, 1.0f, 0.0f)));

        auto down = [window](int a, int b) {
            return glfwGetKey(window, a) == GLFW_PRESS || glfwGetKey(window, b) == GLFW_PRESS;
        };
        glm::vec3 dir(0.0f);
        if (down(GLFW_KEY_W, GLFW_KEY_UP))    dir += flatForward;
        if (down(GLFW_KEY_S, GLFW_KEY_DOWN))  dir -= flatForward;
        if (down(GLFW_KEY_D, GLFW_KEY_RIGHT)) dir += right;
        if (down(GLFW_KEY_A, GLFW_KEY_LEFT))  dir -= right;
        if (glm::length(dir) < 0.001f)
            return glm::vec3(0.0f);

        float speed = moveSpeed;
        if (down(GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT))
            speed *= 2.0f;
        return glm::normalize(dir) * speed * dt;
    }
};
