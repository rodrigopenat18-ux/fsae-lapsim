#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

/// Funções que dependem do sistema operacional (diálogos de arquivo, pastas).
namespace fsae::gui {

/// Diálogo nativo "Abrir" (Windows). Em outros sistemas devolve vazio e a interface
/// usa o campo de texto do caminho. `filter_desc`/`filter_ext`: ex. "Pistas", "*.csv".
std::optional<std::filesystem::path> open_file_dialog(const char* title, const char* filter_desc,
                                                      const char* filter_ext);
/// Diálogo nativo "Salvar como".
std::optional<std::filesystem::path> save_file_dialog(const char* title, const char* filter_desc,
                                                      const char* filter_ext,
                                                      const char* default_name);
/// Abre a pasta no explorador de arquivos.
void open_folder(const std::filesystem::path& dir);

/// Pasta do executável.
std::filesystem::path executable_dir();
/// Pastas `data` candidatas: $FSAE_DATA_DIR, ./data e subindo a partir do executável.
std::vector<std::filesystem::path> data_dirs();
/// Primeira pasta `data/<sub>` existente (vazio se nenhuma).
std::filesystem::path find_data_subdir(const std::string& sub);

/// Arquivo de preferências da interface (%APPDATA%\fsae_lapsim\gui.ini no Windows).
std::filesystem::path settings_path();

/// UTF-8 para exibir caminhos na interface.
std::string to_utf8(const std::filesystem::path& p);

}  // namespace fsae::gui
