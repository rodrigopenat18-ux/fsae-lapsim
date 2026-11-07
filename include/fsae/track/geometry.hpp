#pragma once

#include <cstddef>
#include <vector>

#include "fsae/track/track.hpp"

/// Traçado X/Y/Z da pista (buildTrack do MATLAB, sem os gráficos).
namespace fsae::track {

/// Pontos por segmento usados pelo MATLAB.
inline constexpr int kPointsPerSegment = 150;

struct TrackGeometry {
    std::vector<double> x, y, z;  ///< pontos do traçado [m], `kPointsPerSegment` por segmento
    std::vector<double> heading;  ///< direção do carro em cada ponto [rad]
    std::vector<double> u, v;     ///< 50 vetores de direção (cos, sin) para desenhar setas
    std::vector<std::size_t> vector_index;  ///< índice (base 0) do ponto de cada vetor

    [[nodiscard]] std::size_t size() const noexcept { return x.size(); }
};

/// Gera o traçado partindo de (0,0,0) apontando para +X.
/// Aqui a inclinação é usada como ângulo em radianos (dz = ds·tan(slope)),
/// diferente do initializeTrack (KNOWN_ISSUES.md, item 1).
[[nodiscard]] TrackGeometry build_geometry(const std::vector<Segment>& segments);

/// Segmento (1 = primeiro) mais próximo do ponto (x, y), como a escolha do ponto de
/// largada por clique no runSimulation.
[[nodiscard]] int nearest_segment(const TrackGeometry& geometry, std::size_t segment_count,
                                  double x, double y);

}  // namespace fsae::track
