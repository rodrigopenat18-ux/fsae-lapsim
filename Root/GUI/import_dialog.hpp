#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "IO_Tools/table_import.hpp"
#include "Vehicle/car.hpp"

namespace fsae::gui {

/// O que está sendo importado.
enum class ImportTarget { Torque, Bsfc, Gears };

/// Janela de importação de curvas a partir de planilha (CSV/TXT/XLSX) ou da área de
/// transferência: escolha da planilha, das colunas e da unidade, com prévia.
class ImportDialog {
public:
    /// Abre a partir de um arquivo (mostra o erro na própria janela se falhar).
    void open_file(ImportTarget target, const std::filesystem::path& path);
    /// Abre a partir de texto colado (Ctrl+C no Excel).
    void open_text(ImportTarget target, const std::string& text);

    /// Desenha a janela (modal). Devolve true quando os dados foram aplicados em `spec`.
    bool draw(fsae::CarSpec& spec);

private:
    void load();
    void guess_columns();
    /// Pontos válidos (x, y) já convertidos e ordenados; mensagem de erro se inválidos.
    bool collect(std::vector<double>& x, std::vector<double>& y, std::vector<double>& z,
                 std::string& error) const;

    bool pending_open_ = false;
    ImportTarget target_ = ImportTarget::Torque;
    std::filesystem::path path_;
    std::string text_;           ///< conteúdo colado (quando não há arquivo)
    std::vector<std::string> sheets_;
    int sheet_ = 0;
    fsae::io::DataTable table_;
    std::string load_error_;
    int col_x_ = 0, col_y_ = 1, col_z_ = -1;  ///< z: rpm de subida (marchas), -1 = manter
    int unit_y_ = 0;
    double factor_x_ = 1.0;
};

}  // namespace fsae::gui
