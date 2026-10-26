#include "Collision.h"
#include <algorithm>
#include <cmath>
#include <limits>

Aabb boundsOfUnitCube(const glm::mat4& model) {
    Aabb box{glm::vec3(std::numeric_limits<float>::max()), glm::vec3(std::numeric_limits<float>::lowest())};
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 corner((i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f, (i & 4) ? 0.5f : -0.5f);
        const glm::vec3 p = glm::vec3(model * glm::vec4(corner, 1.0f));
        box.min = glm::min(box.min, p);
        box.max = glm::max(box.max, p);
    }
    return box;
}

bool overlaps(const Aabb& a, const Aabb& b) {
    return a.min.x < b.max.x && a.max.x > b.min.x &&
           a.min.y < b.max.y && a.max.y > b.min.y &&
           a.min.z < b.max.z && a.max.z > b.min.z;
}

// Slab method: clip the ray against the 3 pairs of parallel planes.
bool rayHits(const glm::vec3& origin, const glm::vec3& dir, const Aabb& box, float& tOut) {
    float tNear = 0.0f;
    float tFar = std::numeric_limits<float>::max();
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(dir[axis]) < 1e-6f) {
            // Parallel to this slab: miss unless the origin is inside it.
            if (origin[axis] < box.min[axis] || origin[axis] > box.max[axis])
                return false;
            continue;
        }
        float t1 = (box.min[axis] - origin[axis]) / dir[axis];
        float t2 = (box.max[axis] - origin[axis]) / dir[axis];
        if (t1 > t2)
            std::swap(t1, t2);
        tNear = std::max(tNear, t1);
        tFar = std::min(tFar, t2);
        if (tNear > tFar)
            return false;
    }
    tOut = tNear;
    return true;
}
