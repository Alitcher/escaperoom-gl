#include "Editor.h"
#include "Icons.h"
#include "Placeholders.h"
#include "Renderer.h"
#include "Theme.h"
#include "Ui.h"

#include <imgui.h>
#include <imgui_internal.h>   // ImRect + BeginDragDropTargetCustom (drop into the scene view)
#include <imgui_stdlib.h>     // InputText with std::string
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <set>

namespace fs = std::filesystem;

static constexpr int kNone = -1;
static constexpr int kLevelSettings = -2;

// Panel sizes in pixels.
static constexpr float kToolbarH = 58.0f;
static constexpr float kHierarchyW = 320.0f;
static constexpr float kInspectorW = 440.0f;
static constexpr float kProjectH = 230.0f;

// Height of the Win95 status bar along the bottom of the window.
static float statusBarH() {
    return ImGui::GetFrameHeight() + 8.0f;
}
static constexpr float kFovDeg = 60.0f;
static constexpr size_t kMaxUndo = 200;

static constexpr ImGuiWindowFlags kPanelFlags =
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

static const ImU32 kOutline = IM_COL32(255, 200, 0, 255);   // selection outline
static const ImVec4 kProblemColor(0.65f, 0.0f, 0.0f, 1.0f);

// ---------- Small widgets ----------

// Label in a fixed-width left column, then the next widget fills the rest.
static void labelColumn(const char* label) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(ImGui::GetFontSize() * 7.5f);
    ImGui::SetNextItemWidth(-1.0f);
}

// Unity-style row: label, then X / Y / Z drag fields with red / green / blue
// tags. Clicking a tag resets that axis to `resetValue`.
static bool vec3Row(const char* label, glm::vec3& v, float speed, float resetValue) {
    static const ImVec4 tagColors[3] = {
        ImVec4(0.70f, 0.15f, 0.15f, 1.0f), ImVec4(0.10f, 0.50f, 0.10f, 1.0f), ImVec4(0.15f, 0.25f, 0.65f, 1.0f)};
    static const char* tags[3] = {"X", "Y", "Z"};

    bool changed = false;
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(ImGui::GetFontSize() * 7.5f);

    const float tagW = ImGui::GetFrameHeight();
    const float gap = 4.0f;
    const float fieldW = (ImGui::GetContentRegionAvail().x - 3.0f * tagW - 2.0f * gap) / 3.0f;
    for (int i = 0; i < 3; ++i) {
        if (i > 0)
            ImGui::SameLine(0.0f, gap);
        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button, tagColors[i]);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, tagColors[i]);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, tagColors[i]);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        if (ImGui::Button(tags[i], ImVec2(tagW, tagW))) {
            v[i] = resetValue;
            changed = true;
        }
        ImGui::PopStyleColor(4);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Click to reset %s", tags[i]);
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::SetNextItemWidth(fieldW);
        changed |= ImGui::DragFloat("##v", &v[i], speed, 0.0f, 0.0f, "%.2f");
        sunkenBevelLastItem();
        ImGui::PopID();
    }
    ImGui::PopID();
    return changed;
}

static std::string payloadString(const ImGuiPayload* payload) {
    return std::string((const char*)payload->Data);
}

// Text field for an id, with a dropdown of suggestions on the right. Items and
// objects can also be dragged onto it from the Project panel or Hierarchy.
static bool idField(const char* label, std::string& value, const std::vector<std::string>& options) {
    bool changed = false;
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(ImGui::GetFontSize() * 7.5f);

    const float comboW = ImGui::GetFrameHeight();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - comboW - 4.0f);
    changed |= ImGui::InputText("##value", &value);
    sunkenBevelLastItem();
    if (ImGui::BeginDragDropTarget()) {
        for (const char* type : {"ITEM", "ENTITY"}) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(type)) {
                value = payloadString(p);
                changed = true;
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::BeginCombo("##pick", nullptr, ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft)) {
        if (ImGui::Selectable("(none)", value.empty())) {
            value.clear();
            changed = true;
        }
        for (const std::string& option : options) {
            if (ImGui::Selectable(option.c_str(), option == value)) {
                value = option;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
    return changed;
}

static void tip(const char* text) {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", text);
    ImGui::PopTextWrapPos();
}

// A toolbar button that looks pressed in (Win98 sunken bevel) while `active`.
static bool toggleButton(const char* label, bool active) {
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.84f, 0.84f, 0.84f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    }
    const bool clicked = ImGui::Button(label);
    if (active)
        ImGui::PopStyleColor(3);
    return clicked;
}

// "drawer.glb" -> "drawer"
static std::string modelStem(const std::string& model) {
    const size_t dot = model.find_last_of('.');
    return dot == std::string::npos ? model : model.substr(0, dot);
}

static std::string toLower(std::string s) {
    for (char& c : s)
        c = (char)std::tolower((unsigned char)c);
    return s;
}

// Sensible puzzle settings for a newly placed model.
static Interaction defaultInteraction(const std::string& model, const std::string& id) {
    Interaction in;
    if (model == "drawer.glb" || model == "cabinet.glb") {
        in.type = "container";
    } else if (model == "keypad.glb") {
        in.type = "keypad";
        in.code = "0000";
    } else if (model == "door.glb") {
        in.type = "exit";
    } else if (model == "dice.glb") {
        in.type = "dice";
    } else if (model == "npc.glb") {
        in.type = "npc";
        in.lines = {"Hello!"};
    } else if (model == "note.glb") {
        in.type = "readable";
        in.text = "Write the clue here.";
    } else if (model == "prop_key.glb") {
        in.type = "pickup";
        in.item = "key";
    } else if (model == "item.glb") {
        in.type = "pickup";
        in.item = id;
    }
    in.name = id;
    return in;
}

// ---------- Setup, camera math, selection ----------

Editor::Editor(Level& level, std::string& levelPath, IconCache& icons)
    : m_level(level), m_path(levelPath), m_icons(icons) {
    refresh();
    resetCamera();
    resetHistory();
}

void Editor::refresh() {
    m_scene = buildScene(m_level);
    m_world = worldMatrices(m_level);
    m_parents = parentIndices(m_level);
}

void Editor::resetCamera() {
    const float yaw = glm::radians(m_level.spawnYawDeg);
    const glm::vec3 flatForward(std::sin(yaw), 0.0f, std::cos(yaw));
    m_camera.position = m_level.spawnPosition - flatForward * 1.0f + glm::vec3(0.0f, 0.6f, 0.0f);
    m_camera.yawDeg = m_level.spawnYawDeg;
    m_camera.pitchDeg = -20.0f;
}

float Editor::topY() const {
    return ImGui::GetFrameHeight() + kToolbarH;   // menu bar + toolbar
}

// The 3D view fills whatever the shown panels leave free.
Editor::Rect Editor::sceneRect() const {
    const ImVec2 d = ImGui::GetIO().DisplaySize;
    const float left = m_showHierarchy ? kHierarchyW : 0.0f;
    const float right = m_showInspector ? kInspectorW : 0.0f;
    const float bottom = (m_showProject ? kProjectH : 0.0f) + statusBarH();
    return {left, topY(), std::max(d.x - left - right, 64.0f), std::max(d.y - topY() - bottom, 64.0f)};
}

glm::mat4 Editor::projMatrix() const {
    const Rect r = sceneRect();
    return glm::perspective(glm::radians(kFovDeg), r.w / r.h, 0.05f, 200.0f);
}

void Editor::mouseRay(const glm::vec2& mouse, glm::vec3& origin, glm::vec3& dir) const {
    const Rect r = sceneRect();
    const float nx = (mouse.x - r.x) / r.w * 2.0f - 1.0f;
    const float ny = 1.0f - (mouse.y - r.y) / r.h * 2.0f;
    const glm::mat4 inv = glm::inverse(projMatrix() * viewMatrix());
    glm::vec4 a = inv * glm::vec4(nx, ny, -1.0f, 1.0f);
    glm::vec4 b = inv * glm::vec4(nx, ny, 1.0f, 1.0f);
    a /= a.w;
    b /= b.w;
    origin = glm::vec3(a);
    dir = glm::normalize(glm::vec3(b - a));
}

bool Editor::worldToScreen(const glm::vec3& p, glm::vec2& out) const {
    const glm::vec4 clip = projMatrix() * viewMatrix() * glm::vec4(p, 1.0f);
    if (clip.w < 0.01f)
        return false;   // behind the camera
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    const Rect r = sceneRect();
    out = {r.x + (ndc.x * 0.5f + 0.5f) * r.w, r.y + (0.5f - ndc.y * 0.5f) * r.h};
    return true;
}

bool Editor::entityBounds(int entity, Aabb& out) const {
    if (entity < 0 || entity >= (int)m_scene.size() || m_scene[entity].empty())
        return false;
    out = m_scene[entity][0].bounds;
    for (const SceneBox& b : m_scene[entity]) {
        out.min = glm::min(out.min, b.bounds.min);
        out.max = glm::max(out.max, b.bounds.max);
    }
    return true;
}

bool Editor::isSelected(int entity) const {
    return std::find(m_selection.begin(), m_selection.end(), entity) != m_selection.end();
}

void Editor::selectOnly(int entity) {
    m_selected = entity;
    m_selection.clear();
    if (entity >= 0)
        m_selection.push_back(entity);
    m_selectedItem.clear();
}

void Editor::toggleSelected(int entity) {
    if (entity < 0)
        return;
    auto it = std::find(m_selection.begin(), m_selection.end(), entity);
    if (it != m_selection.end()) {
        m_selection.erase(it);
        if (m_selected == entity)
            m_selected = m_selection.empty() ? kNone : m_selection.back();
    } else {
        m_selection.push_back(entity);
        m_selected = entity;
    }
    m_selectedItem.clear();
}

bool Editor::isDescendant(int entity, int ancestor) const {
    for (int p = m_parents[entity]; p >= 0; p = m_parents[p])
        if (p == ancestor)
            return true;
    return false;
}

std::vector<int> Editor::topLevelSelection() const {
    std::vector<int> out;
    for (int i : m_selection) {
        bool underSelected = false;
        for (int other : m_selection)
            if (other != i && isDescendant(i, other))
                underSelected = true;
        if (!underSelected)
            out.push_back(i);
    }
    return out;
}

std::vector<int> Editor::subtree(int root) const {
    std::vector<int> out{root};
    for (size_t k = 0; k < out.size(); ++k)
        for (int i = 0; i < (int)m_parents.size(); ++i)
            if (m_parents[i] == out[k])
                out.push_back(i);
    return out;
}

void Editor::afterStructureChange() {
    refresh();
    const int n = (int)m_level.entities.size();
    m_selection.erase(std::remove_if(m_selection.begin(), m_selection.end(), [n](int i) { return i >= n; }),
                      m_selection.end());
    if (m_selected >= n)
        m_selected = m_selection.empty() ? kNone : m_selection.back();
    m_idBufferFor = -1;
    m_itemIdBufferFor.clear();
}

// ---------- Camera ----------

// Unity-style scene camera:
//   right drag (+ WASD / QE, Shift faster)  look around and fly
//   Alt or Ctrl + left drag                 orbit around the selection
//   middle drag                             pan
//   Alt + right drag, or mouse wheel        zoom
void Editor::updateCamera(GLFWwindow* window, float dt) {
    ImGuiIO& io = ImGui::GetIO();
    const glm::vec2 mouse(io.MousePos.x, io.MousePos.y);
    const glm::vec2 delta(io.MouseDelta.x, io.MouseDelta.y);
    const bool idle = !m_looking && !m_orbiting && !m_panning && !m_zoomDragging;

    if (idle && m_sceneHovered) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && (io.KeyAlt || io.KeyCtrl) && !ImGuizmo::IsOver()) {
            m_orbiting = true;
            m_pivot = orbitPivot();
            m_pivotDistance = std::max(glm::length(m_camera.position - m_pivot), 0.5f);
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
            m_panning = true;
            m_pivotDistance = distanceUnderMouse(mouse);
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && io.KeyAlt) {
            m_zoomDragging = true;
            m_pivotDistance = distanceUnderMouse(mouse);
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            m_looking = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
            glfwGetCursorPos(window, &m_lastMouseX, &m_lastMouseY);
            return;
        }
    }

    const glm::vec3 forward = glm::normalize(m_camera.forward());
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
    const glm::vec3 up = glm::cross(right, forward);

    if (m_orbiting) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            m_orbiting = false;
        } else {
            m_camera.look(delta.x, delta.y);
            m_camera.position = m_pivot - glm::normalize(m_camera.forward()) * m_pivotDistance;
        }
    }
    if (m_panning) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
            m_panning = false;
        } else {
            // Grab the world: the point under the mouse follows the cursor.
            const float worldPerPixel = 2.0f * m_pivotDistance * std::tan(glm::radians(kFovDeg) * 0.5f) / sceneRect().h;
            m_camera.position += (-right * delta.x + up * delta.y) * worldPerPixel;
        }
    }
    if (m_zoomDragging) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
            m_zoomDragging = false;
        else
            m_camera.position += forward * (delta.x - delta.y) * 0.005f * std::max(m_pivotDistance, 1.0f);
    }

    if (m_looking) {
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS) {
            m_looking = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
            return;
        }
        double x = 0.0, y = 0.0;
        glfwGetCursorPos(window, &x, &y);
        m_camera.look((float)(x - m_lastMouseX), (float)(y - m_lastMouseY));
        m_lastMouseX = x;
        m_lastMouseY = y;

        glm::vec3 move = m_camera.wantedMove(window, dt) * 1.6f;
        const float vertical = 4.0f * dt * (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 2.0f : 1.0f);
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) move.y += vertical;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) move.y -= vertical;
        m_camera.position += move;
    } else if (idle && m_sceneHovered && io.MouseWheel != 0.0f) {
        // Bigger steps when far away, small steps up close.
        const float step = std::max(distanceUnderMouse(mouse) * 0.15f, 0.2f);
        m_camera.position += forward * io.MouseWheel * step;
    }
}

// What the camera orbits: the selection, else the spot in the middle of the view.
glm::vec3 Editor::orbitPivot() const {
    bool any = false;
    Aabb all{};
    for (int i : m_selection) {
        Aabb b;
        if (!entityBounds(i, b))
            b = {glm::vec3(m_world[i][3]), glm::vec3(m_world[i][3])};
        if (!any) {
            all = b;
            any = true;
        } else {
            all.min = glm::min(all.min, b.min);
            all.max = glm::max(all.max, b.max);
        }
    }
    if (any)
        return (all.min + all.max) * 0.5f;
    if (m_selected == kLevelSettings)
        return m_level.spawnPosition;

    const Rect r = sceneRect();
    glm::vec3 o, d;
    mouseRay({r.x + r.w * 0.5f, r.y + r.h * 0.5f}, o, d);
    float t = 0.0f;
    if (raycast(m_scene, o, d, 100.0f, [](size_t) { return true; }, &t) >= 0)
        return o + d * t;
    return m_camera.position + glm::normalize(m_camera.forward()) * 5.0f;
}

float Editor::distanceUnderMouse(const glm::vec2& mouse) const {
    glm::vec3 o, d;
    mouseRay(mouse, o, d);
    float t = 0.0f;
    if (raycast(m_scene, o, d, 100.0f, [](size_t) { return true; }, &t) >= 0)
        return std::max(t, 0.3f);
    return 5.0f;
}

void Editor::handleShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
        return;   // typing in a field
    auto pressed = [](ImGuiKey key) { return ImGui::IsKeyPressed(key, false); };

    if (io.KeyCtrl) {
        if (pressed(ImGuiKey_S)) {
            if (io.KeyShift) {
                m_saveAsName = fs::path(m_path).filename().string();
                m_openSaveAs = true;
            } else {
                save();
            }
        }
        if (pressed(ImGuiKey_N)) {
            if (io.KeyShift)
                createEmpty();
            else if (m_dirty)
                confirmThen("Discard unsaved changes and start a new level?", [this] { newLevel(); });
            else
                newLevel();
        }
        if (pressed(ImGuiKey_O)) m_openOpenLevel = true;
        if (pressed(ImGuiKey_Z)) { if (io.KeyShift) redo(); else undo(); }
        if (pressed(ImGuiKey_Y)) redo();
        if (pressed(ImGuiKey_D)) duplicateSelected();
        if (pressed(ImGuiKey_C)) copySelected();
        if (pressed(ImGuiKey_X)) cutSelected();
        if (pressed(ImGuiKey_V)) paste();
        if (pressed(ImGuiKey_A)) selectAll();
        return;
    }
    if (pressed(ImGuiKey_F2) && m_selected >= 0) m_focusName = true;
    if (pressed(ImGuiKey_W) || pressed(ImGuiKey_1)) m_gizmoOp = 0;
    if (pressed(ImGuiKey_E) || pressed(ImGuiKey_2)) m_gizmoOp = 1;
    if (pressed(ImGuiKey_R) || pressed(ImGuiKey_3)) m_gizmoOp = 2;
    if (pressed(ImGuiKey_F)) focusSelected();
    if (pressed(ImGuiKey_Delete)) deleteSelected();
    if (pressed(ImGuiKey_Escape)) selectOnly(kNone);
}

void Editor::focusSelected() {
    if (m_selection.empty() && m_selected != kLevelSettings)
        return;
    const glm::vec3 center = orbitPivot();
    float size = 1.0f;
    for (int i : m_selection) {
        Aabb b;
        if (entityBounds(i, b))
            size = std::max(size, glm::length(b.max - b.min));
    }
    m_camera.position = center - glm::normalize(m_camera.forward()) * (size * 1.5f + 0.8f);
}

// ---------- Frame ----------

Editor::Action Editor::frame(GLFWwindow* window, float dt) {
    refresh();
    const int n = (int)m_level.entities.size();
    m_selection.erase(std::remove_if(m_selection.begin(), m_selection.end(), [n](int i) { return i >= n; }),
                      m_selection.end());
    if (m_selected >= n)
        m_selected = m_selection.empty() ? kNone : m_selection.back();

    m_window = window;
    updateCamera(window, dt);
    if (!m_looking && !m_orbiting && !m_panning && !m_zoomDragging)
        handleShortcuts();

    Action action = Action::None;
    drawMenuBar(action);
    if (drawToolbar() == Action::Play)
        action = Action::Play;
    if (m_showHierarchy) drawHierarchy();
    if (m_showInspector) drawInspector();
    if (m_showProject) drawProject();
    if (m_showHistory) drawHistoryWindow();
    drawStatusBar();
    drawSceneView();
    drawPopups();
    trackHistory();
    return action;
}

// ---------- Menu bar (File / Edit / GameObject / View / Window / Help) ----------

void Editor::drawMenuBar(Action& action) {
    if (!ImGui::BeginMainMenuBar())
        return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Level", "Ctrl+N")) {
            if (m_dirty)
                confirmThen("Discard unsaved changes and start a new level?", [this] { newLevel(); });
            else
                newLevel();
        }
        if (ImGui::MenuItem("Open Level...", "Ctrl+O"))
            m_openOpenLevel = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Save", "Ctrl+S"))
            save();
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
            m_saveAsName = fs::path(m_path).filename().string();
            m_openSaveAs = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Play", "F5"))
            action = Action::Play;
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4") && m_window)
            glfwSetWindowShouldClose(m_window, true);   // asks first if there are unsaved changes
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        const std::string undoLabel = m_undo.empty() ? "Undo" : "Undo " + m_undo.back().label;
        const std::string redoLabel = m_redo.empty() ? "Redo" : "Redo " + m_redo.back().label;
        if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, !m_undo.empty()))
            undo();
        if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, !m_redo.empty()))
            redo();
        ImGui::MenuItem("Undo History", nullptr, &m_showHistory);
        ImGui::Separator();
        const bool any = !m_selection.empty();
        if (ImGui::MenuItem("Cut", "Ctrl+X", false, any)) cutSelected();
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, any)) copySelected();
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, !m_clipboard.empty())) paste();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, any)) duplicateSelected();
        if (ImGui::MenuItem("Delete", "Del", false, any)) deleteSelected();
        ImGui::Separator();
        if (ImGui::MenuItem("Select All", "Ctrl+A")) selectAll();
        if (ImGui::MenuItem("Deselect All", "Esc")) selectOnly(kNone);
        if (ImGui::MenuItem("Rename", "F2", false, m_selected >= 0)) m_focusName = true;
        if (ImGui::MenuItem("Frame Selected", "F", false, any || m_selected == kLevelSettings)) focusSelected();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("GameObject")) {
        if (ImGui::MenuItem("Create Empty", "Ctrl+Shift+N"))
            createEmpty();
        if (ImGui::BeginMenu("3D Object")) {
            for (const std::string& model : placeholderModels())
                if (ImGui::MenuItem(modelStem(model).c_str()))
                    addEntity(model, viewCenterPoint());
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Make Parent", nullptr, false, m_selection.size() >= 2))
            makeParent();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The last selected object becomes the parent of the others");
        if (ImGui::MenuItem("Clear Parent", nullptr, false, !m_selection.empty()))
            clearParent();
        ImGui::Separator();
        if (ImGui::MenuItem("Move To View", nullptr, false, m_selected >= 0))
            moveToView();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Move tool", "W", m_gizmoOp == 0)) m_gizmoOp = 0;
        if (ImGui::MenuItem("Rotate tool", "E", m_gizmoOp == 1)) m_gizmoOp = 1;
        if (ImGui::MenuItem("Scale tool", "R", m_gizmoOp == 2)) m_gizmoOp = 2;
        ImGui::MenuItem("Local axes", nullptr, &m_gizmoLocal);
        ImGui::MenuItem("Snap", nullptr, &m_snap);
        ImGui::Separator();
        if (ImGui::MenuItem("Camera to player spawn"))
            resetCamera();
        ImGui::MenuItem("Spawn and light markers", nullptr, &m_showMarkers);
        ImGui::MenuItem("Help text in scene", nullptr, &m_showHelpText);
        ImGui::Separator();
        if (ImGui::BeginMenu("Font")) {
            static const char* names[3] = {"Arial", "Times New Roman", "Courier New"};
            for (int i = 0; i < 3; ++i)
                if (ImGui::MenuItem(names[i], nullptr, (int)currentUiFont() == i))
                    setUiFont((UiFont)i);
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window")) {
        ImGui::MenuItem("Hierarchy", nullptr, &m_showHierarchy);
        ImGui::MenuItem("Inspector", nullptr, &m_showInspector);
        ImGui::MenuItem("Project", nullptr, &m_showProject);
        ImGui::MenuItem("Undo History", nullptr, &m_showHistory);
        ImGui::Separator();
        if (ImGui::MenuItem("Show all panels"))
            m_showHierarchy = m_showInspector = m_showProject = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Controls"))
            m_openControls = true;
        if (ImGui::MenuItem("About"))
            m_openAbout = true;
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

// A floating list of every change; click one to go back (or forward) to it.
void Editor::drawHistoryWindow() {
    const Rect r = sceneRect();
    ImGui::SetNextWindowPos(ImVec2(r.x + r.w - 300.0f, r.y + 10.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(290.0f, 320.0f), ImGuiCond_FirstUseEver);
    if (!beginRetroWindow("Undo History", &m_showHistory, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }
    tip("Click a step to go back to it. Grey steps can be redone.");
    ImGui::Separator();

    size_t jumpTo = (size_t)-1;
    const size_t done = m_undo.size();
    if (ImGui::Selectable("(level opened)", done == 0))
        jumpTo = 0;
    for (size_t k = 0; k < m_undo.size(); ++k) {
        ImGui::PushID((int)k);
        if (ImGui::Selectable(m_undo[k].label.c_str(), k + 1 == done))
            jumpTo = k + 1;
        ImGui::PopID();
    }
    // Redo steps, next one first (the redo list is stored last-to-first).
    for (size_t k = 0; k < m_redo.size(); ++k) {
        const HistoryEntry& entry = m_redo[m_redo.size() - 1 - k];
        ImGui::PushID((int)(1000000 + k));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        if (ImGui::Selectable(entry.label.c_str(), false))
            jumpTo = done + k + 1;
        ImGui::PopStyleColor();
        ImGui::PopID();
    }
    ImGui::End();

    if (jumpTo != (size_t)-1)
        jumpToHistory(jumpTo);
}

// Shows `text` in the status bar. It also names the undo step if this action
// changed the level (the label is dropped at the end of the frame otherwise).
void Editor::setStatus(const std::string& text) {
    m_status = text;
    m_statusTime = ImGui::GetTime();
    m_pendingLabel = text;
}

// ---------- Toolbar ----------

Editor::Action Editor::drawToolbar() {
    Action action = Action::None;
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetFrameHeight()));   // under the menu bar
    ImGui::SetNextWindowSize(ImVec2(display.x, kToolbarH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));   // #c0c0c0
    ImGui::Begin("##toolbar", nullptr, kPanelFlags | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.47f, 0.0f, 1.0f));   // site's "online" green
    if (ImGui::Button("> Play (F5)"))
        action = Action::Play;
    ImGui::PopStyleColor();

    static const char* tools[3] = {"Move (W)", "Rotate (E)", "Scale (R)"};
    for (int i = 0; i < 3; ++i) {
        ImGui::SameLine(0.0f, i == 0 ? 20.0f : 4.0f);
        if (toggleButton(tools[i], m_gizmoOp == i))
            m_gizmoOp = i;
    }
    ImGui::SameLine();
    if (ImGui::Button(m_gizmoLocal ? "Local" : "World"))
        m_gizmoLocal = !m_gizmoLocal;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Gizmo axes follow the object (Local) or the room (World)");
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &m_snap);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Move in 0.25 m, rotate in 15 deg steps. Hold Ctrl while dragging to snap once.");

    ImGui::SameLine(0.0f, 20.0f);
    ImGui::BeginDisabled(m_undo.empty());
    if (ImGui::Button("Undo"))
        undo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(m_redo.empty());
    if (ImGui::Button("Redo"))
        redo();
    ImGui::EndDisabled();

    ImGui::SameLine(0.0f, 20.0f);
    if (ImGui::Button("Save"))
        save();

    // Level browser: every .json in the levels folder.
    ImGui::SameLine();
    ImGui::SetNextItemWidth(250.0f);
    const std::string current = fs::path(m_path).filename().string();
    if (ImGui::BeginCombo("##levels", current.c_str())) {
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(LEVELS_DIR, ec)) {
            if (entry.path().extension() != ".json")
                continue;
            const std::string name = entry.path().filename().string();
            const std::string path = entry.path().generic_string();
            if (ImGui::Selectable(name.c_str(), name == current)) {
                if (m_dirty)
                    confirmThen("Discard unsaved changes and open " + name + "?", [this, path] { load(path); });
                else
                    load(path);
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Open a level from the levels folder");

    ImGui::End();
    return action;
}

// Win95 status bar: sunken fields with counts, the last message and the file.
void Editor::drawStatusBar() {
    const ImVec2 d = ImGui::GetIO().DisplaySize;
    const float h = statusBarH();
    ImGui::SetNextWindowPos(ImVec2(0, d.y - h));
    ImGui::SetNextWindowSize(ImVec2(d.x, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 4));
    ImGui::Begin("##status", nullptr, kPanelFlags | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    auto field = [](const std::string& text, float width, ImU32 color) {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float fh = ImGui::GetFrameHeight();
        ImGui::Dummy(ImVec2(width, fh));
        ImDrawList* draw = ImGui::GetWindowDrawList();
        sunkenBevel(draw, pos, ImVec2(pos.x + width, pos.y + fh));
        draw->PushClipRect(pos, ImVec2(pos.x + width - 4, pos.y + fh), true);
        draw->AddText(ImVec2(pos.x + 5, pos.y + (fh - ImGui::GetTextLineHeight()) * 0.5f), color, text.c_str());
        draw->PopClipRect();
        ImGui::SameLine(0.0f, 4.0f);
    };

    const int count = (int)m_level.entities.size();
    field(std::to_string(count) + (count == 1 ? " object" : " objects"), 150.0f, retro::kBlack);
    field(std::to_string(m_selection.size()) + " selected", 130.0f, retro::kBlack);

    std::string message;
    ImU32 color = retro::kBlack;
    if (ImGui::GetTime() - m_statusTime < 4.0) {
        message = m_status;
    } else if (m_dirty) {
        message = "Unsaved changes";
        color = IM_COL32(165, 0, 0, 255);
    } else {
        message = "Saved";
    }
    const float pathW = std::min(520.0f, d.x * 0.35f);
    field(message, std::max(ImGui::GetContentRegionAvail().x - pathW - 4.0f, 80.0f), color);
    field(m_path, std::max(ImGui::GetContentRegionAvail().x, 40.0f), retro::kBlack);
    ImGui::End();
}

// ---------- Hierarchy ----------

void Editor::drawHierarchy() {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0, topY()));
    ImGui::SetNextWindowSize(ImVec2(kHierarchyW, display.y - topY() - statusBarH() - (m_showProject ? kProjectH : 0.0f)));
    beginRetroWindow("Hierarchy", &m_showHierarchy, kPanelFlags);

    if (ImGui::Button("+ Add"))
        ImGui::OpenPopup("add-object");
    if (ImGui::BeginPopup("add-object")) {
        for (const std::string& model : placeholderModels()) {
            if (ImGui::Selectable(modelStem(model).c_str())) {
                glm::vec3 o, d;
                const Rect r = sceneRect();
                mouseRay({r.x + r.w * 0.5f, r.y + r.h * 0.5f}, o, d);
                addEntity(model, placementPoint(o, d));
            }
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(m_selection.empty());
    if (ImGui::Button("Duplicate"))
        duplicateSelected();
    ImGui::SameLine();
    if (ImGui::Button("Delete"))
        deleteSelected();
    ImGui::EndDisabled();

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##filter", "Search...", &m_hierarchyFilter);
    sunkenBevelLastItem();

    const std::vector<std::string> issues = problems();
    const float problemsH = issues.empty() ? ImGui::GetFrameHeightWithSpacing() * 1.5f : 170.0f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1, 1, 1, 1));   // white list box, like Win98
    ImGui::BeginChild("tree", ImVec2(0, -problemsH));
    ImGui::PopStyleColor();

    if (ImGui::Selectable("Level settings", m_selected == kLevelSettings))
        selectOnly(kLevelSettings);
    ImGui::Separator();

    // Children per entity, and which entities are hidden inside a container.
    const int n = (int)m_level.entities.size();
    std::vector<std::vector<int>> children(n);
    std::vector<int> roots;
    for (int i = 0; i < n; ++i) {
        if (m_parents[i] >= 0)
            children[m_parents[i]].push_back(i);
        else
            roots.push_back(i);
    }
    std::vector<bool> inside(n, false);
    for (const Entity& e : m_level.entities) {
        if (e.interaction.type == "container") {
            const int k = m_level.indexOf(e.interaction.contains);
            if (k >= 0)
                inside[k] = true;
        }
    }

    if (m_hierarchyFilter.empty()) {
        for (int root : roots)
            drawTreeNode(root, children, inside);
    } else {
        // Searching: a flat list of matches.
        const std::string filter = toLower(m_hierarchyFilter);
        const std::vector<std::vector<int>> none(n);
        for (int i = 0; i < n; ++i)
            if (toLower(m_level.entities[i].id).find(filter) != std::string::npos)
                drawTreeNode(i, none, inside);
    }

    // Empty space below the tree: drop here to un-parent, click to deselect.
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("##root-drop", ImVec2(std::max(avail.x, 1.0f), std::max(avail.y, ImGui::GetFrameHeight())));
    if (ImGui::IsItemClicked())
        selectOnly(kNone);
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ENTITY"))
            reparent(m_level.indexOf(payloadString(p)), -1);
        ImGui::EndDragDropTarget();
    }
    ImGui::EndChild();
    sunkenBevelLastItem();

    if (m_deferDuplicate) {
        m_deferDuplicate = false;
        duplicateSelected();
    }
    if (m_deferDelete) {
        m_deferDelete = false;
        deleteSelected();
    }

    if (issues.empty()) {
        ImGui::TextDisabled("No problems found.");
    } else {
        ImGui::TextColored(kProblemColor, "Problems (%d)", (int)issues.size());
        ImGui::BeginChild("problems");
        ImGui::PushTextWrapPos(0.0f);
        for (const std::string& p : issues)
            ImGui::TextColored(kProblemColor, "- %s", p.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndChild();
    }
    ImGui::End();
}

void Editor::drawTreeNode(int i, const std::vector<std::vector<int>>& children, const std::vector<bool>& inside) {
    ImGuiIO& io = ImGui::GetIO();
    const std::string id = m_level.entities[i].id;   // copy: the list may change below
    const std::string type = m_level.entities[i].interaction.type;
    const bool selected = isSelected(i);
    const bool leaf = children[i].empty();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_DefaultOpen;
    if (leaf)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (selected)
        flags |= ImGuiTreeNodeFlags_Selected;

    // Win98 list selection: navy row, white text.
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.0f, 0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.0f, 0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.0f, 0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    }
    const bool open = ImGui::TreeNodeEx((void*)(intptr_t)i, flags, "%s", id.c_str());
    if (selected)
        ImGui::PopStyleColor(4);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
        if (io.KeyCtrl || io.KeyShift)
            toggleSelected(i);
        else
            selectOnly(i);
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        focusSelected();

    // Drag onto another object to make it a child, or onto an id field.
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("ENTITY", id.c_str(), id.size() + 1);
        ImGui::Text("%s", id.c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ENTITY"))
            reparent(m_level.indexOf(payloadString(p)), i);
        ImGui::EndDragDropTarget();
    }
    if (ImGui::BeginPopupContextItem()) {
        if (!selected)
            selectOnly(i);
        if (ImGui::MenuItem("Focus", "F")) focusSelected();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) m_deferDuplicate = true;
        if (ImGui::MenuItem("Delete", "Del")) m_deferDelete = true;
        if (ImGui::MenuItem("Un-parent", nullptr, false, m_parents[i] >= 0)) reparent(i, -1);
        ImGui::EndPopup();
    }

    // Puzzle type (and "inside" for hidden things), right-aligned and dimmed.
    std::string tag = type;
    if (inside[i])
        tag += tag.empty() ? "inside" : ", inside";
    if (!tag.empty()) {
        const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
        const ImVec2 ts = ImGui::CalcTextSize(tag.c_str());
        const ImU32 color = selected ? IM_COL32(200, 200, 255, 255) : ImGui::GetColorU32(ImGuiCol_TextDisabled);
        ImGui::GetWindowDrawList()->AddText(ImVec2(max.x - ts.x - 4.0f, min.y + 2.0f), color, tag.c_str());
    }

    if (open && !leaf) {
        for (int child : children[i])
            drawTreeNode(child, children, inside);
        ImGui::TreePop();
    }
}

// ---------- Inspector ----------

void Editor::drawInspector() {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(display.x - kInspectorW, topY()));
    ImGui::SetNextWindowSize(ImVec2(kInspectorW, display.y - topY() - statusBarH()));
    beginRetroWindow("Inspector", &m_showInspector, kPanelFlags);

    if (m_selected >= 0) {
        if (m_selection.size() > 1)
            ImGui::TextColored(ImVec4(0, 0, 0.5f, 1), "%d objects selected - the gizmo moves all of them.",
                               (int)m_selection.size());
        drawEntityInspector(m_level.entities[m_selected]);
    } else if (m_selected == kLevelSettings) {
        drawLevelInspector();
    } else if (!m_selectedItem.empty()) {
        drawItemInspector();
    } else {
        tip("Select an object in the Scene or the Hierarchy to see its Transform and puzzle settings.");
        ImGui::Spacing();
        tip("Select an item in the Project panel to edit its name and icon.");
    }
    ImGui::End();
}

void Editor::drawEntityInspector(Entity& e) {
    ImGui::SeparatorText("Object");

    if (m_idBufferFor != m_selected) {
        m_idBuffer = e.id;
        m_idBufferFor = m_selected;
    }
    labelColumn("Name");
    if (m_focusName) {   // F2 / Edit > Rename
        ImGui::SetKeyboardFocusHere();
        m_focusName = false;
    }
    ImGui::InputText("##id", &m_idBuffer);
    if (ImGui::IsItemDeactivatedAfterEdit() && !renameEntity((size_t)m_selected, m_idBuffer))
        m_idBuffer = e.id;

    labelColumn("Model");
    if (ImGui::BeginCombo("##model", e.model.empty() ? "(none - invisible)" : e.model.c_str())) {
        if (ImGui::Selectable("(none - invisible)", e.model.empty()))
            e.model.clear();
        for (const std::string& model : placeholderModels())
            if (ImGui::Selectable(model.c_str(), model == e.model))
                e.model = model;
        ImGui::EndCombo();
    }

    labelColumn("Parent");
    if (ImGui::BeginCombo("##parent", e.parent.empty() ? "(none)" : e.parent.c_str())) {
        if (ImGui::Selectable("(none)", e.parent.empty()))
            reparent(m_selected, -1);
        for (int j = 0; j < (int)m_level.entities.size(); ++j) {
            if (j == m_selected || isDescendant(j, m_selected))
                continue;   // can't be its own parent
            const std::string& id = m_level.entities[j].id;
            if (ImGui::Selectable(id.c_str(), id == e.parent))
                reparent(m_selected, j);
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Children move with their parent. You can also drag in the Hierarchy.");

    ImGui::Spacing();
    const std::string header = e.parent.empty() ? "Transform###transform"
                                                : "Transform (relative to " + e.parent + ")###transform";
    if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        Transform& t = e.transform;
        vec3Row("Position", t.position, 0.01f, 0.0f);
        vec3Row("Rotation", t.rotationDeg, 0.5f, 0.0f);
        if (vec3Row("Scale", t.scale, 0.01f, 1.0f))
            t.scale = glm::max(t.scale, glm::vec3(0.01f));

        if (ImGui::Button("Reset rotation + scale")) {
            t.rotationDeg = glm::vec3(0.0f);
            t.scale = glm::vec3(1.0f);
        }
        ImGui::SameLine();
        Aabb bounds;
        if (ImGui::Button("Drop to floor") && entityBounds(m_selected, bounds)) {
            // Lower it in world space by how far its bottom is above y = 0.
            glm::mat4 world = m_world[m_selected];
            world[3].y -= bounds.min.y;
            const glm::mat4 parentWorld = m_parents[m_selected] >= 0 ? m_world[m_parents[m_selected]] : glm::mat4(1.0f);
            t.position = glm::vec3((glm::inverse(parentWorld) * world)[3]);
        }
    }

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Puzzle", ImGuiTreeNodeFlags_DefaultOpen))
        drawInteractionInspector(e);
}

void Editor::drawInteractionInspector(Entity& e) {
    Interaction& in = e.interaction;
    static const char* types[] = {"", "pickup", "container", "readable", "keypad", "exit", "npc", "dice"};
    static const char* labels[] = {
        "None (scenery)", "Pickup - goes in the pocket", "Container - drawer, box",
        "Readable - note, sign", "Keypad - type a code", "Exit - door to win",
        "NPC - says one line", "Dice - roll for a code"};

    int current = 0;
    for (int k = 0; k < 8; ++k)
        if (in.type == types[k])
            current = k;
    labelColumn("Type");
    if (ImGui::BeginCombo("##type", labels[current])) {
        for (int k = 0; k < 8; ++k) {
            if (ImGui::Selectable(labels[k], k == current) && k != current) {
                Interaction fresh;
                fresh.type = types[k];
                fresh.name = e.id;
                if (fresh.type == "npc") fresh.lines = {"Hello!"};
                if (fresh.type == "keypad") fresh.code = "0000";
                in = fresh;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Spacing();

    const std::vector<std::string> items = itemIds();
    if (in.type == "pickup") {
        idField("Item", in.item, items);
        tip("Clicking it puts this item in the player's pocket.");
    } else if (in.type == "container") {
        labelColumn("Locked");
        ImGui::Checkbox("##locked", &in.locked);
        if (in.locked) {
            idField("Unlocked by", in.unlockedBy, items);
            tip("The player must hold this item. Or point a keypad's 'Opens' at this container.");
        }
        std::vector<std::string> containable = items;
        for (const std::string& id : entityIds({"pickup", "readable"}))
            containable.push_back(id);
        if (idField("Contains", in.contains, containable)) {
            // An object put inside becomes a child, so it moves with the container.
            const int k = m_level.indexOf(in.contains);
            if (k >= 0 && k != m_selected && m_level.entities[k].parent.empty() && !isDescendant(m_selected, k))
                reparent(k, m_selected);
        }
        tip("An item, or an object (it becomes a child, hidden until opened). "
            "Tip: drag an item from the Project panel onto the object.");
    } else if (in.type == "readable") {
        labelColumn("Text");
        ImGui::InputTextMultiline("##text", &in.text, ImVec2(-1.0f, ImGui::GetTextLineHeight() * 5.0f));
    } else if (in.type == "keypad") {
        labelColumn("Code from");
        const bool fromDice = !in.codeFrom.empty();
        if (ImGui::RadioButton("Fixed code", !fromDice))
            in.codeFrom.clear();
        ImGui::SameLine();
        if (ImGui::RadioButton("Dice total", fromDice) && !fromDice) {
            const std::vector<std::string> dice = entityIds({"dice"});
            in.codeFrom = dice.empty() ? "dice" : dice[0];
        }
        if (!in.codeFrom.empty()) {
            idField("Dice", in.codeFrom, entityIds({"dice"}));
            tip("The code is the total of the player's latest roll of these dice.");
        } else {
            labelColumn("Code");
            ImGui::InputText("##code", &in.code, ImGuiInputTextFlags_CharsDecimal);
        }
        idField("Opens", in.opens, entityIds({"exit", "container"}));
        tip("The door or container this keypad unlocks. Drag it here from the Hierarchy.");
    } else if (in.type == "exit") {
        idField("Unlocked by", in.unlockedBy, items);
        bool winning = m_level.winExit == e.id;
        labelColumn("Wins level");
        if (ImGui::Checkbox("##win", &winning))
            m_level.winExit = winning ? e.id : "";
        tip("Starts locked. A keypad that 'opens' it, or holding the item, unlocks it.");
    } else if (in.type == "npc") {
        labelColumn("Name");
        ImGui::InputText("##npcname", &in.name);
        ImGui::TextUnformatted("Lines (one per talk, then repeats):");
        int remove = -1;
        for (int k = 0; k < (int)in.lines.size(); ++k) {
            ImGui::PushID(k);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 4.0f);
            ImGui::InputText("##line", &in.lines[k]);
            ImGui::SameLine(0.0f, 4.0f);
            if (ImGui::Button("x", ImVec2(ImGui::GetFrameHeight(), 0.0f)))
                remove = k;
            ImGui::PopID();
        }
        if (remove >= 0)
            in.lines.erase(in.lines.begin() + remove);
        if (ImGui::Button("+ Add line"))
            in.lines.push_back("");
        idField("Gives", in.gives, items);
        tip("Item handed to the player on the first talk (optional).");
    } else if (in.type == "dice") {
        labelColumn("Count");
        ImGui::SliderInt("##count", &in.count, 1, 10);
        labelColumn("Sides");
        ImGui::SliderInt("##sides", &in.sides, 2, 20);
        tip("Rolls all dice at once. Use a keypad with 'Dice total' to make the sum a code.");
    } else {
        tip("Just scenery. Pick a type to make it part of a puzzle.");
    }
}

void Editor::drawLevelInspector() {
    ImGui::SeparatorText("Level settings");
    labelColumn("Title");
    ImGui::InputText("##name", &m_level.name);
    labelColumn("Author");
    ImGui::InputText("##author", &m_level.author);

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Player spawn", ImGuiTreeNodeFlags_DefaultOpen)) {
        vec3Row("Position", m_level.spawnPosition, 0.01f, 0.0f);
        labelColumn("Facing");
        ImGui::DragFloat("##yaw", &m_level.spawnYawDeg, 1.0f, -360.0f, 360.0f, "%.0f deg");
        if (ImGui::Button("Move spawn to camera")) {
            m_level.spawnPosition = m_camera.position;
            m_level.spawnYawDeg = m_camera.yawDeg;
        }
        tip("The blue marker in the scene. Move it with the gizmo; Rotate turns it. Y is eye height (about 1.6).");
    }

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Win", ImGuiTreeNodeFlags_DefaultOpen)) {
        idField("Exit", m_level.winExit, entityIds({"exit"}));
        labelColumn("Message");
        ImGui::InputText("##winmsg", &m_level.winMessage);
    }

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
        int remove = -1;
        for (int k = 0; k < (int)m_level.lights.size(); ++k) {
            Light& l = m_level.lights[k];
            ImGui::PushID(k);
            const bool point = l.type == Light::Type::Point;
            ImGui::SeparatorText(point ? "Point light (yellow marker)" : "Directional light (sun)");
            labelColumn("Type");
            int type = point ? 0 : 1;
            if (ImGui::Combo("##type", &type, "Point\0Directional\0"))
                l.type = type == 0 ? Light::Type::Point : Light::Type::Directional;
            if (l.type == Light::Type::Point)
                vec3Row("Position", l.position, 0.01f, 0.0f);
            else
                vec3Row("Direction", l.direction, 0.01f, 0.0f);
            labelColumn("Color");
            ImGui::ColorEdit3("##color", &l.color.x);
            labelColumn("Intensity");
            ImGui::DragFloat("##intensity", &l.intensity, 0.01f, 0.0f, 10.0f, "%.2f");
            if (ImGui::Button("Remove light"))
                remove = k;
            ImGui::PopID();
        }
        if (remove >= 0)
            m_level.lights.erase(m_level.lights.begin() + remove);
        if (ImGui::Button("+ Point light")) {
            Light l;
            l.position = m_camera.position + glm::normalize(m_camera.forward()) * 2.0f;
            m_level.lights.push_back(l);
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Directional light")) {
            Light l;
            l.type = Light::Type::Directional;
            l.direction = {-0.3f, -1.0f, -0.4f};
            l.intensity = 0.6f;
            m_level.lights.push_back(l);
        }
        tip("Up to 4 point lights and 1 directional light are used.");
    }
}

void Editor::drawItemInspector() {
    const std::string id = m_selectedItem;
    auto findDef = [this, &id]() -> ItemDef* {
        for (ItemDef& def : m_level.items)
            if (def.id == id)
                return &def;
        return nullptr;
    };
    auto ensureDef = [this, &id, &findDef]() -> ItemDef& {
        if (!findDef())
            m_level.items.push_back(ItemDef{id, id, ""});
        return *findDef();
    };

    ImGui::SeparatorText("Item (2D)");
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    drawItemTile(ImGui::GetWindowDrawList(), pos, 80.0f, id, m_level, m_icons, false, 0);
    ImGui::Dummy(ImVec2(80.0f, 80.0f));
    ImGui::Spacing();

    if (m_itemIdBufferFor != id) {
        m_itemIdBuffer = id;
        m_itemIdBufferFor = id;
    }
    labelColumn("Id");
    ImGui::InputText("##itemid", &m_itemIdBuffer);
    if (ImGui::IsItemDeactivatedAfterEdit() && !renameItem(id, m_itemIdBuffer))
        m_itemIdBuffer = id;

    const ItemDef* def = findDef();
    std::string name = def ? def->name : id;
    labelColumn("Name");
    if (ImGui::InputText("##name", &name))
        ensureDef().name = name;

    std::string icon = def ? def->icon : "";
    labelColumn("Icon");
    if (ImGui::InputText("##icon", &icon))
        ensureDef().icon = icon;
    tip("Optional PNG, relative to the level file, e.g. icons/key.png. Without one the item is a colored tile.");

    ImGui::Spacing();
    ImGui::SeparatorText("Used by");
    bool used = false;
    for (const Entity& e : m_level.entities) {
        const Interaction& in = e.interaction;
        std::string how;
        if (in.type == "pickup" && in.item == id) how = "pickup";
        else if (in.contains == id) how = "inside";
        else if (in.unlockedBy == id) how = "unlocks it";
        else if (in.gives == id) how = "gift";
        if (!how.empty()) {
            ImGui::BulletText("%s (%s)", e.id.c_str(), how.c_str());
            used = true;
        }
    }
    if (!used)
        tip("Nothing uses this item yet. Drag it from the Project panel onto an object.");

    ImGui::Spacing();
    if (ImGui::Button("Remove name and icon")) {
        m_level.items.erase(std::remove_if(m_level.items.begin(), m_level.items.end(),
                                           [&id](const ItemDef& d) { return d.id == id; }),
                            m_level.items.end());
    }
}

// ---------- Project panel ----------

void Editor::drawProject() {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0, display.y - statusBarH() - kProjectH));
    ImGui::SetNextWindowSize(ImVec2(display.x - (m_showInspector ? kInspectorW : 0.0f), kProjectH));
    beginRetroWindow("Project", &m_showProject, kPanelFlags);

    const float tile = 76.0f;
    const float labelH = ImGui::GetTextLineHeight() + 4.0f;
    const float gap = ImGui::GetStyle().ItemSpacing.x;

    if (ImGui::BeginTabBar("project-tabs")) {
        if (ImGui::BeginTabItem("Models (3D)")) {
            tip("Drag a model into the scene to place it, or click to add it in front of the camera.");
            const std::vector<std::string> models = placeholderModels();
            const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
            for (size_t i = 0; i < models.size(); ++i) {
                const std::string& model = models[i];
                ImGui::PushID((int)i);
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                if (ImGui::InvisibleButton("tile", ImVec2(tile, tile + labelH))) {
                    glm::vec3 o, d;
                    const Rect r = sceneRect();
                    mouseRay({r.x + r.w * 0.5f, r.y + r.h * 0.5f}, o, d);
                    addEntity(model, placementPoint(o, d));
                }
                const bool hovered = ImGui::IsItemHovered();
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("MODEL", model.c_str(), model.size() + 1);
                    ImGui::Text("Place %s", modelStem(model).c_str());
                    ImGui::EndDragDropSource();
                }

                // Tile: white Win98 icon box with the model's main color, name underneath.
                ImDrawList* draw = ImGui::GetWindowDrawList();
                const ImVec2 max(pos.x + tile, pos.y + tile);
                draw->AddRectFilled(pos, max, retro::kWhite);
                const glm::vec3 c = placeholderFor(model)[0].color;
                draw->AddRectFilled(ImVec2(pos.x + 16, pos.y + 16), ImVec2(max.x - 16, max.y - 16),
                                    ImGui::GetColorU32(ImVec4(c.r, c.g, c.b, 1.0f)));
                draw->AddRect(pos, max, hovered ? retro::kNavy : retro::kMid, 0.0f, 0, hovered ? 2.0f : 1.0f);
                const std::string name = modelStem(model);
                const ImVec2 ts = ImGui::CalcTextSize(name.c_str());
                draw->PushClipRect(ImVec2(pos.x, max.y), ImVec2(max.x, max.y + labelH), true);
                draw->AddText(ImVec2(pos.x + std::max((tile - ts.x) * 0.5f, 0.0f), max.y + 2.0f),
                              ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
                draw->PopClipRect();
                ImGui::PopID();

                if (i + 1 < models.size() && ImGui::GetItemRectMax().x + gap + tile < right)
                    ImGui::SameLine();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Items (2D)")) {
            tip("Drag an item onto an object: into a drawer, as the key for a lock, as an NPC's gift. "
                "Drop it on the floor to leave it lying there as a pickup. Click to edit it.");
            const std::vector<std::string> items = itemIds();
            const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
            for (size_t i = 0; i < items.size(); ++i) {
                const std::string& item = items[i];
                ImGui::PushID((int)i);
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                if (ImGui::InvisibleButton("tile", ImVec2(tile, tile + labelH))) {
                    selectOnly(kNone);
                    m_selectedItem = item;
                }
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("ITEM", item.c_str(), item.size() + 1);
                    ImGui::Text("%s", m_level.itemName(item).c_str());
                    ImGui::EndDragDropSource();
                }
                ImDrawList* draw = ImGui::GetWindowDrawList();
                const bool chosen = item == m_selectedItem;
                drawItemTile(draw, pos, tile, item, m_level, m_icons, chosen, 0);
                const std::string name = m_level.itemName(item);
                const ImVec2 ts = ImGui::CalcTextSize(name.c_str());
                const float tx = pos.x + std::max((tile - ts.x) * 0.5f, 0.0f);
                draw->PushClipRect(ImVec2(pos.x, pos.y + tile), ImVec2(pos.x + tile, pos.y + tile + labelH), true);
                if (chosen)   // Win95 icon label selection: navy box, white text
                    draw->AddRectFilled(ImVec2(tx - 2, pos.y + tile + 1), ImVec2(tx + ts.x + 2, pos.y + tile + labelH),
                                        retro::kNavy);
                draw->AddText(ImVec2(tx, pos.y + tile + 2.0f), chosen ? retro::kWhite : retro::kBlack, name.c_str());
                draw->PopClipRect();
                ImGui::PopID();
                ImGui::SameLine();
                if (ImGui::GetItemRectMax().x + gap + tile >= right)
                    ImGui::NewLine();
            }
            if (ImGui::Button("+ New item", ImVec2(tile, tile))) {
                std::string id = "item";
                for (int n = 2; std::find(items.begin(), items.end(), id) != items.end(); ++n)
                    id = "item-" + std::to_string(n);
                m_level.items.push_back(ItemDef{id, id, ""});
                selectOnly(kNone);
                m_selectedItem = id;
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

// ---------- Scene view ----------

// Where a new object should stand: the surface under the mouse if it is below
// eye level (floor, table top), otherwise the floor (y = 0) along the ray.
glm::vec3 Editor::placementPoint(const glm::vec3& origin, const glm::vec3& dir) const {
    float t = 0.0f;
    const int hit = raycast(m_scene, origin, dir, 100.0f, [](size_t) { return true; }, &t);
    if (hit >= 0) {
        const glm::vec3 p = origin + dir * t;
        if (p.y <= origin.y)
            return p;
    }
    if (dir.y < -1e-4f)
        return origin + dir * (-origin.y / dir.y);
    glm::vec3 p = origin + dir * 3.0f;
    p.y = 0.0f;
    return p;
}

void Editor::drawSceneView() {
    ImGuiIO& io = ImGui::GetIO();
    const Rect r = sceneRect();
    ImGui::SetNextWindowPos(ImVec2(r.x, r.y));
    ImGui::SetNextWindowSize(ImVec2(r.w, r.h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    // A see-through window over the 3D view: it owns the gizmo and catches
    // clicks and drops, but has no widgets (so the gizmo can be grabbed).
    ImGui::Begin("##scene", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse |
                 ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::PopStyleVar(2);
    m_sceneHovered = ImGui::IsWindowHovered();

    const glm::mat4 view = viewMatrix();
    const glm::mat4 proj = projMatrix();
    const glm::vec2 mouse(io.MousePos.x, io.MousePos.y);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const bool cameraDrag = m_looking || m_orbiting || m_panning || m_zoomDragging;

    // --- Gizmo on the selection ---
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(r.x, r.y, r.w, r.h);
    ImGuizmo::Enable(!cameraDrag && !m_boxSelecting);
    const bool snap = m_snap || io.KeyCtrl;
    float snapMove[3] = {0.25f, 0.25f, 0.25f};
    float snapRotate[3] = {15.0f, 15.0f, 15.0f};
    float snapScale[3] = {0.1f, 0.1f, 0.1f};

    if (m_selected >= 0) {
        glm::mat4 m = m_world[m_selected];
        static const ImGuizmo::OPERATION ops[3] = {ImGuizmo::TRANSLATE, ImGuizmo::ROTATE, ImGuizmo::SCALE};
        float* snapValues = m_gizmoOp == 0 ? snapMove : (m_gizmoOp == 1 ? snapRotate : snapScale);
        const ImGuizmo::MODE mode = (m_gizmoLocal || m_gizmoOp == 2) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), ops[m_gizmoOp], mode,
                                 glm::value_ptr(m), nullptr, snap ? snapValues : nullptr)) {
            // The same world-space change goes to every selected object (children follow their parents).
            const glm::mat4 change = m * glm::inverse(m_world[m_selected]);
            for (int i : topLevelSelection()) {
                const glm::mat4 newWorld = (i == m_selected) ? m : change * m_world[i];
                const glm::mat4 parentWorld = m_parents[i] >= 0 ? m_world[m_parents[i]] : glm::mat4(1.0f);
                Transform& t = m_level.entities[i].transform;
                const Transform moved = transformFromMatrix(glm::inverse(parentWorld) * newWorld, t.rotationDeg);
                // Keep only what this tool changes, so float noise can't creep into the rest.
                if (m_gizmoOp == 0 || (m_gizmoOp == 1 && i != m_selected)) t.position = moved.position;
                if (m_gizmoOp == 1) t.rotationDeg = moved.rotationDeg;
                if (m_gizmoOp == 2) t.scale = glm::max(moved.scale, glm::vec3(0.01f));
            }
            static const char* verbs[3] = {"Move ", "Rotate ", "Scale "};
            m_pendingLabel = verbs[m_gizmoOp] + (m_selection.size() > 1 ? std::to_string(m_selection.size()) + " objects"
                                                                         : m_level.entities[m_selected].id);
            refresh();
        }
    } else if (m_selected == kLevelSettings) {
        // The spawn point: move it, or turn it around Y.
        glm::mat4 m = glm::rotate(glm::translate(glm::mat4(1.0f), m_level.spawnPosition),
                                  glm::radians(m_level.spawnYawDeg), glm::vec3(0, 1, 0));
        const ImGuizmo::OPERATION op = m_gizmoOp == 1 ? ImGuizmo::ROTATE_Y : ImGuizmo::TRANSLATE;
        if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), op, ImGuizmo::WORLD,
                                 glm::value_ptr(m), nullptr, snap ? (op == ImGuizmo::TRANSLATE ? snapMove : snapRotate) : nullptr)) {
            m_level.spawnPosition = glm::vec3(m[3]);
            m_level.spawnYawDeg = glm::degrees(std::atan2(m[2].x, m[2].z));
            m_pendingLabel = "Move spawn";
        }
    }

    // --- Left click selects; left drag on empty space draws a selection box ---
    if (m_sceneHovered && !cameraDrag && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !io.KeyAlt && !io.KeyCtrl && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
        m_pressPending = true;
        m_pressPos = mouse;
        m_boxSelecting = false;
    }
    if (m_pressPending) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (glm::length(mouse - m_pressPos) > 5.0f)
                m_boxSelecting = true;
        } else {
            if (m_boxSelecting) {
                // Everything whose whole box is inside the rectangle.
                const glm::vec2 lo = glm::min(m_pressPos, mouse), hi = glm::max(m_pressPos, mouse);
                if (!io.KeyShift)
                    selectOnly(kNone);
                for (int i = 0; i < (int)m_level.entities.size(); ++i) {
                    Aabb b;
                    if (!entityBounds(i, b))
                        continue;
                    bool enclosed = true;
                    for (int c = 0; c < 8 && enclosed; ++c) {
                        const glm::vec3 corner((c & 1) ? b.max.x : b.min.x, (c & 2) ? b.max.y : b.min.y,
                                               (c & 4) ? b.max.z : b.min.z);
                        glm::vec2 s;
                        enclosed = worldToScreen(corner, s) && s.x >= lo.x && s.x <= hi.x && s.y >= lo.y && s.y <= hi.y;
                    }
                    if (enclosed && !isSelected(i)) {
                        m_selection.push_back(i);
                        m_selected = i;
                    }
                }
            } else {
                glm::vec3 o, d;
                mouseRay(m_pressPos, o, d);
                float hitDist = 0.0f;
                int hit = raycast(m_scene, o, d, 1000.0f, [](size_t) { return true; }, &hitDist);
                // The spawn marker is clickable too.
                const Aabb spawnBox{m_level.spawnPosition - glm::vec3(0.2f), m_level.spawnPosition + glm::vec3(0.2f)};
                float spawnDist = 0.0f;
                if (rayHits(o, d, spawnBox, spawnDist) && (hit < 0 || spawnDist < hitDist))
                    hit = kLevelSettings;
                if (io.KeyShift && hit >= 0)
                    toggleSelected(hit);
                else
                    selectOnly(hit);
            }
            m_pressPending = false;
            m_boxSelecting = false;
        }
    }
    if (m_boxSelecting) {
        const ImVec2 a(m_pressPos.x, m_pressPos.y), b(mouse.x, mouse.y);
        draw->AddRectFilled(ImMin(a, b), ImMax(a, b), IM_COL32(0, 0, 128, 50));
        draw->AddRect(ImMin(a, b), ImMax(a, b), retro::kNavy, 0.0f, 0, 1.5f);
    }

    // --- Drops from the Project panel ---
    const ImRect sceneBounds(ImVec2(r.x, r.y), ImVec2(r.x + r.w, r.y + r.h));
    if (ImGui::BeginDragDropTargetCustom(sceneBounds, ImGui::GetID("scene-drop"))) {
        glm::vec3 o, d;
        mouseRay(mouse, o, d);
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("MODEL")) {
            addEntity(payloadString(p), placementPoint(o, d));
        } else if (const ImGuiPayload* p2 = ImGui::AcceptDragDropPayload("ITEM")) {
            const int hit = raycast(m_scene, o, d, 1000.0f, [](size_t) { return true; });
            dropItemOn(hit, payloadString(p2), placementPoint(o, d));
        }
        ImGui::EndDragDropTarget();
    }

    // --- Outline around every selected object ---
    for (int i : m_selection) {
        for (const SceneBox& box : m_scene[i]) {
            glm::vec2 s[8];
            bool visible = true;
            for (int c = 0; c < 8 && visible; ++c) {
                const glm::vec3 corner((c & 1) ? 0.5f : -0.5f, (c & 2) ? 0.5f : -0.5f, (c & 4) ? 0.5f : -0.5f);
                visible = worldToScreen(glm::vec3(box.model * glm::vec4(corner, 1.0f)), s[c]);
            }
            if (!visible)
                continue;
            static const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                                             {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
            for (const auto& e : edges)
                draw->AddLine(ImVec2(s[e[0]].x, s[e[0]].y), ImVec2(s[e[1]].x, s[e[1]].y), kOutline, 2.0f);
        }
    }

    // --- Help lines at the bottom of the view ---
    const char* help[2] = {
        "Right drag + WASD/QE: fly   Alt/Ctrl + left drag: orbit   Middle drag: pan   Wheel: zoom",
        "Left drag: box select   Shift+click: add   F: focus   W/E/R: move/rotate/scale   Ctrl: snap"};
    const float lineH = ImGui::GetTextLineHeight();
    for (int k = 0; k < 2 && m_showHelpText; ++k) {
        const ImVec2 p(r.x + 8, r.y + r.h - (2 - k) * (lineH + 2) - 4);
        draw->AddText(ImVec2(p.x + 1, p.y + 1), IM_COL32(0, 0, 0, 200), help[k]);
        draw->AddText(p, IM_COL32(255, 255, 255, 230), help[k]);
    }

    ImGui::End();
}

void Editor::render(Renderer& renderer) {
    refresh();   // pick up this frame's edits
    const ImGuiIO& io = ImGui::GetIO();
    const Rect r = sceneRect();
    const float sx = io.DisplayFramebufferScale.x, sy = io.DisplayFramebufferScale.y;
    const GLint x = (GLint)(r.x * sx);
    const GLint y = (GLint)((io.DisplaySize.y - (r.y + r.h)) * sy);   // GL counts from the bottom
    const GLsizei w = (GLsizei)(r.w * sx), h = (GLsizei)(r.h * sy);

    glViewport(x, y, w, h);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, w, h);
    glClearColor(0.0f, 0.5f, 0.5f, 1.0f);   // Windows desktop teal, like the site
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

    renderer.begin(viewMatrix(), projMatrix(), m_level);
    renderer.drawScene(
        m_scene, [](size_t) { return true; },
        [this](size_t i, glm::vec3 color) {
            return isSelected((int)i) ? glm::mix(color, glm::vec3(1.0f, 0.85f, 0.2f), 0.3f) : color;
        });

    if (!m_showMarkers)
        return;

    // Spawn marker: blue cube with a nose pointing where the player looks.
    const glm::mat4 spawn = glm::rotate(glm::translate(glm::mat4(1.0f), m_level.spawnPosition),
                                        glm::radians(m_level.spawnYawDeg), glm::vec3(0, 1, 0));
    renderer.drawBox(glm::scale(spawn, glm::vec3(0.3f)), glm::vec3(0.2f, 0.45f, 1.0f), true);
    renderer.drawBox(glm::scale(glm::translate(spawn, glm::vec3(0, 0, 0.3f)), glm::vec3(0.08f, 0.08f, 0.3f)),
                     glm::vec3(0.5f, 0.75f, 1.0f), true);

    // Point lights: small yellow cubes.
    for (const Light& l : m_level.lights)
        if (l.type == Light::Type::Point)
            renderer.drawBox(glm::scale(glm::translate(glm::mat4(1.0f), l.position), glm::vec3(0.15f)),
                             glm::vec3(1.0f, 0.9f, 0.4f), true);
}

// ---------- Popups ----------

void Editor::confirmThen(const std::string& question, std::function<void()> action) {
    m_confirmQuestion = question;
    m_confirmAction = std::move(action);
    m_openConfirm = true;
}

void Editor::drawPopups() {
    // Popups are opened here, outside any panel, so open and draw share one ID scope.
    if (m_openSaveAs) { ImGui::OpenPopup("Save level as"); m_openSaveAs = false; }
    if (m_openConfirm) { ImGui::OpenPopup("Are you sure?"); m_openConfirm = false; }
    if (m_openQuitPrompt) { ImGui::OpenPopup("Unsaved changes"); m_openQuitPrompt = false; }
    if (m_openDropChoice) { ImGui::OpenPopup("drop-choice"); m_openDropChoice = false; }
    if (m_openOpenLevel) { ImGui::OpenPopup("Open Level"); m_openOpenLevel = false; }
    if (m_openControls) { ImGui::OpenPopup("Controls"); m_openControls = false; }
    if (m_openAbout) { ImGui::OpenPopup("About"); m_openAbout = false; }

    const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);

    // File > Open: every level in the levels folder, in a white Win95 list.
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (beginRetroPopupModal("Open Level", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static std::string chosen;
        ImGui::TextUnformatted("Levels folder:");
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1, 1, 1, 1));
        ImGui::BeginChild("files", ImVec2(420.0f, 260.0f));
        ImGui::PopStyleColor();
        std::error_code ec;
        std::string open;
        for (const auto& entry : fs::directory_iterator(LEVELS_DIR, ec)) {
            if (entry.path().extension() != ".json")
                continue;
            const std::string path = entry.path().generic_string();
            const bool selected = path == chosen;
            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0.5f, 1));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
            }
            if (ImGui::Selectable(entry.path().filename().string().c_str(), selected,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                chosen = path;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    open = path;
            }
            if (selected)
                ImGui::PopStyleColor(2);
        }
        ImGui::EndChild();
        sunkenBevelLastItem();
        ImGui::Spacing();
        ImGui::BeginDisabled(chosen.empty());
        if (ImGui::Button("Open"))
            open = chosen;
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        if (!open.empty()) {
            ImGui::CloseCurrentPopup();
            if (m_dirty)
                confirmThen("Discard unsaved changes and open " + fs::path(open).filename().string() + "?",
                            [this, open] { load(open); });
            else
                load(open);
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (beginRetroPopupModal("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static const char* rows[][2] = {
            {"Right drag + WASD / QE", "look around and fly (Shift = faster)"},
            {"Alt or Ctrl + left drag", "orbit around the selection"},
            {"Middle drag", "pan"},
            {"Mouse wheel, Alt + right drag", "zoom"},
            {"Left click / left drag", "select / box select (Shift adds)"},
            {"W / E / R", "move / rotate / scale tool"},
            {"Ctrl while dragging a handle", "snap"},
            {"F", "frame the selection"},
            {"F2", "rename"},
            {"Ctrl+C / X / V / D", "copy / cut / paste / duplicate"},
            {"Del", "delete (with children)"},
            {"Ctrl+Z / Ctrl+Y", "undo / redo"},
            {"Ctrl+N / O / S", "new / open / save level"},
            {"F5", "play the level"},
        };
        if (ImGui::BeginTable("controls", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
            for (const auto& row : rows) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row[0]);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row[1]);
            }
            ImGui::EndTable();
        }
        if (ImGui::Button("OK"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (beginRetroPopupModal("About", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Escape Room Engine - level editor");
        ImGui::TextDisabled("C++17, OpenGL 4.6, Dear ImGui, ImGuizmo");
        ImGui::TextDisabled("A personal project exploring real-time 3D graphics.");
        if (ImGui::Button("OK"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (beginRetroPopupModal("Save level as", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("File name (saved in the levels folder):");
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(340.0f);
        const bool enter = ImGui::InputText("##file", &m_saveAsName, ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::Button("Save") || enter) {
            ImGui::CloseCurrentPopup();
            saveAs(m_saveAsName);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (beginRetroPopupModal("Are you sure?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(m_confirmQuestion.c_str());
        if (ImGui::Button("Yes")) {
            ImGui::CloseCurrentPopup();
            if (m_confirmAction)
                m_confirmAction();
        }
        ImGui::SameLine();
        if (ImGui::Button("No"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (beginRetroPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Save changes to %s before quitting?", fs::path(m_path).filename().string().c_str());
        if (ImGui::Button("Save and quit")) {
            save();
            if (!m_dirty)
                m_quitConfirmed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Quit without saving")) {
            m_quitConfirmed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // An item dropped on a container: put it inside, or make it the key?
    if (ImGui::BeginPopup("drop-choice")) {
        if (m_dropEntity >= 0 && m_dropEntity < (int)m_level.entities.size()) {
            Entity& e = m_level.entities[m_dropEntity];
            const std::string item = m_level.itemName(m_dropItem);
            ImGui::TextDisabled("%s -> %s", item.c_str(), e.id.c_str());
            if (ImGui::Selectable(("Put it inside the " + e.id).c_str())) {
                e.interaction.contains = m_dropItem;
                setStatus("The " + e.id + " now contains " + item);
            }
            if (ImGui::Selectable(("Make it the key that unlocks the " + e.id).c_str())) {
                e.interaction.locked = true;
                e.interaction.unlockedBy = m_dropItem;
                setStatus("The " + e.id + " is now locked; " + item + " unlocks it");
            }
        }
        ImGui::EndPopup();
    }
}

// ---------- Editing ----------

std::string Editor::uniqueId(const std::string& base) const {
    const std::string root = base.empty() ? "object" : base;
    if (m_level.indexOf(root) < 0)
        return root;
    for (int n = 2;; ++n) {
        const std::string id = root + "-" + std::to_string(n);
        if (m_level.indexOf(id) < 0)
            return id;
    }
}

int Editor::addEntity(const std::string& model, const glm::vec3& floorPoint) {
    Entity e;
    e.id = uniqueId(modelStem(model));
    e.model = model;
    // Stand it on the point: lift by how far its boxes reach below the origin.
    float lowest = 0.0f;
    for (const PlaceholderBox& box : placeholderFor(model))
        lowest = std::min(lowest, box.offset.y - box.size.y * 0.5f);
    e.transform.position = floorPoint - glm::vec3(0.0f, lowest, 0.0f);
    e.interaction = defaultInteraction(model, e.id);
    m_level.entities.push_back(e);
    refresh();
    selectOnly((int)m_level.entities.size() - 1);
    setStatus("Added " + e.id);
    return m_selected;
}

glm::vec3 Editor::viewCenterPoint() const {
    const Rect r = sceneRect();
    glm::vec3 o, d;
    mouseRay({r.x + r.w * 0.5f, r.y + r.h * 0.5f}, o, d);
    return placementPoint(o, d);
}

// Copies each selected object with all its children (parents first).
void Editor::copySelected() {
    refresh();
    const std::vector<int> roots = topLevelSelection();
    if (roots.empty())
        return;
    m_clipboard.clear();
    for (int root : roots)
        for (int k : subtree(root))
            m_clipboard.push_back(m_level.entities[k]);
    setStatus("Copied " + std::to_string(m_clipboard.size()) + (m_clipboard.size() == 1 ? " object" : " objects"));
}

void Editor::cutSelected() {
    copySelected();
    deleteSelected();
    setStatus("Cut " + std::to_string(m_clipboard.size()) + (m_clipboard.size() == 1 ? " object" : " objects"));
}

// Adds the clipboard as new objects. Names get "-2" etc. if taken, and links
// inside the pasted group (parent, keypad opens, contains, dice) point at the
// new copies. Top objects keep their parent if it still exists.
void Editor::paste() {
    if (m_clipboard.empty())
        return;
    std::set<std::string> copiedIds;
    for (const Entity& e : m_clipboard)
        copiedIds.insert(e.id);

    std::map<std::string, std::string> renamed;
    const size_t firstNew = m_level.entities.size();
    for (const Entity& source : m_clipboard) {
        Entity copy = source;
        copy.id = uniqueId(source.id);
        renamed[source.id] = copy.id;
        m_level.entities.push_back(copy);
    }

    std::vector<int> roots;
    for (size_t c = firstNew; c < m_level.entities.size(); ++c) {
        Entity& e = m_level.entities[c];
        auto remap = [&renamed](std::string& id) {
            auto it = renamed.find(id);
            if (it != renamed.end())
                id = it->second;
        };
        const bool isRoot = !copiedIds.count(e.parent);
        if (isRoot) {
            if (m_level.indexOf(e.parent) < 0)
                e.parent.clear();
            e.transform.position.x += 0.5f;   // so the copy is visible next to the original
            roots.push_back((int)c);
        } else {
            remap(e.parent);
        }
        remap(e.interaction.opens);
        remap(e.interaction.codeFrom);
        remap(e.interaction.contains);
    }
    refresh();
    m_selection = roots;
    m_selected = roots.back();
    m_selectedItem.clear();
    setStatus(roots.size() == 1 ? "Pasted " + m_level.entities[m_selected].id
                                : "Pasted " + std::to_string(roots.size()) + " objects");
}

// Copy + paste, without touching the clipboard.
void Editor::duplicateSelected() {
    if (m_selection.empty())
        return;
    const std::vector<Entity> saved = m_clipboard;
    copySelected();
    paste();
    m_clipboard = saved;
    setStatus(m_selection.size() == 1 ? "Duplicate " + m_level.entities[m_selected].id
                                      : "Duplicate " + std::to_string(m_selection.size()) + " objects");
}

void Editor::selectAll() {
    m_selection.clear();
    for (int i = 0; i < (int)m_level.entities.size(); ++i)
        m_selection.push_back(i);
    m_selected = m_selection.empty() ? kNone : m_selection.back();
    m_selectedItem.clear();
}

void Editor::makeParent() {
    if (m_selected < 0 || m_selection.size() < 2)
        return;
    const int parent = m_selected;
    const std::vector<int> others = m_selection;
    for (int i : others)
        if (i != parent)
            reparent(i, parent);
    setStatus("Make " + m_level.entities[parent].id + " the parent");
}

void Editor::clearParent() {
    for (int i : topLevelSelection())
        reparent(i, -1);
    setStatus("Clear parent");
}

// An invisible object to group others under (like Unity's empty GameObject).
void Editor::createEmpty() {
    Entity e;
    e.id = uniqueId("empty");
    e.transform.position = viewCenterPoint();
    m_level.entities.push_back(e);
    refresh();
    selectOnly((int)m_level.entities.size() - 1);
    setStatus("Create " + e.id);
}

// Puts the selected object where the middle of the scene view points.
void Editor::moveToView() {
    if (m_selected < 0)
        return;
    glm::mat4 world = m_world[m_selected];
    world[3] = glm::vec4(viewCenterPoint(), 1.0f);
    Aabb bounds;
    if (entityBounds(m_selected, bounds))
        world[3].y += m_world[m_selected][3].y - bounds.min.y;   // stand on the spot, not in it
    const glm::mat4 parentWorld = m_parents[m_selected] >= 0 ? m_world[m_parents[m_selected]] : glm::mat4(1.0f);
    m_level.entities[m_selected].transform.position = glm::vec3((glm::inverse(parentWorld) * world)[3]);
    setStatus("Move " + m_level.entities[m_selected].id + " to view");
}

// Deletes the selected objects and all their children, like Unity.
void Editor::deleteSelected() {
    refresh();
    std::set<int> doomed;
    for (int root : topLevelSelection())
        for (int k : subtree(root))
            doomed.insert(k);
    if (doomed.empty())
        return;
    for (auto it = doomed.rbegin(); it != doomed.rend(); ++it)
        m_level.entities.erase(m_level.entities.begin() + *it);
    selectOnly(kNone);
    afterStructureChange();
    setStatus("Deleted " + std::to_string(doomed.size()) + (doomed.size() == 1 ? " object" : " objects") +
              " (Ctrl+Z to undo)");
}

bool Editor::renameEntity(size_t index, const std::string& newId) {
    const std::string oldId = m_level.entities[index].id;
    if (newId == oldId)
        return true;
    if (newId.empty()) {
        setStatus("A name can't be empty");
        return false;
    }
    if (m_level.indexOf(newId) >= 0) {
        setStatus("There is already an object called " + newId);
        return false;
    }
    m_level.entities[index].id = newId;
    // Keep every link pointing at it working.
    for (Entity& e : m_level.entities) {
        Interaction& in = e.interaction;
        if (e.parent == oldId) e.parent = newId;
        if (in.opens == oldId) in.opens = newId;
        if (in.codeFrom == oldId) in.codeFrom = newId;
        if (in.contains == oldId) in.contains = newId;
    }
    if (m_level.winExit == oldId)
        m_level.winExit = newId;
    setStatus("Renamed " + oldId + " to " + newId);
    return true;
}

bool Editor::renameItem(const std::string& oldId, const std::string& newId) {
    if (newId == oldId)
        return true;
    if (newId.empty()) {
        setStatus("An item id can't be empty");
        return false;
    }
    const std::vector<std::string> existing = itemIds();
    if (std::find(existing.begin(), existing.end(), newId) != existing.end()) {
        setStatus("There is already an item called " + newId);
        return false;
    }
    const bool oldIsEntity = m_level.indexOf(oldId) >= 0;
    for (ItemDef& def : m_level.items)
        if (def.id == oldId)
            def.id = newId;
    for (Entity& e : m_level.entities) {
        Interaction& in = e.interaction;
        if (in.item == oldId) in.item = newId;
        if (in.unlockedBy == oldId) in.unlockedBy = newId;
        if (in.gives == oldId) in.gives = newId;
        if (in.contains == oldId && !oldIsEntity) in.contains = newId;
    }
    m_selectedItem = newId;
    m_itemIdBufferFor = newId;
    setStatus("Renamed item " + oldId + " to " + newId);
    return true;
}

// Makes `child` a child of `newParent` (-1 = top level) without moving it in the world.
bool Editor::reparent(int child, int newParent) {
    refresh();
    if (child < 0 || child >= (int)m_level.entities.size())
        return false;
    Entity& e = m_level.entities[child];
    if (newParent == child || (newParent >= 0 && isDescendant(newParent, child))) {
        setStatus("Can't put " + e.id + " inside itself or its own child");
        return false;
    }
    const std::string newParentId = newParent >= 0 ? m_level.entities[newParent].id : "";
    if (e.parent == newParentId)
        return true;
    const glm::mat4 parentWorld = newParent >= 0 ? m_world[newParent] : glm::mat4(1.0f);
    e.transform = transformFromMatrix(glm::inverse(parentWorld) * m_world[child], e.transform.rotationDeg);
    e.parent = newParentId;
    refresh();
    setStatus(newParentId.empty() ? e.id + " is now top level" : e.id + " is now a child of " + newParentId);
    return true;
}

// An item dragged from the Project panel and dropped in the scene.
void Editor::dropItemOn(int entity, const std::string& item, const glm::vec3& point) {
    const std::string name = m_level.itemName(item);
    if (entity >= 0) {
        Entity& e = m_level.entities[entity];
        Interaction& in = e.interaction;
        selectOnly(entity);
        if (in.type == "container") {
            m_dropEntity = entity;
            m_dropItem = item;
            m_openDropChoice = true;   // inside, or the key? ask
            return;
        }
        if (in.type == "exit") {
            in.unlockedBy = item;
            setStatus("Holding " + name + " now unlocks the " + e.id);
            return;
        }
        if (in.type == "npc") {
            in.gives = item;
            setStatus(in.name + " now gives " + name);
            return;
        }
        if (in.type == "pickup") {
            in.item = item;
            setStatus("Picking up " + e.id + " now gives " + name);
            return;
        }
    }
    // Dropped on the floor or scenery: leave it lying there as a pickup.
    const int added = addEntity("item.glb", point);
    m_level.entities[added].interaction.item = item;
    setStatus(name + " placed as a pickup");
}

// ---------- Lists ----------

std::vector<std::string> Editor::itemIds() const {
    std::set<std::string> ids;
    for (const ItemDef& def : m_level.items)
        ids.insert(def.id);
    for (const Entity& e : m_level.entities) {
        const Interaction& in = e.interaction;
        if (in.type == "pickup") ids.insert(in.item);
        ids.insert(in.unlockedBy);
        ids.insert(in.gives);
        if (m_level.indexOf(in.contains) < 0) ids.insert(in.contains);
    }
    ids.erase("");
    return std::vector<std::string>(ids.begin(), ids.end());
}

std::vector<std::string> Editor::entityIds(const std::vector<std::string>& types) const {
    std::vector<std::string> out;
    for (const Entity& e : m_level.entities)
        if (std::find(types.begin(), types.end(), e.interaction.type) != types.end())
            out.push_back(e.id);
    return out;
}

// Things that would make the level unplayable or confusing, in plain words.
std::vector<std::string> Editor::problems() const {
    std::vector<std::string> out;
    auto typeOf = [this](const std::string& id) -> std::string {
        const int k = m_level.indexOf(id);
        if (k < 0)
            return "";   // no such object
        const std::string& type = m_level.entities[k].interaction.type;
        return type.empty() ? "scenery" : type;
    };

    std::set<std::string> ids, openedByKeypad;
    for (const Entity& e : m_level.entities) {
        if (!ids.insert(e.id).second)
            out.push_back("Two objects are called '" + e.id + "'.");
        if (e.interaction.type == "keypad")
            openedByKeypad.insert(e.interaction.opens);
    }

    for (size_t i = 0; i < m_level.entities.size(); ++i) {
        const Entity& e = m_level.entities[i];
        const Interaction& in = e.interaction;
        const std::string& id = e.id;
        if (!e.model.empty() && !hasPlaceholder(e.model))
            out.push_back(id + ": unknown model '" + e.model + "' (shown magenta).");
        if (!e.parent.empty()) {
            if (m_level.indexOf(e.parent) < 0)
                out.push_back(id + ": parent '" + e.parent + "' doesn't exist.");
            else if (i < m_parents.size() && m_parents[i] < 0)
                out.push_back(id + ": parent loop through '" + e.parent + "'.");
        }

        if (in.type == "pickup" && in.item.empty()) {
            out.push_back(id + ": pickup gives no item.");
        } else if (in.type == "container") {
            if (in.locked && in.unlockedBy.empty() && !openedByKeypad.count(id))
                out.push_back(id + ": locked, but nothing unlocks it.");
        } else if (in.type == "keypad") {
            if (in.opens.empty())
                out.push_back(id + ": keypad doesn't open anything.");
            else if (typeOf(in.opens).empty())
                out.push_back(id + ": opens '" + in.opens + "', which doesn't exist.");
            else if (typeOf(in.opens) != "exit" && typeOf(in.opens) != "container")
                out.push_back(id + ": opens '" + in.opens + "', which is not a door or container.");
            if (in.codeFrom.empty() && in.code.empty())
                out.push_back(id + ": keypad has no code.");
            if (!in.codeFrom.empty() && typeOf(in.codeFrom) != "dice")
                out.push_back(id + ": dice '" + in.codeFrom + "' not found.");
        } else if (in.type == "exit") {
            if (in.unlockedBy.empty() && !openedByKeypad.count(id))
                out.push_back(id + ": exit can never be unlocked.");
        } else if (in.type == "npc") {
            if (in.lines.empty())
                out.push_back(id + ": NPC has nothing to say.");
        } else if (in.type == "readable") {
            if (in.text.empty())
                out.push_back(id + ": note has no text.");
        }
    }

    if (m_level.winExit.empty())
        out.push_back("No winning exit (see Level settings).");
    else if (typeOf(m_level.winExit) != "exit")
        out.push_back("Winning exit '" + m_level.winExit + "' is not an exit.");
    return out;
}

// ---------- Files and history ----------

void Editor::save() {
    std::string error;
    if (!saveLevel(m_path, m_level, error)) {
        setStatus("Save failed: " + error);
        return;
    }
    m_savedJson = levelSnapshot(m_level);
    m_stableJson = m_savedJson;
    m_dirty = false;
    setStatus("Saved " + fs::path(m_path).filename().string());
}

void Editor::saveAs(const std::string& fileName) {
    std::string name = fileName;
    while (!name.empty() && std::isspace((unsigned char)name.back())) name.pop_back();
    while (!name.empty() && std::isspace((unsigned char)name.front())) name.erase(name.begin());
    if (name.empty() || name.find_first_of("/\\:") != std::string::npos) {
        setStatus("Please type a plain file name, like my-room.json");
        return;
    }
    if (fs::path(name).extension() != ".json")
        name += ".json";
    const std::string path = std::string(LEVELS_DIR) + "/" + name;

    auto doSave = [this, path] {
        m_path = path;
        m_level.baseDir = folderOf(path);
        save();
    };
    std::error_code ec;
    if (fs::exists(path, ec) && fs::path(path) != fs::path(m_path))
        confirmThen(name + " already exists. Replace it?", doSave);
    else
        doSave();
}

void Editor::load(const std::string& path) {
    Level loaded;
    std::string error;
    if (!loadLevel(path, loaded, error)) {
        setStatus("Load failed: " + error);
        return;
    }
    m_level = std::move(loaded);
    m_path = path;
    selectOnly(kNone);
    afterStructureChange();
    resetHistory();
    resetCamera();
    setStatus("Opened " + fs::path(path).filename().string());
}

void Editor::newLevel() {
    Level level;
    level.name = "New Room";
    level.spawnPosition = {0.0f, 1.6f, 2.2f};
    level.spawnYawDeg = 180.0f;

    Light sun;
    sun.type = Light::Type::Directional;
    sun.direction = {-0.3f, -1.0f, -0.4f};
    sun.intensity = 0.6f;
    Light lamp;
    lamp.position = {0.0f, 2.6f, 0.0f};
    lamp.color = {1.0f, 0.9f, 0.8f};
    lamp.intensity = 1.2f;
    level.lights = {sun, lamp};

    Entity floor;
    floor.id = "floor";
    floor.model = "floor.glb";
    floor.transform.scale = {6.0f, 1.0f, 6.0f};
    Entity walls;
    walls.id = "walls";
    walls.model = "room.glb";
    Entity door;
    door.id = "door";
    door.model = "door.glb";
    door.transform.position = {0.0f, 1.0f, -3.0f};
    door.interaction.type = "exit";
    Entity keypad;
    keypad.id = "keypad";
    keypad.model = "keypad.glb";
    keypad.parent = "door";
    keypad.transform.position = {0.6f, 0.2f, 0.0f};
    keypad.interaction.type = "keypad";
    keypad.interaction.code = "1234";
    keypad.interaction.opens = "door";
    level.entities = {floor, walls, door, keypad};
    level.winExit = "door";
    level.winMessage = "You escaped!";
    level.baseDir = LEVELS_DIR;

    // A free file name so Save never overwrites an existing level by accident.
    std::string path = std::string(LEVELS_DIR) + "/untitled.json";
    std::error_code ec;
    for (int n = 2; fs::exists(path, ec); ++n)
        path = std::string(LEVELS_DIR) + "/untitled-" + std::to_string(n) + ".json";

    m_level = std::move(level);
    m_path = path;
    selectOnly(kNone);
    afterStructureChange();
    resetHistory();
    m_savedJson.clear();   // not on disk yet
    m_dirty = true;
    resetCamera();
    setStatus("New level - Save writes " + fs::path(path).filename().string());
}

void Editor::resetHistory() {
    m_undo.clear();
    m_redo.clear();
    m_stableJson = levelSnapshot(m_level);
    m_savedJson = m_stableJson;
    m_dirty = false;
}

// After each finished edit (no field being dragged or typed in, no gizmo held),
// remember the level as it was before, for Undo.
void Editor::trackHistory() {
    if (ImGui::IsAnyItemActive() || ImGuizmo::IsUsing())
        return;
    const std::string now = levelSnapshot(m_level);
    if (now != m_stableJson) {
        // Name the step after the action that caused it, or what was selected.
        std::string label = m_pendingLabel;
        if (label.empty())
            label = m_selected >= 0 ? "Change " + m_level.entities[m_selected].id
                                    : (m_selected == kLevelSettings ? "Change level settings" : "Change");
        m_undo.push_back({m_stableJson, label});
        if (m_undo.size() > kMaxUndo)
            m_undo.erase(m_undo.begin());
        m_redo.clear();
        m_stableJson = now;
    }
    m_pendingLabel.clear();
    m_dirty = m_stableJson != m_savedJson;
}

static bool restoreSnapshot(Level& level, const std::string& snapshot) {
    Level restored;
    std::string error;
    if (!parseLevel(snapshot, level.baseDir, restored, error))
        return false;
    level = std::move(restored);
    return true;
}

void Editor::undo() {
    if (m_undo.empty())
        return;
    const HistoryEntry step = m_undo.back();
    m_undo.pop_back();
    if (!restoreSnapshot(m_level, step.snapshot))
        return;
    m_redo.push_back({m_stableJson, step.label});
    m_stableJson = step.snapshot;
    m_dirty = m_stableJson != m_savedJson;
    afterStructureChange();
    setStatus("Undo " + step.label);
}

void Editor::redo() {
    if (m_redo.empty())
        return;
    const HistoryEntry step = m_redo.back();
    m_redo.pop_back();
    if (!restoreSnapshot(m_level, step.snapshot))
        return;
    m_undo.push_back({m_stableJson, step.label});
    m_stableJson = step.snapshot;
    m_dirty = m_stableJson != m_savedJson;
    afterStructureChange();
    setStatus("Redo " + step.label);
}

void Editor::jumpToHistory(size_t undoCount) {
    while (m_undo.size() > undoCount)
        undo();
    while (m_undo.size() < undoCount && !m_redo.empty())
        redo();
}
