#pragma once
#include "Camera.h"
#include "Level.h"
#include "Scene.h"
#include <functional>
#include <string>
#include <vector>

class IconCache;
class Renderer;

// Unity-style level editor:
//   toolbar (top)       - Play, Move / Rotate / Scale, Local / World, Undo, Save, level files
//   Hierarchy (left)    - parent / child tree of objects; Level settings; problems
//   Scene view (middle) - the 3D room; click / box-select, drag gizmo handles,
//                         drop models and items from the Project panel
//   Inspector (right)   - Transform (Position / Rotation / Scale) and puzzle settings
//   Project (bottom)    - 3D models and 2D items to drag into the scene
class Editor {
public:
    enum class Action { None, Play };

    // `level` and `levelPath` are edited in place (Load / New / Save as change them).
    Editor(Level& level, std::string& levelPath, IconCache& icons);

    // Panels, gizmo, camera, picking, shortcuts. Call between ImGui::NewFrame
    // and ImGui::Render.
    Action frame(GLFWwindow* window, float dt);

    // Draws the 3D scene view. Call after frame().
    void render(Renderer& renderer);

    bool dirty() const { return m_dirty; }
    void askToQuit() { m_openQuitPrompt = true; }
    bool quitConfirmed() const { return m_quitConfirmed; }

    // Puts the editor camera where the level's player spawns.
    void resetCamera();

private:
    struct Rect { float x, y, w, h; };

    // One undo step: the level before (undo list) or after (redo list) a change.
    struct HistoryEntry {
        std::string snapshot;
        std::string label;   // "Move drawer", "Added dice", ...
    };

    void refresh();   // rebuild scene boxes, world matrices, parent table
    float topY() const;       // bottom of the menu bar + toolbar
    Rect sceneRect() const;
    glm::mat4 viewMatrix() const { return m_camera.view(); }
    glm::mat4 projMatrix() const;
    void mouseRay(const glm::vec2& mouse, glm::vec3& origin, glm::vec3& dir) const;
    bool worldToScreen(const glm::vec3& p, glm::vec2& out) const;
    bool entityBounds(int entity, Aabb& out) const;   // false if it has no boxes

    // Selection: m_selected is the primary (shown in the Inspector, carries
    // the gizmo); m_selection is everything selected.
    bool isSelected(int entity) const;
    void selectOnly(int entity);          // also kNone / kLevelSettings
    void toggleSelected(int entity);
    std::vector<int> topLevelSelection() const;   // selected, minus children of selected
    bool isDescendant(int entity, int ancestor) const;
    std::vector<int> subtree(int root) const;     // root first, then children

    // Panels
    void drawMenuBar(Action& action);
    void drawHistoryWindow();
    void drawStatusBar();
    Action drawToolbar();
    void drawHierarchy();
    void drawTreeNode(int entity, const std::vector<std::vector<int>>& children, const std::vector<bool>& inside);
    void drawInspector();
    void drawEntityInspector(Entity& e);
    void drawInteractionInspector(Entity& e);
    void drawLevelInspector();
    void drawItemInspector();
    void drawProject();
    void drawSceneView();
    void drawPopups();

    // Camera and shortcuts
    void updateCamera(GLFWwindow* window, float dt);
    glm::vec3 orbitPivot() const;
    float distanceUnderMouse(const glm::vec2& mouse) const;
    void handleShortcuts();
    void focusSelected();

    // Editing
    std::string uniqueId(const std::string& base) const;
    int addEntity(const std::string& model, const glm::vec3& floorPoint);
    glm::vec3 viewCenterPoint() const;   // where the middle of the scene view points
    void duplicateSelected();
    void deleteSelected();
    void copySelected();                 // selected objects + their children
    void cutSelected();
    void paste();
    void selectAll();
    void makeParent();                   // primary becomes parent of the other selected
    void clearParent();
    void createEmpty();
    void moveToView();
    bool renameEntity(size_t index, const std::string& newId);
    bool renameItem(const std::string& oldId, const std::string& newId);
    bool reparent(int child, int newParent);   // keeps the world position
    void dropItemOn(int entity, const std::string& item, const glm::vec3& point);
    glm::vec3 placementPoint(const glm::vec3& origin, const glm::vec3& dir) const;

    // Lists for dropdowns
    std::vector<std::string> itemIds() const;
    std::vector<std::string> entityIds(const std::vector<std::string>& types) const;
    std::vector<std::string> problems() const;

    // Files and history
    void save();
    void saveAs(const std::string& fileName);
    void load(const std::string& path);
    void newLevel();
    void confirmThen(const std::string& question, std::function<void()> action);
    void resetHistory();
    void trackHistory();
    void undo();
    void redo();
    void jumpToHistory(size_t undoCount);   // undo / redo until this many steps are done
    void afterStructureChange();   // drop stale selection / buffers
    void setStatus(const std::string& text);

    Level& m_level;
    std::string& m_path;
    IconCache& m_icons;
    Scene m_scene;
    std::vector<glm::mat4> m_world;
    std::vector<int> m_parents;
    FpsCamera m_camera;

    int m_selected = -1;
    std::vector<int> m_selection;
    std::string m_selectedItem;        // item chosen in the Project panel
    int m_gizmoOp = 0;                 // 0 move, 1 rotate, 2 scale
    bool m_gizmoLocal = false;
    bool m_snap = false;
    std::string m_hierarchyFilter;
    std::string m_idBuffer;            // id being typed in the Inspector
    int m_idBufferFor = -1;
    std::string m_itemIdBuffer;
    std::string m_itemIdBufferFor;

    // Camera drags in the scene view
    bool m_sceneHovered = false;
    bool m_looking = false;            // right mouse: look + WASD fly
    bool m_orbiting = false;           // Alt / Ctrl + left mouse
    bool m_panning = false;            // middle mouse
    bool m_zoomDragging = false;       // Alt + right mouse
    glm::vec3 m_pivot{0.0f};
    float m_pivotDistance = 5.0f;
    double m_lastMouseX = 0.0, m_lastMouseY = 0.0;

    // Left click / box select in the scene view
    bool m_pressPending = false;
    glm::vec2 m_pressPos{0.0f};
    bool m_boxSelecting = false;

    std::vector<HistoryEntry> m_undo, m_redo;
    std::string m_pendingLabel;        // name for the next undo step (set by actions)
    std::string m_stableJson;          // level as of the last finished edit
    std::string m_savedJson;           // level as last saved / loaded
    bool m_dirty = false;

    std::string m_status;
    double m_statusTime = -100.0;

    bool m_openSaveAs = false;
    std::string m_saveAsName;
    bool m_openConfirm = false;
    std::string m_confirmQuestion;
    std::function<void()> m_confirmAction;
    bool m_openQuitPrompt = false;
    bool m_quitConfirmed = false;
    GLFWwindow* m_window = nullptr;
    std::vector<Entity> m_clipboard;   // copied objects, parents before children

    // Window menu: which panels are shown; View menu: overlays.
    bool m_showHierarchy = true;
    bool m_showInspector = true;
    bool m_showProject = true;
    bool m_showHistory = false;
    bool m_showHelpText = true;
    bool m_showMarkers = true;
    bool m_openOpenLevel = false;
    bool m_openControls = false;
    bool m_openAbout = false;
    bool m_focusName = false;          // F2: jump into the Name field

    // Tree context-menu actions run after the tree is drawn (they change the list).
    bool m_deferDelete = false;
    bool m_deferDuplicate = false;
    bool m_openDropChoice = false;     // item dropped on a container: inside or key?
    int m_dropEntity = -1;
    std::string m_dropItem;
};
