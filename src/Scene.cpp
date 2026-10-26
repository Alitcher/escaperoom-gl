#include "Scene.h"
#include "Placeholders.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <algorithm>

glm::mat4 entityMatrix(const Transform& t) {
    const glm::vec3 r = glm::radians(t.rotationDeg);
    return glm::translate(glm::mat4(1.0f), t.position) *
           glm::eulerAngleXYZ(r.x, r.y, r.z) *
           glm::scale(glm::mat4(1.0f), t.scale);
}

// Moves each angle by whole turns (360) so it lands as close as possible to `near`.
static glm::vec3 wrapToward(glm::vec3 angles, const glm::vec3& near) {
    for (int i = 0; i < 3; ++i)
        angles[i] += 360.0f * std::round((near[i] - angles[i]) / 360.0f);
    return angles;
}

static float distance(const glm::vec3& a, const glm::vec3& b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z);
}

Transform transformFromMatrix(const glm::mat4& m, const glm::vec3& previousRotationDeg) {
    Transform t;
    t.position = glm::vec3(m[3]);
    glm::mat4 rotation(1.0f);
    for (int i = 0; i < 3; ++i) {
        const glm::vec3 axis(m[i]);
        t.scale[i] = glm::length(axis);
        rotation[i] = glm::vec4(axis / std::max(t.scale[i], 1e-6f), 0.0f);
    }
    float x = 0.0f, y = 0.0f, z = 0.0f;
    glm::extractEulerAngleXYZ(rotation, x, y, z);

    // For X-Y-Z angles, (x, y, z) and (x + 180, 180 - y, z + 180) are the same rotation.
    const glm::vec3 a = wrapToward(glm::degrees(glm::vec3(x, y, z)), previousRotationDeg);
    const glm::vec3 b = wrapToward(glm::degrees(glm::vec3(x, 0.0f, z)) + glm::vec3(180.0f, 180.0f - glm::degrees(y), 180.0f),
                                   previousRotationDeg);
    t.rotationDeg = distance(a, previousRotationDeg) <= distance(b, previousRotationDeg) ? a : b;
    return t;
}

std::vector<int> parentIndices(const Level& level) {
    const size_t n = level.entities.size();
    std::vector<int> parents(n, -1);
    for (size_t i = 0; i < n; ++i) {
        const int p = level.indexOf(level.entities[i].parent);
        // Walk up from p; reaching i again (or walking forever) means a loop.
        int walk = p;
        size_t steps = 0;
        while (walk >= 0 && walk != (int)i && steps++ <= n)
            walk = level.indexOf(level.entities[walk].parent);
        parents[i] = (walk == (int)i || steps > n) ? -1 : p;
    }
    return parents;
}

std::vector<glm::mat4> worldMatrices(const Level& level) {
    const size_t n = level.entities.size();
    const std::vector<int> parents = parentIndices(level);
    std::vector<glm::mat4> world(n);
    std::vector<bool> done(n, false);
    // Parents first: resolve each chain from the top down.
    for (size_t i = 0; i < n; ++i) {
        std::vector<int> chain;
        for (int k = (int)i; k >= 0 && !done[k]; k = parents[k])
            chain.push_back(k);
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            const int k = *it;
            const glm::mat4 parentWorld = parents[k] >= 0 ? world[parents[k]] : glm::mat4(1.0f);
            world[k] = parentWorld * entityMatrix(level.entities[k].transform);
            done[k] = true;
        }
    }
    return world;
}

Scene buildScene(const Level& level) {
    Scene scene(level.entities.size());
    const std::vector<glm::mat4> world = worldMatrices(level);
    for (size_t i = 0; i < level.entities.size(); ++i) {
        const Entity& e = level.entities[i];
        if (e.model.empty())
            continue;
        const glm::mat4& base = world[i];
        for (const PlaceholderBox& box : placeholderFor(e.model)) {
            const glm::mat4 model = glm::scale(glm::translate(base, box.offset), box.size);
            scene[i].push_back({model, box.color, boundsOfUnitCube(model)});
        }
    }
    return scene;
}

int raycast(const Scene& scene, const glm::vec3& origin, const glm::vec3& dir, float maxDist,
            const EntityFilter& canHit, float* distOut) {
    float nearest = maxDist;
    int hit = -1;
    for (size_t i = 0; i < scene.size(); ++i) {
        if (!canHit(i))
            continue;
        for (const SceneBox& box : scene[i]) {
            float t = 0.0f;
            if (rayHits(origin, dir, box.bounds, t) && t < nearest) {
                nearest = t;
                hit = (int)i;
            }
        }
    }
    if (distOut)
        *distOut = nearest;
    return hit;
}

bool overlapsAny(const Scene& scene, const Aabb& box, const EntityFilter& solid) {
    for (size_t i = 0; i < scene.size(); ++i) {
        if (!solid(i))
            continue;
        for (const SceneBox& b : scene[i])
            if (overlaps(box, b.bounds))
                return true;
    }
    return false;
}
