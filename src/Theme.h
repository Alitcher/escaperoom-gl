#pragma once

struct ImDrawList;
struct ImVec2;

// Retro Windows 95 / 98 look for all ImGui windows (editor panels and game
// popups), matching the portfolio site: gray #c0c0c0 windows, navy title bars
// with white text (gray when not focused), raised buttons, sunken white lists,
// dotted tree lines. Call once after ImGui::CreateContext.
void applyRetroTheme();

// UI fonts to pick from (View > Font in the editor). A font whose file is
// missing falls back to ImGui's built-in font.
enum class UiFont { Arial, TimesNewRoman, CourierNew };
void setUiFont(UiFont font);   // takes effect from the next frame
UiFont currentUiFont();

// ImGui draws title text in the normal text color (black here). These draw it
// white on the navy title bar. Use them instead of ImGui::Begin / BeginPopupModal
// for any window with a title bar; end with ImGui::End / EndPopup as usual.
bool beginRetroWindow(const char* name, bool* open = nullptr, int flags = 0);
bool beginRetroPopupModal(const char* name, bool* open = nullptr, int flags = 0);

// Win95 bevels: sunken = dark top-left, white bottom-right (lists, text boxes);
// raised = the opposite (buttons, panels).
void sunkenBevel(ImDrawList* draw, const ImVec2& min, const ImVec2& max);
void sunkenBevelLastItem();

// Win98 palette, for code that draws its own widgets.
namespace retro {
constexpr unsigned int kNavy = 0xFF800000;      // #000080 (ImU32 is ABGR)
constexpr unsigned int kWhite = 0xFFFFFFFF;
constexpr unsigned int kLight = 0xFFDFDFDF;     // bevel highlight
constexpr unsigned int kGray = 0xFFC0C0C0;      // window / button gray
constexpr unsigned int kMid = 0xFF808080;       // bevel shadow
constexpr unsigned int kDark = 0xFF404040;      // bevel dark edge
constexpr unsigned int kBlack = 0xFF000000;
}
