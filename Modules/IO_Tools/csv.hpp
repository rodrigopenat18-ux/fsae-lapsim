#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace fsae::io {

/// Tabela numérica lida de um CSV com cabeçalho, armazenada por colunas.
class CsvTable {
public:
    CsvTable() = default;
    CsvTable(std::vector<std::string> header, std::vector<std::vector<double>> columns);

    [[nodiscard]] std::size_t rows() const noexcept;
    [[nodiscard]] std::size_t cols() const noexcept { return header_.size(); }
    [[nodiscard]] const std::vector<std::string>& header() const noexcept { return header_; }

    [[nodiscard]] bool has_column(std::string_view name) const noexcept;
    /// Índice da coluna; lança std::out_of_range se não existir.
    [[nodiscard]] std::size_t column_index(std::string_view name) const;
    [[nodiscard]] const std::vector<double>& column(std::string_view name) const;
    [[nodiscard]] const std::vector<double>& column(std::size_t index) const;

private:
    std::vector<std::string> header_;
    std::vector<std::vector<double>> columns_;
};

/// Lê um CSV numérico com uma linha de cabeçalho.
/// Aceita `Inf`, `-Inf` e `NaN`. Lança std::runtime_error em caso de erro.
CsvTable read_numeric_csv(const std::filesystem::path& path, char delimiter = ',');

/// Lê um CSV de duas colunas `key,value` (com cabeçalho) num mapa.
std::map<std::string, double> read_key_value_csv(const std::filesystem::path& path,
                                                 char delimiter = ',');

}  // namespace fsae::io
