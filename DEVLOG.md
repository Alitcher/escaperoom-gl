# Development Log

How this engine is being built: what was done, why, and what is still needed.
`ROADMAP.md` is the long-term plan. This file is the day-to-day journey and the
feature list that decides what to build next.

---

## 1. How we plan

1. **Playable first, pretty later.** Every step must end with something you can
   run and see. A rough room you can walk around beats a perfect renderer with
   nothing to play.
2. **Placeholders over blockers.** When a feature depends on something that does
   not exist yet (for example 3D model files), draw a colored box instead and
   keep going. Swap in the real thing later.
3. **Data drives the game.** The level JSON is the source of truth. The engine
   reads it; it never hard-codes a room. Anything a level needs must be
   expressible in `LEVEL_FORMAT.md`.
4. **Small steps, each one building.** One feature at a time, compiled and
   checked before the next.
5. **Write down decisions.** Every non-obvious choice goes in the decision log
   below, with the reason.

---

## 2. Journey so far

### Step 0 - Scaffold (2026-06-29)
- CMake + vcpkg project, a window with an OpenGL 4.6 core context, cleared to a
  dark color. Esc closes it.
- Planning docs written: `ROADMAP.md`, `LEVEL_FORMAT.md`,
  `levels/sample-room.json` (the cosplay green room).

### Step 1 - First triangle (2026-10-25)
- First vertex buffer (VBO), vertex layout (VAO) and shader program.
- One triangle with a red, green and blue corner; the GPU blends the colors.
- Problem found: the triangle looked stretched. The window is 16:9, but OpenGL
  maps -1..1 to the full width and the full height. First fix: pass the aspect
  ratio to the shader as a **uniform** and divide x by it. (Replaced in step 2
  by a projection matrix.)
- Build setup: `C:\vcpkg` was not installed, so the build uses the vcpkg that
  ships with Visual Studio 2022. That one needs a pinned `builtin-baseline` in
  `vcpkg.json`, which also makes library versions repeatable.

### Step 2 - 3D, camera, rough room, level loading (2026-10-25)
Goal: walk around a box version of the green room, built from the JSON file.

- **3D:** a unit cube mesh with normals, model / view / projection matrices
  (glm), depth test and back-face culling.
- **FPS camera:** mouse to look, WASD to walk on the floor, Shift to run. The
  cursor is hidden and locked to the window.
- **Level loading:** `levels/sample-room.json` is read with nlohmann/json into a
  `Level` struct (spawn, lights, entities, interactions, win). Bad files give a
  clear error message instead of a crash. Duplicate entity ids are rejected.
- **Placeholder boxes:** there is no model loader yet, so each model name
  (`drawer.glb`, `door.glb`, ...) maps to one or more colored boxes. Unknown
  model names show as a magenta box so mistakes are easy to see.
- **Lighting:** simple diffuse light from the level's lights (one directional +
  up to 4 point lights) plus ambient, so box faces are readable.
- **Code split into pieces:** `Shader`, `Mesh`, `FpsCamera`, `Level`,
  `Placeholders`, and a short `main.cpp`.
- **Level fixes:** the spawn point was at z = 4, outside the 6 x 6 room, so it
  moved to z = 2.2. The poster was hidden behind the cabinet, so it moved to
  the left of the door.

### Step 3 - Playable escape room (2026-10-25)
Goal: solve the sample room from start to "You escaped".

- **Walls block you:** the player is a box (0.5 m wide, 1.8 m tall). Each frame
  the move is tried on x and z separately, so walking into a wall at an angle
  slides along it instead of stopping dead.
- **Arrow keys** walk too, same as WASD. The window title shows your x / z
  position, an easy check that walking works. The window grabs keyboard focus
  when it opens (one guess for "WASD does nothing": the console kept focus).
- **Crosshair + clicking:** a ray from the camera finds the nearest box within
  2.5 m. If it belongs to something usable, the crosshair turns yellow, the
  object lights up and a hint shows ("[E / click] Open drawer"). Walls in front
  block the ray, so you can't use things through walls.
- **Puzzle rules** (`Game.cpp`, no rendering inside): inventory, pickup,
  container (locked / unlockedBy / contains), readable, keypad, exit, win.
  Opened containers are drawn darker.
- **UI with Dear ImGui** (`Ui.cpp`): inventory + timer, controls help, messages,
  note popup, keypad (click buttons or type digits), inventory window (Tab, read
  the note again), win screen with time, Play again and Quit.
- **Mouse modes:** while walking, the cursor is hidden and turns the camera. When
  a popup is open, the cursor is free so you can click buttons.
- **Level format:** added `win.message`, and wrote down the rule for what
  `contains` does (this was an open question).
- **Checked:** a throwaway test program played the whole sample room through
  the game rules (locked cabinet, drawer gives key, cabinet shows the note,
  wrong code, right code, door wins, restart) and checked walls and clicking
  from several spots. All passed.

### Step 4 - Pocket items, NPC talk, dice (2026-10-25)
Goal: items you hold and use yourself, a character who talks, and a dice lock.

- **Pocket bar:** 9 slots at the bottom of the screen with 2D item icons. Keys
  1-9 hold an item (same key again puts it away); the held item has a yellow
  frame and its name shows above the bar. The Tab inventory shows the same
  icons; click one to hold it.
- **Using items:** locked things now need the right item **held**, not just
  owned. The crosshair hint says "Use Prop key on cabinet" while holding
  something. A wrong item says it doesn't work.
- **Item icons:** a level can list `items` with a display name and a PNG icon
  (loaded with stb_image). Without an icon the item is a colored tile with its
  letters, so levels work with no art at all.
- **NPCs:** new `npc` type. Clicking shows one line of speech above the pocket
  bar ("Mika: ..."); each click shows the next line, then it starts over. An NPC
  can `give` an item on the first talk. Walking is not blocked while they talk.
- **Dice:** new `dice` type. Clicking rolls 3 dice at once (count and sides are
  set in the level), shows the faces with pips after a short tumble, and the
  total. Space or the button rolls again.
- **Dice lock:** a keypad with `codeFrom: "dice"` wants the total of the last
  roll. Rolling again changes the code. Before any roll, no code works.
- **Exits** can now also be unlocked by holding an item (`unlockedBy`).
- **Sample room puzzle**, longer now: talk to Mika -> roll the dice -> type the
  total into the drawer lock -> take the key -> hold it on the cabinet -> read
  the note -> type 4827 at the door -> escape.
- **Checked:** the throwaway test played the new chain: NPC lines in order and
  wrapping, drawer locked before the roll, 500 rolls all in range, wrong and
  right dice totals, cabinet refusing a key that is not held, wrong item on the
  door, win, restart. Also clicking the lock, dice, Mika and door from where a
  player would stand. All passed.

### Step 5 - Level editor, phases E1 + E2 (2026-10-25)
Goal: a designer can build and test a room without touching JSON, with a
Unity-like layout.

- **Layout:** toolbar on top, Hierarchy left, Scene view middle, Inspector
  right, Project panel bottom. The 3D view renders only into the middle area.
- **Scene view:** click an object to select it (orange outline), drag ImGuizmo
  handles to move / rotate / scale. Right mouse + WASD / QE flies the camera,
  wheel zooms, F focuses. Ctrl or the Snap box snaps to 0.25 m / 15 deg.
- **Inspector:** Transform with Unity-style red / green / blue X Y Z fields
  (click a letter to reset that axis), puzzle type dropdown and the fields for
  each type, with dropdowns of matching ids. Objects can be dragged from the
  Hierarchy onto id fields (e.g. a keypad's "Opens").
- **Project panel:** Models (3D) tab: drag into the scene to place on the
  surface under the mouse, or click to add. Items (2D) tab: drag onto a drawer
  (asks: put inside, or make it the key), a door (key), an NPC (gift), or the
  floor (a pickup lying there). Click an item to edit its name and icon.
- **Level settings:** title, author, player spawn (blue marker in the scene,
  movable with the gizmo), winning exit + message, lights (yellow markers).
- **Problems list:** plain-language checks, e.g. "keypad opens 'door2', which
  doesn't exist", "locked, but nothing unlocks it", "no winning exit".
- **Files:** Save (Ctrl+S), Save as, New (a starter room with a door and
  keypad), and a level browser listing every file in `levels/`. Saved JSON is
  tidy and readable (`[0, 1.6, 2.2]`, no float noise). Unsaved changes are
  shown, and quitting asks first.
- **Undo / Redo:** the whole level is snapshotted after each finished edit
  (not every keystroke or drag frame). Simple and can't miss a change.
- **Playtest:** F5 or Play builds a fresh game from the current level.
- **Code split:** `main.cpp` got too big, so it became `Renderer` (drawing),
  `Scene` (boxes, matrices, rays), `PlaySession` (the game) and `Editor`.
- **Rotation order changed** to X-Y-Z (glm `eulerAngleXYZ`) so gizmo results
  convert back to the same three numbers. Only Y rotations were used before, so
  no level changed. When a gizmo rotation can be written two ways (e.g.
  (0, -120, 0) and (-180, -60, 180)), the one closest to the old numbers is
  kept, so the Inspector doesn't jump.
- **Checked:** save -> load gives the same level; undo snapshots load back;
  2000 random transforms convert matrix -> numbers -> matrix exactly; the sample
  puzzle still wins after a save and reload. The editor UI itself needs a person
  to click through it.

### Step 6 - Unity camera controls, parents, Win98 look (2026-10-25)
Feedback on step 5: no Unity-style navigation, and the dark theme didn't fit.

- **Camera like Unity:** Alt (or Ctrl) + left drag orbits around the
  selection, middle drag pans (the point under the mouse follows the cursor),
  wheel and Alt + right drag zoom with steps that scale with distance, right
  drag + WASD still flies. Ctrl+click on a gizmo handle still means "snap".
- **Selection like Unity:** left drag on empty space draws a selection box
  (objects fully inside are selected), Shift+click adds or removes. The gizmo
  moves all selected objects together.
- **Parent / child:** entities have an optional `parent`; their transform is
  relative to it. The Hierarchy is a tree: drag onto an object to parent, onto
  the empty area to un-parent; the object keeps its place in the world. Things
  hidden inside a container show "inside" and are its children. Setting a
  container's "Contains" to an object makes it a child automatically.
  Duplicate copies the whole branch and points its links (keypad opens,
  contains, dice) at the copies; Delete removes the branch.
- **Sample room:** dice, drawer lock and key are children of the drawer, the
  note of the cabinet, the door keypad of the door. World positions unchanged.
- **Windows 98 theme** matching the portfolio site: gray panels, navy title
  bars, raised bevels, white text boxes and list, navy selected rows with white
  text, Courier New, teal scene background. Game popups use it too.
- **Safety:** a parent loop (a -> b -> a) or a missing parent is treated as top
  level and listed in Problems, so a bad file can't hang the engine.
- **Checked:** children land at the same world spots as before; moving or
  turning a parent carries its children; save / load keeps parents; loops and
  missing parents don't hang; clicking the drawer lock still works; the puzzle
  still wins.

### Step 7 - Menu bar, Windows 95 windows (2026-10-25)
Feedback: the editor needs a real menu bar like Word / Unity, and windows that
look like Windows 95 Explorer.

- **Menu bar:** File, Edit, GameObject, View, Window, Help with the usual
  shortcuts (Ctrl+N / O / S / Shift+S, Ctrl+Z / Y, Ctrl+X / C / V / D / A, F2,
  F, Del, F5). Undo / Redo show what they will undo ("Undo Move drawer").
- **Undo History window:** every step by name; click one to jump back or
  forward to it. Steps are named by the action (move, rename, paste ...) or
  "Change <object>" for Inspector edits.
- **Clipboard:** Cut / Copy / Paste work on objects with their children; links
  inside the pasted group point at the new copies. Duplicate = copy + paste
  without touching the clipboard.
- **GameObject menu:** Create Empty (invisible object to group things under),
  3D Object, Make Parent (last selected becomes the parent), Clear Parent, Move
  To View.
- **Window menu / X buttons:** panels can be hidden; the scene view grows.
- **Win95 style:** gray #c0c0c0 windows, title bars navy when focused and gray
  when not, white title text, sunken white list and fields, dotted tree lines,
  navy selected labels, a status bar with sunken fields. The program's own
  Windows title bar is navy too (Windows 11 allows apps to color it).
- **Fonts:** Arial by default; View > Font switches to Times New Roman or
  Courier New live.

---

## 3. Feature list

Status: **done**, **next**, **later**, **maybe**.

### A. Must have - a playable escape room
| Feature | Status | Notes |
|---|---|---|
| Window, game loop, input | done | |
| 3D rendering of boxes | done | placeholder look |
| FPS camera (look + walk) | done | |
| Load level JSON | done | `Level.cpp` |
| Simple lighting from level lights | done | diffuse only |
| Walk with arrow keys too | done | |
| Walls stop the player (collision) | done | box vs box, slides along walls |
| Crosshair in the screen center | done | yellow when aiming at something usable |
| Click to interact (ray picking) | done | E or left click, 2.5 m reach |
| Inventory (set of item ids) | done | HUD + Tab window |
| `pickup` handler | done | |
| `container` handler | done | locked / unlockedBy / contains |
| `readable` handler (show text) | done | popup, re-read from inventory |
| `keypad` handler (enter code) | done | buttons or keyboard digits |
| `exit` handler + win screen | done | "You made it to the con floor!" + time |
| On-screen UI (text, keypad, inventory) | done | Dear ImGui |
| Restart the level | done | R key or Play again |
| Hide items until revealed | done | rule written in LEVEL_FORMAT.md |
| Pocket bar with 2D item icons | done | 9 slots, PNG or drawn tile |
| Hold an item and use it on things | done | keys 1-9 or click in inventory |
| NPC with one line per talk | done | `npc` type, can give an item |
| Roll 3 dice, total is a code | done | `dice` type + keypad `codeFrom` |
| Real icon images for the sample items | next | add PNGs in `levels/icons/` |
| Visible "open" state for drawer / door | next | today only a darker color |
| Combine two items into one | maybe | classic adventure-game puzzle |
| Give an item to an NPC | maybe | NPC `wants` an item, gives something back |
| Settings: mouse speed, invert look | later | |

### B. Engine and graphics
| Feature | Status | Notes |
|---|---|---|
| Shader / Mesh / Camera classes | done | |
| Real 3D models (glTF) | later | replaces placeholder boxes |
| Textures | later | stb_image is already installed |
| Resource manager (load once) | later | |
| Phong lighting (specular) | later | |
| Shadow mapping | later | |
| Post-processing (framebuffers) | later | |
| Skybox | later | less useful indoors |
| Frame time / FPS display | later | |

### C. Editor (make your own rooms)
| Feature | Status | Notes |
|---|---|---|
| Save a level back to JSON | done | tidy, readable output |
| Hierarchy + Inspector (Unity-style Transform) | done | |
| Select and move / rotate / scale in 3D | done | ImGuizmo, snap with Ctrl |
| Add / duplicate / delete props | done | drag from Project panel |
| Drag 2D items onto objects | done | inside, key, gift, or pickup |
| Undo / redo | done | whole-level snapshots |
| Problems list | done | plain-language level checks |
| Playtest from the editor | done | F5 |
| Puzzle wiring panel | done (basic) | dropdowns + drag ids; a visual link graph is still "later" |
| Level browser | done (basic) | toolbar list of `levels/*.json`; online sharing is "maybe" |
| Unity scene navigation (orbit, pan, zoom) | done | Alt/Ctrl + left, middle, wheel |
| Parent / child hierarchy | done | tree, drag to parent, branch copy / delete |
| Multi-select + box select | done | Shift adds; gizmo moves all |
| Windows 98 theme | done | matches the portfolio site |
| Resizable / dockable panels | later | needs ImGui's docking branch |
| Lines in the scene showing puzzle links | later | e.g. keypad -> door |

### D. Nice to have
| Feature | Status | Notes |
|---|---|---|
| Sound (door, keypad beeps) | maybe | |
| Animated drawer / door opening | maybe | |
| Vulkan renderer | maybe | stretch goal |
| Share levels online | maybe | needs a server |

---

## 4. Open questions
- **Box sizes vs real models:** placeholder sizes live in code
  (`Placeholders.cpp`). When real models arrive, should a level be able to
  override size per entity, or does the model file define it?
- **Keypad on the wall:** today the keypad is a 2D popup. Later, should it be
  clickable 3D buttons on the keypad model?
- **Dice lock fairness:** the code changes on every roll, so the player must
  type the total of their *latest* roll. Is that what we want, or should the
  first roll be fixed?
- **Scope:** the roadmap warned against growing into inventory systems and
  NPCs. We chose to add a small version of both. Keep them small: no dialogue
  trees, no NPC walking.

---

## 5. Decision log
| Date | Decision | Why |
|---|---|---|
| 2026-06-29 | OpenGL 4.6 first, Vulkan later | fastest way to a working game |
| 2026-06-29 | Levels are JSON files | makes rooms data, enables an editor |
| 2026-10-25 | Use Visual Studio's vcpkg + pinned baseline | already installed; repeatable versions |
| 2026-10-25 | nlohmann/json for level files | simple, header-only, standard choice |
| 2026-10-25 | Placeholder boxes per model name | play the room before a model loader exists |
| 2026-10-25 | Walk on the floor plane only (no flying) | it is a walking game, not a free camera |
| 2026-10-25 | Default level path baked in at build time | the exe finds the level from any folder |
| 2026-10-25 | `contains` with an entity id hides that entity until opened | no extra `hidden` flag needed; levels stay short |
| 2026-10-25 | Puzzle rules in `Game`, UI in `Ui`, no GL in either | rules can be tested without a window; editor can reuse them |
| 2026-10-25 | Dear ImGui for all game UI | already planned for the editor; fastest way to text and buttons |
| 2026-10-25 | Keypad as a 2D popup | much faster than 3D buttons; can upgrade later |
| 2026-10-25 | Items must be held to be used | the player solves it, not the game; classic adventure feel |
| 2026-10-25 | Icons optional, drawn tile as fallback | levels work without any art files |
| 2026-10-25 | NPC talk is one line at a time, non-blocking | simple to write in JSON; player keeps walking |
| 2026-10-25 | Dice total feeds a normal keypad (`codeFrom`) | reuses the keypad instead of a new lock type |
| 2026-10-25 | Exits accept `unlockedBy` too | same rule as containers; answered an open question |
| 2026-10-25 | Unity-style fixed layout, no docking | familiar to designers; docking needs a different ImGui build |
| 2026-10-25 | Undo = snapshot of the whole level | levels are small; nothing can be forgotten |
| 2026-10-25 | Rotation order X-Y-Z | gizmo results convert back exactly |
| 2026-10-25 | Editor and game in one exe, F5 switches | instant playtest of exactly what is on screen |
| 2026-10-25 | Unity's mouse scheme, plus Ctrl + left drag to orbit | designers know Unity; Ctrl was asked for |
| 2026-10-25 | Transforms are local to the parent | same as Unity; moving a drawer moves its lock and key |
| 2026-10-25 | Windows 98 theme | one visual identity with the portfolio site |
