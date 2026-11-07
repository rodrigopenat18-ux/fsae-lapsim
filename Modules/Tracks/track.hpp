#pragma once

#include <filesystem>
#include <string>
#include <vector>

/// Descrição da pista em segmentos (matriz `segments` do MATLAB).
namespace fsae::track {

enum class SegmentType : int { Straight = 0, Left = 1, Right = 2 };

/// Um trecho da pista.
struct Segment {
    double length = 0;  ///< comprimento [m] (coluna 2)
    double radius = 0;  ///< raio [m]; 0 em reta (coluna 3)
    double slope = 0;   ///< inclinação (coluna 4) — ver docs/problemas_conhecidos.md, item 1
    SegmentType type = SegmentType::Straight;  ///< (coluna 5)

    /// O MATLAB considera curva qualquer segmento com raio diferente de zero.
    [[nodiscard]] bool is_corner() const noexcept { return radius != 0; }
};

struct Track {
    std::string name;
    std::vector<Segment> segments;

    [[nodiscard]] double length() const noexcept;
};

/// Lê uma pista. Dois formatos são aceitos (detectados automaticamente):
///
/// 1. CSV do projeto (cabeçalho obrigatório; linhas iniciadas com `#` são comentários):
///    ```
///    length_m,radius_m,slope,type
///    228.05,0,0,straight
///    57.551087148523706,28.08,0,right
///    ```
/// 2. Matriz `segments` do MATLAB em CSV, com cabeçalho `index,length,radius,slope,type`
///    e tipo numérico (0 reta, 1 esquerda, 2 direita). A coluna `index` é ignorada.
/// 3. Formato do `trackExcel.m` / OptimumLap: `Tipo;Comprimento;Raio`, sem cabeçalho,
///    tipo `Left`/`Right`/`Straight` (maiúsculas ou minúsculas), inclinação 0.
///
/// Lança std::runtime_error com o número da linha em caso de erro.
[[nodiscard]] Track load_track(const std::filesystem::path& path);

/// Igual a `load_track`, a partir do texto do arquivo.
[[nodiscard]] Track parse_track(const std::string& text, const std::string& name = "pista");

/// Grava no formato CSV do projeto (números na menor forma que preserva o valor exato).
void save_track(const Track& track, const std::filesystem::path& path,
                const std::string& header_comment = "");
[[nodiscard]] std::string track_to_csv(const Track& track, const std::string& header_comment = "");

/// Começa a volta no segmento `start_segment` (1 = sem mudança), girando a lista
/// como o `circshift(segments, -(k-1), 1)` do runSimulation.
[[nodiscard]] std::vector<Segment> rotate_to_start(const std::vector<Segment>& segments,
                                                   int start_segment);

[[nodiscard]] const char* to_string(SegmentType type) noexcept;

}  // namespace fsae::track
