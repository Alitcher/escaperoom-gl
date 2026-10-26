#pragma once
#include "Collision.h"
#include "Level.h"
#include <functional>
#include <vector>

// One placeholder box of an entity, ready to draw and to collide with.
struct SceneBox {
    glm::mat4 model;
    glm::vec3 color;
    Aabb bounds;
};

// Boxes per entity, in the same order as level.entities.
using Scene = std::vector<std::vector<SceneBox>>;

// Picks which entities a query looks at.
using EntityFilter = std::function<bool(size_t)>;

// Entity transform -> matrix: translate * Rx * Ry * Rz * scale.
glm::mat4 entityMatrix(const Transform& t);

// The inverse of entityMatrix (used after dragging a gizmo). One rotation has
// several Euler spellings, e.g. (0, -120, 0) == (-180, -60, 180); this picks
// the one closest to `previousRotationDeg`, so the Inspector numbers don't jump.
Transform transformFromMatrix(const glm::mat4& m, const glm::vec3& previousRotationDeg = glm::vec3(0.0f));

// Parent index of every entity, or -1 for top level. A missing parent or a
// parent loop (a -> b -> a) also counts as top level, so bad data can't hang.
std::vector<int> parentIndices(const Level& level);

// World matrix of every entity: parent world * own local transform.
std::vector<glm::mat4> worldMatrices(const Level& level);

Scene buildScene(const Level& level);

// Nearest entity hit by a ray within `maxDist`, or -1. `dir` must be normalized.
int raycast(const Scene& scene, const glm::vec3& origin, const glm::vec3& dir, float maxDist,
            const EntityFilter& canHit, float* distOut = nullptr);

bool overlapsAny(const Scene& scene, const Aabb& box, const EntityFilter& solid);
