#include "Renderer.h"
#include <string>

static const char* kVertexShaderSrc = R"(#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vWorldPos;
out vec3 vNormal;

void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    // Normals need the inverse-transpose so non-uniform scale doesn't bend them.
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uProj * uView * world;
}
)";

// Simple diffuse lighting: ambient + one directional light + a few point lights.
static const char* kFragmentShaderSrc = R"(#version 460 core
const int MAX_POINT_LIGHTS = 4;

in vec3 vWorldPos;
in vec3 vNormal;

uniform vec3 uColor;
uniform int  uUnlit;
uniform vec3 uAmbient;

uniform int  uHasDirLight;
uniform vec3 uDirLightDir;      // direction the light travels
uniform vec3 uDirLightColor;    // color * intensity

uniform int  uPointLightCount;
uniform vec3 uPointLightPos[MAX_POINT_LIGHTS];
uniform vec3 uPointLightColor[MAX_POINT_LIGHTS];

out vec4 FragColor;

void main() {
    if (uUnlit != 0) {
        FragColor = vec4(uColor, 1.0);
        return;
    }
    vec3 n = normalize(vNormal);
    vec3 light = uAmbient;

    if (uHasDirLight != 0)
        light += uDirLightColor * max(dot(n, normalize(-uDirLightDir)), 0.0);

    for (int i = 0; i < uPointLightCount; ++i) {
        vec3 toLight = uPointLightPos[i] - vWorldPos;
        float dist = length(toLight);
        float attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);
        light += uPointLightColor[i] * max(dot(n, toLight / dist), 0.0) * attenuation;
    }

    FragColor = vec4(uColor * light, 1.0);
}
)";

static constexpr int kMaxPointLights = 4;

Renderer::Renderer()
    : m_shader(kVertexShaderSrc, kFragmentShaderSrc), m_cube(cubeVertices()) {}

void Renderer::begin(const glm::mat4& view, const glm::mat4& proj, const Level& level) {
    m_shader.use();
    m_shader.setMat4("uProj", proj);
    m_shader.setMat4("uView", view);
    m_shader.setVec3("uAmbient", glm::vec3(0.25f));

    // Send the level's lights (extra point lights are ignored).
    int hasDir = 0;
    int pointCount = 0;
    for (const Light& l : level.lights) {
        const glm::vec3 color = l.color * l.intensity;
        if (l.type == Light::Type::Directional && !hasDir) {
            hasDir = 1;
            m_shader.setVec3("uDirLightDir", l.direction);
            m_shader.setVec3("uDirLightColor", color);
        } else if (l.type == Light::Type::Point && pointCount < kMaxPointLights) {
            const std::string i = std::to_string(pointCount);
            m_shader.setVec3(("uPointLightPos[" + i + "]").c_str(), l.position);
            m_shader.setVec3(("uPointLightColor[" + i + "]").c_str(), color);
            ++pointCount;
        }
    }
    m_shader.setInt("uHasDirLight", hasDir);
    m_shader.setInt("uPointLightCount", pointCount);
}

void Renderer::drawBox(const glm::mat4& model, const glm::vec3& color, bool unlit) {
    m_shader.setMat4("uModel", model);
    m_shader.setVec3("uColor", color);
    m_shader.setInt("uUnlit", unlit ? 1 : 0);
    m_cube.draw();
}

void Renderer::drawScene(const Scene& scene, const EntityFilter& visible,
                         const std::function<glm::vec3(size_t, glm::vec3)>& tint) {
    for (size_t i = 0; i < scene.size(); ++i) {
        if (!visible(i))
            continue;
        for (const SceneBox& box : scene[i])
            drawBox(box.model, tint(i, box.color));
    }
}
