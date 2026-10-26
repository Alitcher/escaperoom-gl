cmake_minimum_required(VERSION 3.21)
project(EscapeRoomEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# All from vcpkg - no code generation needed.
find_package(glfw3 CONFIG REQUIRED)          # window + context + input
find_package(GLEW REQUIRED)                  # OpenGL function loader
find_package(OpenGL REQUIRED)                # the system OpenGL lib
find_package(glm CONFIG REQUIRED)            # math
find_package(nlohmann_json CONFIG REQUIRED)  # level JSON
find_package(imgui CONFIG REQUIRED)          # game UI + editor panels
find_package(imguizmo CONFIG REQUIRED)       # editor move / rotate / scale handles
find_package(Stb REQUIRED)                   # stb_image: item icons

add_executable(engine
    src/main.cpp
    src/Shader.cpp
    src/Mesh.cpp
    src/Level.cpp
    src/Placeholders.cpp
    src/Collision.cpp
    src/Game.cpp
    src/Ui.cpp
    src/Icons.cpp
    src/Scene.cpp
    src/Renderer.cpp
    src/PlaySession.cpp
    src/Editor.cpp
    src/Theme.cpp
)

# Where the default level lives, so the exe finds it from any working folder.
target_compile_definitions(engine PRIVATE LEVELS_DIR="${CMAKE_SOURCE_DIR}/levels")
target_include_directories(engine PRIVATE ${Stb_INCLUDE_DIR})

target_link_libraries(engine PRIVATE
    glfw
    GLEW::GLEW
    OpenGL::GL
    glm::glm
    nlohmann_json::nlohmann_json
    imgui::imgui
    imguizmo::imguizmo
)

# Windows: dwmapi colors the window's title bar (navy, Win95 style).
if(WIN32)
    target_link_libraries(engine PRIVATE dwmapi)
endif()
