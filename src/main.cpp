// Escape Room Engine: play a level, or build one in the editor.
//
// Usage: engine [--edit] [path/to/level.json]   (default: levels/sample-room.json)
//   --edit   start in the level editor instead of playing
// F5 switches between playing and editing at any time.
#include <GL/glew.h>       // GL function loader - MUST be included before GLFW
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dwmapi.h>
#endif
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

#include "Editor.h"
#include "Icons.h"
#include "Level.h"
#include "PlaySession.h"
#include "Renderer.h"
#include "Theme.h"

// Windows 11 lets an app color its own title bar: navy with white text, like a
// Win95 window. Older Windows just ignores this and keeps its normal title bar.
static void paintTitleBar(GLFWwindow* window) {
#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(window);
    const COLORREF navy = RGB(0, 0, 128);
    const COLORREF white = RGB(255, 255, 255);
    DwmSetWindowAttribute(hwnd, 35 /* DWMWA_CAPTION_COLOR */, &navy, sizeof(navy));
    DwmSetWindowAttribute(hwnd, 36 /* DWMWA_TEXT_COLOR */, &white, sizeof(white));
    DwmSetWindowAttribute(hwnd, 34 /* DWMWA_BORDER_COLOR */, &navy, sizeof(navy));
#else
    (void)window;
#endif
}

int main(int argc, char** argv) {
    bool startInEditor = false;
    std::string levelPath = std::string(LEVELS_DIR) + "/sample-room.json";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--edit")
            startInEditor = true;
        else
            levelPath = arg;
    }

    Level level;
    std::string error;
    if (!loadLevel(levelPath, level, error)) {
        std::cerr << "Failed to load level: " << error << "\n";
        return -1;
    }
    std::cout << "Loaded \"" << level.name << "\" by " << level.author << ": "
              << level.entities.size() << " entities, " << level.lights.size() << " lights\n";

    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }

    // Ask for an OpenGL 4.6 core profile context. (Windows/Linux - macOS caps at 4.1.)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1600, 900, "Escape Room Engine", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    paintTitleBar(window);
    glfwSwapInterval(1); // vsync
    glfwFocusWindow(window);   // make sure keys go to the game, not the console

    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

    // Load OpenGL functions via GLEW. (glewExperimental is required for core profiles.)
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to init GLEW\n";
        glfwTerminate();
        return -1;
    }

    std::cout << "OpenGL " << glGetString(GL_VERSION) << "\n";

    // Dear ImGui for the game UI and the editor.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;                                // don't write imgui.ini
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;  // we control the cursor
    applyRetroTheme();   // Windows 98 look, like the portfolio site
    ImGui::GetStyle().ScaleAllSizes(1.4f);
    ImGui::GetStyle().FontScaleMain = 1.4f;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    glEnable(GL_DEPTH_TEST);   // nearer surfaces hide farther ones
    glEnable(GL_CULL_FACE);    // skip the back sides of boxes

    {   // Scope so GL objects are freed before the context is destroyed.
        Renderer renderer;
        if (!renderer.valid()) {
            glfwTerminate();
            return -1;
        }
        IconCache icons;
        Editor editor(level, levelPath, icons);

        // Playing = a PlaySession exists. It is rebuilt from the level every
        // time play starts, so edits always show up in the playtest.
        std::unique_ptr<PlaySession> play;
        bool usedEditor = startInEditor;
        if (!startInEditor)
            play = std::make_unique<PlaySession>(level);

        auto stopPlaying = [&]() {
            play->releaseMouse(window);
            play.reset();
        };

        double lastTime = glfwGetTime();
        double lastTitleUpdate = 0.0;

        while (!glfwWindowShouldClose(window)) {
            const double now = glfwGetTime();
            const float dt = (float)(now - lastTime);
            lastTime = now;

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGuizmo::BeginFrame();

            const bool toggle = ImGui::IsKeyPressed(ImGuiKey_F5, false);
            if (play) {
                const bool wantQuit = play->frame(window, dt, icons);
                if (toggle || (wantQuit && usedEditor)) {
                    stopPlaying();   // back to the editor
                    usedEditor = true;
                } else if (wantQuit) {
                    glfwSetWindowShouldClose(window, true);
                }
            } else {
                if (editor.frame(window, dt) == Editor::Action::Play || toggle)
                    play = std::make_unique<PlaySession>(level);
            }

            // Closing with unsaved editor changes: show the editor and ask first.
            if (glfwWindowShouldClose(window) && editor.dirty() && !editor.quitConfirmed()) {
                glfwSetWindowShouldClose(window, false);
                if (play)
                    stopPlaying();
                editor.askToQuit();
            }
            if (editor.quitConfirmed())
                glfwSetWindowShouldClose(window, true);

            // Title bar: level + mode. While playing, the position is an easy
            // check that walking works.
            if (now - lastTitleUpdate > 0.25) {
                lastTitleUpdate = now;
                char buf[256];
                if (play)
                    std::snprintf(buf, sizeof(buf), "Escape Room Engine - %s   (x %.1f, z %.1f)",
                                  level.name.c_str(), play->camera().position.x, play->camera().position.z);
                else
                    std::snprintf(buf, sizeof(buf), "Escape Room Engine - Editor - %s%s",
                                  levelPath.c_str(), editor.dirty() ? " *" : "");
                glfwSetWindowTitle(window, buf);
            }

            int width = 0, height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            glViewport(0, 0, width, height);
            glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (play)
                play->render(renderer, window);
            else
                editor.render(renderer);

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
