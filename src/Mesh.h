#pragma once
#include <GL/glew.h>
#include <vector>

// Vertex data on the GPU. Vertices are interleaved: position (3 floats) then
// normal (3 floats), drawn as a plain triangle list.
class Mesh {
public:
    explicit Mesh(const std::vector<float>& vertices);
    ~Mesh();
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void draw() const;

private:
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLsizei m_vertexCount = 0;
};

// A 1 x 1 x 1 cube centered on the origin, with one normal per face.
std::vector<float> cubeVertices();
