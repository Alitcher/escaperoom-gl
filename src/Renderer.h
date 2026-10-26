#pragma once
#include "Level.h"
#include "Mesh.h"
#include "Scene.h"
#include "Shader.h"
#include <functional>

// Draws lit boxes. The caller sets the viewport and clears; begin() sets the
// camera and the level's lights for everything drawn after it.
class Renderer {
public:
    Renderer();
    bool valid() const { return m_shader.valid(); }

    void begin(const glm::mat4& view, const glm::mat4& proj, const Level& level);

    // `unlit` = flat color, for editor markers.
    void drawBox(const glm::mat4& model, const glm::vec3& color, bool unlit = false);

    // Every entity that passes `visible`; `tint` may change each box's color.
    void drawScene(const Scene& scene, const EntityFilter& visible,
                   const std::function<glm::vec3(size_t entity, glm::vec3 color)>& tint);

private:
    Shader m_shader;
    Mesh m_cube;
};
