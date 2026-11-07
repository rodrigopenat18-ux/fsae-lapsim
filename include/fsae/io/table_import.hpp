#pragma once

#include <filesystem>
#include <string>
#include <vector>

/// Leitura de tabelas numéricas de planilhas para importar curvas (torque, BSFC,
/// relações de marcha): CSV/TXT (separador `,`, `;` ou tabulação, decimal com ponto ou
/// vírgula), Excel `.xlsx` e texto colado do Excel.
namespace fsae::io {

struct DataTable {
    std::vector<std::string> columns;           ///< nome de cada coluna (cabeçalho ou "Coluna A")
    std::vector<std::vector<double>> values;    ///< values[coluna][linha]; NaN se não for número
    std::size_t first_data_row = 0;             ///< linha (base 0) do arquivo onde os dados começam

    [[nodiscard]] std::size_t rows() const noexcept { return values.empty() ? 0 : values[0].size(); }
    [[nodiscard]] std::size_t cols() const noexcept { return columns.size(); }
};

/// Converte células de texto em tabela numérica.
///
/// Os dados começam na primeira linha com pelo menos duas células numéricas (ou uma, se
/// a tabela tiver uma coluna só); a linha imediatamente anterior, se tiver texto, vira o
/// cabeçalho. Linhas de título acima são ignoradas, e linhas totalmente vazias no meio
/// dos dados também.
[[nodiscard]] DataTable table_from_cells(const std::vector<std::vector<std::string>>& cells);

/// Texto delimitado (CSV, TXT ou o que o Excel coloca na área de transferência).
/// O separador é detectado (tabulação, `;` ou `,`). Com `;` ou tabulação, vírgula
/// decimal ("12,5") também é aceita.
[[nodiscard]] DataTable parse_delimited_table(const std::string& text);

/// Nomes das planilhas de um arquivo `.xlsx`, na ordem do arquivo.
[[nodiscard]] std::vector<std::string> xlsx_sheet_names(const std::filesystem::path& path);

/// Lê a planilha `sheet` (base 0) de um `.xlsx`.
[[nodiscard]] DataTable read_xlsx_table(const std::filesystem::path& path, std::size_t sheet = 0);

/// Lê `.xlsx` ou texto delimitado conforme a extensão.
[[nodiscard]] DataTable read_table_file(const std::filesystem::path& path, std::size_t sheet = 0);

}  // namespace fsae::io
