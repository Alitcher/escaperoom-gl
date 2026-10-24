# Escape Room Engine

A from-scratch **3D game engine in C++ / OpenGL** — a small **escape-room creation
platform**: build rooms in a level editor, save them as **JSON**, and the engine
loads & plays any of them. A personal project exploring real-time 3D graphics and
engine architecture.

See **`ROADMAP.md`** for the plan and **`LEVEL_FORMAT.md`** for the level format
(`levels/sample-room.json` is an example).

> Right now it loads `levels/sample-room.json` and the room is playable from
> start to escape. Props are colored placeholder boxes until a model loader
> exists. See **`DEVLOG.md`** for progress and the feature list.

## Stack
C++17 · **OpenGL 4.6 core** · GLFW (window/input) · **GLEW** (GL function loader) ·
glm (math) · Dear ImGui + ImGuizmo (editor) · nlohmann/json (levels) · CMake + vcpkg.

> **GLFW vs GLEW:** GLFW opens the window + GL context + handles input; **GLEW**
> loads the modern OpenGL functions (they live in the GPU driver and must be looked
> up at runtime). You need both, but only one loader — GLEW here.
>
> **Requires** a GPU/driver supporting **OpenGL 4.6** — any modern NVIDIA / AMD /
> Intel GPU on **Windows or Linux**. (macOS caps at OpenGL 4.1 — use Win/Linux, or
> plan the Vulkan/Metal path for Mac.)

---

## One-time setup (Windows)

### 1. Install the C++ toolchain
Install **Visual Studio 2022 Community** and tick the **"Desktop development with
C++"** workload (gives you the MSVC compiler + CMake). VS Code + "CMake Tools" +
"C/C++" extensions also works.

### 2. Install vcpkg (handles the libraries)
```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
```
Dependencies (GLFW, **GLEW**, glm, stb) are listed in `vcpkg.json`, so vcpkg
installs them automatically when CMake configures (manifest mode). No code
generation and no Python — GLEW is a normal C++ library.

### 3. Configure + build
```powershell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```
(First configure is slow — vcpkg compiles GLFW/GLEW/glm once, then caches them.)

### 4. Run
```powershell
.\build\Debug\engine.exe
```
You should be standing inside the green room. Controls: **mouse** look,
**WASD / arrow keys** walk, **Shift** run, **E / left click** use the thing
under the crosshair, **Tab** inventory, **R** restart, **Esc** close popup or
quit. The console prints your OpenGL
version (should be **4.6**). To load another level:
`.\build\Debug\engine.exe path\to\level.json`.

> **Tip (VS Code):** create `.vscode/settings.json` with
> `{ "cmake.configureSettings": { "CMAKE_TOOLCHAIN_FILE": "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" } }`
> so the CMake Tools extension picks up vcpkg automatically.

---

## Next steps
Follow `ROADMAP.md` — the next milestones are: a triangle → shaders → textures →
3D cube → FPS camera → lighting → the escape-room puzzles.

## Learn alongside
- **LearnOpenGL.com** — the milestones follow it.
- **The Cherno** (YouTube) — C++ game-engine architecture.
