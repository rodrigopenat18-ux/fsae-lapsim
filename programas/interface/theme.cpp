#include "theme.hpp"

#include <array>
#include <cstdint>

#include <implot.h>

namespace fsae::gui {

namespace {

Theme g_theme = Theme::Light;

ImVec4 hex(std::uint32_t c, float a = 1.0f) {
    auto ch = [c](int shift) { return static_cast<float>(static_cast<int>((c >> shift) & 0xffu)) / 255.0f; };
    return ImVec4(ch(16), ch(8), ch(0), a);
}

// Paleta categórica de referência: mesmas 8 cores, passos para fundo claro e escuro.
constexpr std::array<std::uint32_t, 8> kLight = {0x2a78d6, 0xeb6834, 0x1baf7a, 0xeda100,
                                                 0xe87ba4, 0x008300, 0x6250d6, 0xe34948};
constexpr std::array<std::uint32_t, 8> kDark = {0x3987e5, 0xd95926, 0x199e70, 0xc98500,
                                                0xd55181, 0x008300, 0x9085e9, 0xe66767};

void light() {
    ImGui::StyleColorsLight();
    ImVec4* c = ImGui::GetStyle().Colors;
    const ImVec4 ink = hex(0x1f1f1d), ink2 = hex(0x6b6a65);
    const ImVec4 bg = hex(0xefeee9), panel = hex(0xfbfaf7), field = hex(0xffffff);
    const ImVec4 neutral = hex(0xe3e1da), neutral_h = hex(0xd6e4f2), neutral_a = hex(0xbfd5ec);
    const ImVec4 accent = hex(0x2a78d6);
    c[ImGuiCol_Text] = ink;
    c[ImGuiCol_TextDisabled] = ink2;
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = panel;
    c[ImGuiCol_PopupBg] = field;
    c[ImGuiCol_MenuBarBg] = hex(0xe6e4dd);
    c[ImGuiCol_Border] = hex(0xd3d1c9);
    c[ImGuiCol_FrameBg] = field;
    c[ImGuiCol_FrameBgHovered] = hex(0xf1f5fa);
    c[ImGuiCol_FrameBgActive] = hex(0xe4eef9);
    c[ImGuiCol_TitleBg] = hex(0xe3e1da);
    c[ImGuiCol_TitleBgActive] = hex(0xd9d6cc);
    c[ImGuiCol_Button] = neutral;
    c[ImGuiCol_ButtonHovered] = neutral_h;
    c[ImGuiCol_ButtonActive] = neutral_a;
    c[ImGuiCol_Header] = hex(0xe8e6df);
    c[ImGuiCol_HeaderHovered] = neutral_h;
    c[ImGuiCol_HeaderActive] = neutral_a;
    c[ImGuiCol_Tab] = hex(0xe0ded6);
    c[ImGuiCol_TabHovered] = neutral_h;
    c[ImGuiCol_TabActive] = panel;
    c[ImGuiCol_TabUnfocused] = hex(0xe0ded6);
    c[ImGuiCol_TabUnfocusedActive] = panel;
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = hex(0x1f5fae);
    c[ImGuiCol_Separator] = hex(0xd3d1c9);
    c[ImGuiCol_TableHeaderBg] = hex(0xe8e6df);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(0, 0, 0, 0.025f);
    c[ImGuiCol_TableBorderStrong] = hex(0xd3d1c9);
    c[ImGuiCol_TableBorderLight] = hex(0xe3e1da);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.35f);

    ImPlot::StyleColorsLight();
    ImPlotStyle& p = ImPlot::GetStyle();
    p.Colors[ImPlotCol_PlotBg] = field;
    p.Colors[ImPlotCol_FrameBg] = ImVec4(0, 0, 0, 0);
    p.Colors[ImPlotCol_PlotBorder] = hex(0xd3d1c9);
    p.Colors[ImPlotCol_AxisGrid] = ImVec4(0, 0, 0, 0.07f);
    p.Colors[ImPlotCol_LegendBg] = ImVec4(1, 1, 1, 0.92f);
    p.Colors[ImPlotCol_LegendBorder] = hex(0xd3d1c9);
}

void dark() {
    ImGui::StyleColorsDark();
    ImVec4* c = ImGui::GetStyle().Colors;
    const ImVec4 bg = hex(0x1a1a19), panel = hex(0x222220), field = hex(0x2c2c29);
    const ImVec4 neutral = hex(0x3a3a36), neutral_h = hex(0x4a4a45), neutral_a = hex(0x5a5a54);
    c[ImGuiCol_Text] = hex(0xf0efec);
    c[ImGuiCol_TextDisabled] = hex(0xa3a29a);
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = panel;
    c[ImGuiCol_PopupBg] = panel;
    c[ImGuiCol_MenuBarBg] = hex(0x262624);
    c[ImGuiCol_Border] = hex(0x3a3a36);
    c[ImGuiCol_FrameBg] = field;
    c[ImGuiCol_FrameBgHovered] = hex(0x34342f);
    c[ImGuiCol_FrameBgActive] = hex(0x3c3c37);
    c[ImGuiCol_TitleBg] = panel;
    c[ImGuiCol_TitleBgActive] = hex(0x2e2e2b);
    c[ImGuiCol_Button] = neutral;
    c[ImGuiCol_ButtonHovered] = neutral_h;
    c[ImGuiCol_ButtonActive] = neutral_a;
    c[ImGuiCol_Header] = hex(0x30302c);
    c[ImGuiCol_HeaderHovered] = neutral_h;
    c[ImGuiCol_HeaderActive] = neutral_a;
    c[ImGuiCol_Tab] = hex(0x2c2c29);
    c[ImGuiCol_TabHovered] = neutral_h;
    c[ImGuiCol_TabActive] = hex(0x3a3a36);
    c[ImGuiCol_TabUnfocused] = hex(0x2c2c29);
    c[ImGuiCol_TabUnfocusedActive] = hex(0x3a3a36);
    c[ImGuiCol_CheckMark] = hex(0x3987e5);
    c[ImGuiCol_SliderGrab] = hex(0x3987e5);
    c[ImGuiCol_Separator] = hex(0x3a3a36);
    c[ImGuiCol_TableHeaderBg] = hex(0x30302c);

    ImPlot::StyleColorsDark();
    ImPlotStyle& p = ImPlot::GetStyle();
    p.Colors[ImPlotCol_PlotBg] = bg;
    p.Colors[ImPlotCol_FrameBg] = ImVec4(0, 0, 0, 0);
    p.Colors[ImPlotCol_AxisGrid] = ImVec4(1, 1, 1, 0.08f);
}

}  // namespace

void apply_theme(Theme t) {
    g_theme = t;
    ImGuiStyle& s = ImGui::GetStyle();
    const ImGuiStyle sizes = s;  // preserva tamanhos/escala já ajustados
    if (t == Theme::Light) light();
    else dark();
    ImVec4 colors[ImGuiCol_COUNT];
    for (int i = 0; i < ImGuiCol_COUNT; ++i) colors[i] = s.Colors[i];
    s = sizes;
    for (int i = 0; i < ImGuiCol_COUNT; ++i) s.Colors[i] = colors[i];
    // No tema claro os campos precisam de borda (senão caixas vazias somem no fundo claro)
    s.FrameBorderSize = t == Theme::Light ? 1.0f : 0.0f;
    ImPlot::GetStyle().LineWeight = 2.0f;
}

Theme current_theme() { return g_theme; }

ImVec4 run_palette(int slot) {
    const auto& p = g_theme == Theme::Light ? kLight : kDark;
    return hex(p[static_cast<std::size_t>(slot) % p.size()]);
}

ImVec4 color_error() { return g_theme == Theme::Light ? hex(0xc0392b) : hex(0xe66767); }
ImVec4 color_warning() { return g_theme == Theme::Light ? hex(0x9a6400) : hex(0xe0a020); }
ImVec4 color_ok() { return g_theme == Theme::Light ? hex(0x13865d) : hex(0x2fbf87); }
ImVec4 color_muted() { return g_theme == Theme::Light ? hex(0x8a8982) : hex(0x8f8e88); }
ImVec4 color_track() { return g_theme == Theme::Light ? hex(0xd5d3cb) : hex(0x55554f); }
ImVec4 color_highlight_row() {
    return g_theme == Theme::Light ? ImVec4(0.93f, 0.63f, 0.0f, 0.22f) : ImVec4(0.6f, 0.4f, 0.0f, 0.45f);
}

bool primary_button(const char* label, const ImVec2& size) {
    const ImVec4 base = g_theme == Theme::Light ? hex(0x13865d) : hex(0x199e70);
    ImGui::PushStyleColor(ImGuiCol_Button, base);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g_theme == Theme::Light ? hex(0x17a06f) : hex(0x22b582));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, g_theme == Theme::Light ? hex(0x0f6e4c) : hex(0x15855e));
    ImGui::PushStyleColor(ImGuiCol_Text, hex(0xffffff));
    const bool r = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return r;
}

}  // namespace fsae::gui
