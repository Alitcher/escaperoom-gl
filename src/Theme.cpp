#include "Theme.h"
#include <imgui.h>
#include <filesystem>

static ImVec4 hex(unsigned int rgb, float alpha = 1.0f) {
    return ImVec4(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, alpha);
}

static ImFont* s_fonts[3] = {nullptr, nullptr, nullptr};
static UiFont s_current = UiFont::Arial;

void setUiFont(UiFont font) {
    s_current = font;
    ImGui::GetIO().FontDefault = s_fonts[(int)font];   // nullptr = built-in font
}

UiFont currentUiFont() {
    return s_current;
}

bool beginRetroWindow(const char* name, bool* open, int flags) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));   // only the title bar is drawn now
    const bool visible = ImGui::Begin(name, open, flags);
    ImGui::PopStyleColor();
    return visible;
}

bool beginRetroPopupModal(const char* name, bool* open, int flags) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    const bool visible = ImGui::BeginPopupModal(name, open, flags);
    ImGui::PopStyleColor();
    return visible;
}

void sunkenBevel(ImDrawList* draw, const ImVec2& min, const ImVec2& max) {
    const ImVec2 a(min.x - 1, min.y - 1), b(max.x, max.y);
    draw->AddLine(a, ImVec2(b.x, a.y), retro::kMid);            // top
    draw->AddLine(a, ImVec2(a.x, b.y), retro::kMid);            // left
    draw->AddLine(ImVec2(a.x, b.y), b, retro::kWhite);          // bottom
    draw->AddLine(ImVec2(b.x, a.y), ImVec2(b.x, b.y + 1), retro::kWhite);   // right
}

void sunkenBevelLastItem() {
    sunkenBevel(ImGui::GetWindowDrawList(), ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
}

void applyRetroTheme() {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    // Windows fonts; Arial is the default (closest to Win95's MS Sans Serif).
    static const char* files[3] = {"C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/times.ttf",
                                   "C:/Windows/Fonts/cour.ttf"};
    for (int i = 0; i < 3; ++i) {
        std::error_code ec;
        if (std::filesystem::exists(files[i], ec))
            s_fonts[i] = io.Fonts->AddFontFromFileTTF(files[i], 13.0f);
    }
    style.FontSizeBase = 13.0f;
    setUiFont(UiFont::Arial);

    // Square corners everywhere; 1 px borders give the bevel (see Border colors).
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 0.0f;   // children get a drawn sunken bevel instead
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 1.0f;
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.ScrollbarSize = 16.0f;
    style.TreeLinesFlags = ImGuiTreeNodeFlags_DrawLinesToNodes;   // Explorer-style tree lines
    style.TreeLinesSize = 1.0f;

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = hex(0x000000);
    c[ImGuiCol_TextDisabled] = hex(0x606060);
    c[ImGuiCol_WindowBg] = hex(0xC0C0C0);
    c[ImGuiCol_ChildBg] = hex(0xC0C0C0, 0.0f);
    c[ImGuiCol_PopupBg] = hex(0xC0C0C0);
    // ImGui draws BorderShadow 1 px down-right of Border: white top-left +
    // dark bottom-right = the raised Win95 bevel on buttons.
    c[ImGuiCol_Border] = hex(0xFFFFFF);
    c[ImGuiCol_BorderShadow] = hex(0x404040);

    // Text boxes are white, like Win95 edit fields.
    c[ImGuiCol_FrameBg] = hex(0xFFFFFF);
    c[ImGuiCol_FrameBgHovered] = hex(0xFFFFFF);
    c[ImGuiCol_FrameBgActive] = hex(0xFFFFFF);

    // Focused window: navy title bar; the others: gray, like Win95.
    c[ImGuiCol_TitleBg] = hex(0x808080);
    c[ImGuiCol_TitleBgActive] = hex(0x000080);
    c[ImGuiCol_TitleBgCollapsed] = hex(0x808080);
    c[ImGuiCol_MenuBarBg] = hex(0xC0C0C0);

    c[ImGuiCol_ScrollbarBg] = hex(0xDFDFDF);
    c[ImGuiCol_ScrollbarGrab] = hex(0xC0C0C0);
    c[ImGuiCol_ScrollbarGrabHovered] = hex(0xC8C8C8);
    c[ImGuiCol_ScrollbarGrabActive] = hex(0xB0B0B0);

    c[ImGuiCol_CheckMark] = hex(0x000000);
    c[ImGuiCol_SliderGrab] = hex(0xC0C0C0);
    c[ImGuiCol_SliderGrabActive] = hex(0x808080);

    c[ImGuiCol_Button] = hex(0xC0C0C0);
    c[ImGuiCol_ButtonHovered] = hex(0xC8C8C8);
    c[ImGuiCol_ButtonActive] = hex(0xB0B0B0);

    // Section headers: a slightly darker gray bar.
    c[ImGuiCol_Header] = hex(0xB4B4B4);
    c[ImGuiCol_HeaderHovered] = hex(0xC8C8C8);
    c[ImGuiCol_HeaderActive] = hex(0xA8A8A8);

    c[ImGuiCol_Separator] = hex(0x808080);
    c[ImGuiCol_SeparatorHovered] = hex(0x000080);
    c[ImGuiCol_SeparatorActive] = hex(0x000080);
    c[ImGuiCol_ResizeGrip] = hex(0x000000, 0.0f);
    c[ImGuiCol_ResizeGripHovered] = hex(0x000080, 0.4f);
    c[ImGuiCol_ResizeGripActive] = hex(0x000080, 0.8f);

    // Tabs: the selected one is the window gray so it joins the page below.
    c[ImGuiCol_Tab] = hex(0xB0B0B0);
    c[ImGuiCol_TabHovered] = hex(0xC8C8C8);
    c[ImGuiCol_TabSelected] = hex(0xC0C0C0);
    c[ImGuiCol_TabSelectedOverline] = hex(0xC0C0C0);
    c[ImGuiCol_TabDimmed] = hex(0xB0B0B0);
    c[ImGuiCol_TabDimmedSelected] = hex(0xC0C0C0);

    c[ImGuiCol_TreeLines] = hex(0x808080);
    c[ImGuiCol_TextSelectedBg] = hex(0x000080, 0.35f);
    c[ImGuiCol_DragDropTarget] = hex(0x000080);
    c[ImGuiCol_ModalWindowDimBg] = hex(0x000000, 0.35f);
}
