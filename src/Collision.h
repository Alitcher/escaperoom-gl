#pragma once
#include <glm/glm.hpp>

// Axis-aligned bounding box in world space.
struct Aabb {
    glm::vec3 min;
    glm::vec3 max;
};

// World-space box around a unit cube transformed by `model` (works with rotation).
Aabb boundsOfUnitCube(const glm::mat4& model);

bool overlaps(const Aabb& a, const Aabb& b);

// Ray vs box. On hit returns true and the distance along `dir` in `tOut`.
// `dir` must be normalized.
bool rayHits(const glm::vec3& origin, const glm::vec3& dir, const Aabb& box, float& tOut);
