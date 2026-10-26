#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>

// A linked vertex + fragment shader program.
class Shader {
public:
    // Compiles and links both stages. Check valid() afterwards; errors go to stderr.
    Shader(const char* vertexSrc, const char* fragmentSrc);
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    bool valid() const { return m_program != 0; }
    void use() const;

    void setInt(const char* name, int value) const;
    void setFloat(const char* name, float value) const;
    void setVec3(const char* name, const glm::vec3& value) const;
    void setMat4(const char* name, const glm::mat4& value) const;

private:
    GLint location(const char* name) const;

    GLuint m_program = 0;
};
