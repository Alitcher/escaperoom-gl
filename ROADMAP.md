# Escape Room Engine — Portfolio Roadmap

A from-scratch **3D game engine in modern C++ (C++17)** with **OpenGL** (then a
**Vulkan** stretch). It's a **data-driven escape-room creation platform**: players
build their own rooms in a **level editor**, each room is saved as a **JSON file**,
and the engine **loads & plays any of them**. So it's not one game — it's a tiny
UGC engine. A personal project driven by my interest in **real-time 3D graphics
and game engines** — and a portfolio piece showing real graphics programming, the
GPU pipeline, performance, clean engine architecture, **plus tools/editor
programming and serialization**.

> **Background bridge:** Unity already taught me 3D math, shaders, meshes, cameras,
> draw calls and performance. This project moves from *engine user* → *engine
> builder*, and from C#/HLSL → C++/GLSL.

---

## 1. Tech choices

- **Language:** C++17 (move to 20 later if you like).
- **Graphics API:** **OpenGL 4.6 core.** Modern GL — direct state access (DSA),
  compute shaders, SPIR-V — and the fastest way to *learn graphics* and ship the
  game. (Needs a Win/Linux GPU; macOS caps at 4.1.) Vulkan is ~10× the code —
  great for showing depth, but do it as a **second renderer backend** once the
  engine works (see §8 stretch). OpenGL now + a Vulkan slice later is a strong combo.
- **Build:** **CMake** + **vcpkg** (manifest mode) for dependencies.
- **Why not an existing engine:** the point is to *write the renderer*, so no Unity/Unreal.

## 2. Dependencies (all via vcpkg)
| Lib | Job |
|---|---|
| **GLFW** | window + input + GL context |
| **GLEW** | the GL **function loader** (modern GL functions live in the driver) |
| **glm** | vectors/matrices/quaternions (like Unity's math) |
| **stb_image** | load textures (PNG/JPG) |
| **tinygltf** or **Assimp** | load 3D models (glTF/OBJ) |
| **Dear ImGui** | debug panels + the **editor UI** |
| **ImGuizmo** | 3D move/rotate/scale **handles** in the editor |
| **nlohmann/json** | read/write the **level JSON** (serialization) |

> Everything installs via **vcpkg** (no code generation, no Python). Add the editor
> libs (ImGui, ImGuizmo, nlohmann/json) to `vcpkg.json` when you reach those
> milestones — keep the first build small (glfw + glew + glm).

## 3. Toolchain setup (Windows) — see `README.md`
Visual Studio 2022 (Desktop C++ workload) **or** VS Code + CMake Tools, plus
vcpkg. The starter scaffold already compiles a window — get that running first.

---

## 4. Engine architecture (build it in layers)
```
Platform      → Window, input, timing                 (GLFW)
Renderer      → Shader, Buffer, Mesh, Texture,
                Framebuffer, Camera, Renderer          (OpenGL)  ← the graphics meat
Resources     → asset loading + caching (models, textures, shaders)
Scene/World   → DATA-DRIVEN Entity + Transform + components (a light ECS)
Serialization → load/save a World ↔ level JSON          (nlohmann/json)
Editor        → ImGui panels + ImGuizmo handles: place/move props, wire puzzles, save
Runtime/Game  → play a loaded level (interaction, puzzles, win state)
UI            → ImGui (editor + in-game)
```
- The engine is **data-driven**: a *level* is just data (JSON). The **Editor**
  edits a `World` and saves it; **Play** loads a `World` from JSON and runs it.
  Same `World`, two modes — that's what makes it a creation *platform*, not one game.
- Keep the **Renderer behind an interface** (`IRenderer` / abstract `Shader`,
  `Mesh`, …) so a Vulkan backend can slot in later without touching game/editor
  code — the same "swap the implementation" idea as the cosplay-map repo's
  database layer.

---

## 5. Milestones (tick as you go)

### Graphics fundamentals (OpenGL)
- [ ] Window + input + game loop (scaffold ✔)
- [ ] First **triangle** (VAO/VBO + vertex/fragment **shaders**)
- [ ] Uniforms + animated color
- [ ] **Textures** (stb_image)
- [ ] **Transforms** with glm (model/view/projection)
- [ ] **3D**: depth test + spinning cube
- [ ] **FPS camera** (WASD + mouse-look) ← your Unity camera knowledge
- [ ] Multiple objects → a **room** (floor/walls)

### Engine + assets
- [ ] Abstractions: `Shader`, `Mesh`, `Texture`, `Camera`, `Renderer` classes
- [ ] **Model loading** (glTF/OBJ) for props (desk, key, door)
- [ ] Resource manager (load once, reuse)
- [ ] Light **ECS** (Entity + Transform + components)

### Data-driven levels & editor
- [ ] **Load a level from JSON** into a `World` and play it (Phase E0)
- [ ] **Save** a `World` back to JSON (Phase E1)
- [ ] ImGui **property panel** to edit entity fields (Phase E1)
- [ ] **Select + ImGuizmo** move/rotate/scale in 3D (Phase E2)
- [ ] **Prop palette** — add/delete props (Phase E2)
- [ ] **Puzzle-wiring** UI (links entities by id) (Phase E3)
- [ ] **Level browser** — load any community room (Phase E4)

### Lighting & rendering techniques (the strong signals)
- [ ] **Phong lighting** (ambient/diffuse/specular; point + directional)
- [ ] **Normal mapping**
- [ ] **Shadow mapping** (depth pass) ← big graphics credibility
- [ ] **Framebuffers + post-processing** (e.g. bloom, vignette, fog)
- [ ] **Skybox** (cubemap)
- [ ] Text rendering (for clues/UI)

### Game: the escape room
- [ ] Ray-pick interactables (click a drawer/key/keypad)
- [ ] Inventory + simple **puzzles** (find key → unlock; enter code → open door)
- [ ] Win state ("you escaped"), reset
- [ ] ImGui game UI + debug overlay

### Performance & polish
- [ ] Frame timing + FPS, GPU/CPU profiling
- [ ] Draw-call **batching**, frustum culling (Unity concepts!)
- [ ] CMake `install` / package a runnable build

---

## 6. The escape room — cosplay themed (keep it tiny but complete)
**Setting:** a **convention green room / cosplay dressing room.** You're a cosplayer
locked in before the con — find your costume pieces and get to the stage door.
Player looks around (FPS camera). Goal: **escape onto the con floor.**

Same mechanics, cosplay set-dressing (theme is free — zero extra engineering):
- A **drawer** (click → opens → reveals a **wig / prop key**).
- That unlocks a **costume cabinet** with a **note** showing a 4-digit code.
- A **keypad** by the **stage door**; enter the code → door opens → "You made it
  to the con floor!" 🎌
- Props: a **mannequin** with a costume, a **prop sword**, a **camera**, con
  **posters**, 2 lights, one shadow-casting light. Enough to show every technique.

> **Why cosplay theme:** it ties your portfolio together (Cosplay Map + this game)
> into one memorable story, and keeps you motivated to finish — without changing
> any of the graphics work.
>
> **Keep the engine the star.** Don't expand into character creators, inventory
> systems, or NPCs. A *finished, polished* small engine (with shadow mapping + a
> 2-puzzle cosplay escape room) beats an ambitious half-built game every time.

**Cosplay-forward alternative (optional, later):** a **3D costume viewer** — load
glTF character models, orbit camera, PBR materials, lighting. Very graphics-heavy
and on-brand. But the escape room is the better "complete game" demo, so start there.

The escape room above ships as a **sample level** (`levels/sample-room.json`) — but
players make their own via the editor.

---

## 6.1 Level format (JSON) — each room is a file
A level is a plain JSON file (one "creation instance"). The engine loads it into a
`World` and plays it; the editor writes it back out. Full schema in
**`LEVEL_FORMAT.md`** + example in **`levels/sample-room.json`**. In brief:

```jsonc
{
  "name": "My Cosplay Room",
  "author": "alicia",
  "spawn": { "position": [0, 1.6, 4], "yaw": 180 },
  "lights": [ { "type": "point", "position": [0,2.5,0], "color": [1,0.9,0.8], "intensity": 1.2 } ],
  "entities": [
    { "id": "drawer",  "model": "drawer.glb",  "transform": { "position": [-2,0.5,-1] },
      "interaction": { "type": "container", "locked": false, "contains": "key" } },
    { "id": "cabinet", "model": "cabinet.glb", "transform": { "position": [2,1,-2] },
      "interaction": { "type": "container", "locked": true, "unlockedBy": "key", "contains": "note" } },
    { "id": "note",    "model": "note.glb",    "transform": { "position": [2,1,-2] },
      "interaction": { "type": "readable", "text": "Code: 4827" } },
    { "id": "keypad",  "model": "keypad.glb",  "transform": { "position": [0,1.2,-3] },
      "interaction": { "type": "keypad", "code": "4827", "opens": "door" } },
    { "id": "door",    "model": "door.glb",    "transform": { "position": [0,1,-3] },
      "interaction": { "type": "exit" } }
  ],
  "win": { "exit": "door" }
}
```
Puzzle logic is **data, not code**: `contains` / `unlockedBy` / `code` / `opens`
link entities by `id`. A new puzzle type = a new `interaction.type` handler in the
runtime — the JSON stays the source of truth.

## 6.2 The editor (build-your-own-room) — phase it!
> ⚠️ The editor is the biggest scope jump in this project. **Do it in phases** so
> you always have something that runs:

- **Phase E0 — Load & play JSON (do this FIRST).** No editing yet: read a level
  file, build the `World`, play it. This alone is a strong *data-driven engine*
  demo and de-risks everything after it.
- **Phase E1 — Minimal editor.** ImGui entity list + property panel (edit
  position/rotation/scale, model, interaction fields). **Save**/**Load** JSON.
  Toggle Editor ↔ Play.
- **Phase E2 — Visual editing.** Click-select a prop, move it with **ImGuizmo**
  handles, add props from a **palette**, delete — live in 3D.
- **Phase E3 — Puzzle wiring UI.** Set "drawer contains X", "cabinet unlocked by
  X", keypad code + target door — all in the panel.
- **Phase E4 — Share.** A **level browser** listing JSON files; load any community
  room. (Later: a tiny upload/download server so people share creations.)

---

## 7. What this demonstrates (skills on show)
| Skill | Where |
|---|---|
| Modern **C++17** | the whole engine |
| **Graphics programming** (OpenGL → Vulkan) | renderer, shaders, framebuffers |
| **3D techniques** | lighting, shadow mapping, post-processing, skybox |
| **Tools / editor programming** | the ImGui + ImGuizmo level editor |
| **Serialization / data-driven design** | level JSON load/save, the `World` |
| **Algorithms & performance** | culling, batching, profiling |
| Architecture & simplicity | layered engine + renderer/serialization seams |
| Open source | publish the engine on GitHub |

---

## 8. Stretch goals (do after the game works)
- [ ] **Vulkan backend** behind the same `IRenderer` interface (huge depth signal).
- [ ] A **terrain or vector-tile renderer** mini-demo (heightmap terrain, or
      render the GeoJSON polygons from your cosplay-map project — a fun cross-link
      and great geometry/graphics practice).
- [ ] Compute-shader effect (particles).

---

## 9. Learning resources (in order)
- **LearnOpenGL.com** — *the* OpenGL course; the milestones above follow it.
- **The Cherno — "Game Engine Series" + "OpenGL" (YouTube)** — engine architecture in C++.
- **"A Tour of C++" (Stroustrup)** + **learncpp.com** — modern C++.
- **"3D Math Primer for Graphics and Game Development"** — the math (you know much of it).
- **vulkan-tutorial.com** + **webgpufundamentals.org** — when you reach the stretch.
- Read **MapLibre GL JS** source for how a real map engine renders vector tiles.

---

## 10. Make it a portfolio piece (don't skip this)
- A great **README**: 1 GIF of the escape room, a feature list, an architecture
  diagram, and a short **"what I built and learned"** write-up.
- A **technical blog post / devlog** per hard feature (shadow mapping, the camera,
  the renderer abstraction) — practising clear technical communication.
- **Performance numbers** (frame time before/after batching).
- Publish on **GitHub**. Optionally contribute to an open-source renderer for
  extra credibility. Be ready to explain your AI-assisted workflow — while clearly
  understanding every line yourself.

---

## Suggested order if impatient
LearnOpenGL (window → triangle → textures → camera → lighting) **on this scaffold**
→ wrap it in `Shader`/`Mesh`/`Camera` classes → load 2 models → shadow mapping →
build the escape-room puzzle loop → polish README. Then Vulkan/terrain as stretch.
