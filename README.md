# Escape Room Engine

A from-scratch **3D game engine in C++ / OpenGL** — a small **escape-room creation
platform**: build rooms in a level editor, save them as **JSON**, and the engine
loads & plays any of them. A personal project exploring real-time 3D graphics and
engine architecture.

See **`ROADMAP.md`** for the plan and **`LEVEL_FORMAT.md`** for the level format
(`levels/sample-room.json` is an example).

> Right now it loads `levels/sample-room.json` and the room is playable from
> start to escape, and it has a Unity-style **level editor** for building rooms.
> Props are colored placeholder boxes until a model loader exists. See
> **`DEVLOG.md`** for progress and the feature list.

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
under the crosshair, **1-9** hold an item from your pocket, **Tab** inventory,
**Space** roll the dice again, **R** restart, **F5** level editor, **Esc**
close popup or quit. The console prints your OpenGL
version (should be **4.6**). To load another level:
`.\build\Debug\engine.exe path\to\level.json`.

### 5. Level editor
```powershell
.\build\Debug\engine.exe --edit
```
Or press **F5** while playing. F5 (or the green Play button) playtests the
room; Esc or F5 in the playtest goes back to the editor.

| Panel | What it does |
|---|---|
| Menu bar | File (New, Open, Save, Save As, Play, Exit), Edit (Undo / Redo, Undo History, Cut / Copy / Paste, Duplicate, Delete, Select All, Rename), GameObject (Create Empty, 3D Object, Make / Clear Parent, Move To View), View (tools, Snap, Font), Window (show / hide panels), Help |
| Toolbar | Play, Move / Rotate / Scale, Local / World, Snap, Undo / Redo, Save, open a level |
| Status bar (bottom) | object count, selection count, last message, file path |
| Hierarchy (left) | parent / child tree of objects (drag one onto another to parent it); Level settings (spawn, win, lights); problems that would break the level |
| Scene (middle) | the room in 3D; click to select, drag the colored handles |
| Inspector (right) | Transform (Position / Rotation / Scale, X Y Z) and puzzle settings |
| Project (bottom) | 3D models and 2D items: drag them into the scene |

Scene controls (like Unity):

| Input | Does |
|---|---|
| Right drag (+ WASD / QE, Shift faster) | look around and fly |
| Alt or Ctrl + left drag | orbit around the selection (or what you look at) |
| Middle drag | pan |
| Mouse wheel, or Alt + right drag | zoom |
| Left click / left drag on empty space | select / box select (Shift adds) |
| F | focus on the selection |
| W / E / R | move / rotate / scale tool (Ctrl while dragging = snap) |
| Del, Ctrl+D | delete / duplicate (with children) |
| Ctrl+Z, Ctrl+Y, Ctrl+S | undo, redo, save |

Drop an item on a drawer to put it inside or make it the drawer's key; on a
door to make it the key; on an NPC to make it their gift; on the floor to leave
it lying there.

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
