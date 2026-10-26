#include "Shader.h"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

// Compiles one shader stage. Returns 0 and prints the error log on failure.
static GLuint compileStage(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error:\n" << log << "\n";
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

Shader::Shader(const char* vertexSrc, const char* fragmentSrc) {
    GLuint vs = compileStage(GL_VERTEX_SHADER, vertexSrc);
    GLuint fs = compileStage(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!vs || !fs) {
        glDeleteShader(vs);
        glDeleteShader(fs);
        return;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    // The program keeps its own copy, so the stages can be freed now.
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Shader link error:\n" << log << "\n";
        glDeleteProgram(program);
        return;
    }
    m_program = program;
}

Shader::~Shader() {
    glDeleteProgram(m_program);
}

void Shader::use() const {
    glUseProgram(m_program);
}

// Looked up by name on every call. Fine for a few objects; cache it if it ever
// shows up in profiling.
GLint Shader::location(const char* name) const {
    return glGetUniformLocation(m_program, name);
}

void Shader::setInt(const char* name, int value) const {
    glUniform1i(location(name), value);
}

void Shader::setFloat(const char* name, float value) const {
    glUniform1f(location(name), value);
}

void Shader::setVec3(const char* name, const glm::vec3& value) const {
    glUniform3fv(location(name), 1, glm::value_ptr(value));
}

void Shader::setMat4(const char* name, const glm::mat4& value) const {
    glUniformMatrix4fv(location(name), 1, GL_FALSE, glm::value_ptr(value));
}
