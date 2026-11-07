#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Simulation/lap.hpp"
#include "Tracks/geometry.hpp"
#include "Tracks/track.hpp"
#include "Vehicle/car.hpp"

/// Gravação dos resultados de uma simulação.
namespace fsae::io {

/// Contexto da simulação, gravado no summary.json.
struct RunInfo {
    std::string car_name;
    std::string car_file;    ///< vazio = carro padrão
    std::string track_name;
    std::string track_file;
    std::string event;
    int start_segment = 1;
};

/// telemetry.csv: uma linha por amostra, colunas com os nomes do `SimOut.History` do
/// MATLAB, mais `lap` (número da volta da amostra). Números na menor forma que
/// preserva o valor exato.
void write_telemetry_csv(const std::filesystem::path& path, const sim::SimulationResult& result);

/// summary.json: resultados (`SimOut.Results`), tempos e consumo por volta, e o contexto.
void write_summary_json(const std::filesystem::path& path, const sim::SimulationResult& result,
                        const RunInfo& info);
[[nodiscard]] std::string summary_json(const sim::SimulationResult& result, const RunInfo& info);

/// track.csv: segmentos usados (já com o segmento de largada aplicado) e a velocidade
/// máxima de cada um.
void write_segments_csv(const std::filesystem::path& path,
                        const std::vector<track::Segment>& segments,
                        const sim::SimulationResult& result);

/// geometry.csv: traçado X/Y/Z (buildTrack) para gráficos.
void write_geometry_csv(const std::filesystem::path& path, const track::TrackGeometry& geometry);

/// Menor representação decimal que reproduz o double exatamente ("inf", "nan" para não finitos).
[[nodiscard]] std::string format_number(double v);

}  // namespace fsae::io
