#pragma once

#include <imgui.h>

/// Temas da interface (claro e escuro) e as cores que dependem do tema.
namespace fsae::gui {

enum class Theme { Light = 0, Dark = 1 };

/// Aplica o tema ao ImGui e ao ImPlot.
void apply_theme(Theme t);
[[nodiscard]] Theme current_theme();

/// Cor fixa de uma execução (paleta categórica validada, passos de cada tema).
[[nodiscard]] ImVec4 run_palette(int slot);

[[nodiscard]] ImVec4 color_error();
[[nodiscard]] ImVec4 color_warning();
[[nodiscard]] ImVec4 color_ok();
[[nodiscard]] ImVec4 color_muted();       ///< linhas de referência nos gráficos
[[nodiscard]] ImVec4 color_track();       ///< traçado da pista no mapa
[[nodiscard]] ImVec4 color_highlight_row();

/// Botão de ação principal (verde), para "Rodar simulação" e "Aplicar".
bool primary_button(const char* label, const ImVec2& size = ImVec2(0, 0));

}  // namespace fsae::gui
