#include "Mesh.h"
#include <glm/glm.hpp>

static constexpr int kFloatsPerVertex = 6;

Mesh::Mesh(const std::vector<float>& vertices)
    : m_vertexCount((GLsizei)(vertices.size() / kFloatsPerVertex)) {
    // VAO = remembers the vertex layout. VBO = the vertex data on the GPU.
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    const GLsizei stride = kFloatsPerVertex * sizeof(float);
    // location 0 = aPos: 3 floats at the start of each vertex
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    // location 1 = aNormal: 3 floats after the position
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

Mesh::~Mesh() {
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
}

void Mesh::draw() const {
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
}

std::vector<float> cubeVertices() {
    // Each face is described by its normal n and two edge directions u, v with
    // cross(u, v) == n, so the triangles come out counter-clockwise (front face).
    struct Face { glm::vec3 n, u, v; };
    const Face faces[6] = {
        {{ 1, 0, 0}, { 0, 0, -1}, {0, 1,  0}},
        {{-1, 0, 0}, { 0, 0,  1}, {0, 1,  0}},
        {{ 0, 1, 0}, { 1, 0,  0}, {0, 0, -1}},
        {{ 0,-1, 0}, { 1, 0,  0}, {0, 0,  1}},
        {{ 0, 0, 1}, { 1, 0,  0}, {0, 1,  0}},
        {{ 0, 0,-1}, {-1, 0,  0}, {0, 1,  0}},
    };
    // Two triangles per face, as (u, v) corner signs.
    const float corners[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};

    std::vector<float> out;
    out.reserve(6 * 6 * kFloatsPerVertex);
    for (const Face& f : faces) {
        for (const auto& c : corners) {
            const glm::vec3 p = 0.5f * (f.n + c[0] * f.u + c[1] * f.v);
            out.insert(out.end(), {p.x, p.y, p.z, f.n.x, f.n.y, f.n.z});
        }
    }
    return out;
}
