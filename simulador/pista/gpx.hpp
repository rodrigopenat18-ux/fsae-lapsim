#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "pista/track.hpp"

/// Inclinação dos segmentos a partir da altitude de um GPX (applyAltitudeFromGPX do
/// script Plot_GPX.m).
namespace fsae::track {

struct GpxPoint {
    double lat = 0;  ///< [graus]
    double lon = 0;  ///< [graus]
    double ele = 0;  ///< altitude [m]
};

/// Lê os `<trkpt lat=".." lon="..">` com `<ele>` de um arquivo GPX.
[[nodiscard]] std::vector<GpxPoint> load_gpx(const std::filesystem::path& path);
[[nodiscard]] std::vector<GpxPoint> parse_gpx(const std::string& xml, const std::string& name = "gpx");

struct GpxSlopeResult {
    std::vector<Segment> segments;  ///< segmentos de entrada com a inclinação [rad] preenchida
    std::vector<double> alt_start;  ///< altitude no início de cada segmento [m]
    std::vector<double> alt_end;    ///< altitude no fim de cada segmento [m]
    std::vector<double> delta_z;    ///< desnível corrigido de cada segmento [m]
    double z_error_total = 0;       ///< diferença de altitude entre o fim e o começo da volta [m]
};

/// Mesma lógica do applyAltitudeFromGPX:
///  1. suaviza a altitude com média móvel centrada de 5 pontos;
///  2. projeta lat/lon num plano local e acumula a distância percorrida no GPX;
///  3. interpola a altitude nas junções dos segmentos (por distância, com extrapolação);
///  4. distribui o erro de fechamento da volta proporcionalmente ao comprimento;
///  5. inclinação = atan(Δz / L).
/// Supõe que o GPX começa na largada e percorre a pista no mesmo sentido dos segmentos.
[[nodiscard]] GpxSlopeResult apply_gpx_altitude(const std::vector<Segment>& segments,
                                                const std::vector<GpxPoint>& points,
                                                int smoothing_window = 5);

}  // namespace fsae::track
