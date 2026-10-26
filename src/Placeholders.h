#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

// Stand-in boxes for 3D models, until the engine can load real model files.
// Each model name maps to one or more colored boxes in the entity's local space.
struct PlaceholderBox {
    glm::vec3 offset;   // box center, relative to the entity position
    glm::vec3 size;
    glm::vec3 color;
};

// Unknown model names get a single magenta box so they are easy to spot.
const std::vector<PlaceholderBox>& placeholderFor(const std::string& model);

bool hasPlaceholder(const std::string& model);

// Every model name that has a placeholder, sorted (the editor's model palette).
std::vector<std::string> placeholderModels();
