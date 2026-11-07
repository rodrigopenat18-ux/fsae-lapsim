#pragma once

#include <vector>

#include "pista/track.hpp"
#include "carro/car.hpp"

/// Velocidade máxima nas curvas (calculateMaxSafeCurveSpeed / initializeTrack).
namespace fsae::track {

struct CornerSpeed {
    double v_max = 0;   ///< velocidade máxima na curva [m/s] = min(v_grip, v_tip)
    double v_grip = 0;  ///< limite por aderência lateral (com downforce) [m/s]
    double v_tip = 0;   ///< limite por roda descarregada (carga ≤ 0,01 N) [m/s]

    /// O MATLAB avisa quando o limite foi o tombamento e não a aderência.
    [[nodiscard]] bool limited_by_tipping() const noexcept { return v_tip < (v_grip - 0.2); }
};

/// Velocidade máxima numa curva de raio `radius` [m] e inclinação `slope` [rad].
///
/// Aderência: solução analítica de m·v²/R = μy·(m·g·cos θ + K·v²). Se o denominador
/// m − μy·K·R ficar negativo (R grande), o resultado é 0 — ver docs/problemas_conhecidos.md, item 2.
/// Tombamento: testa v = 1; 1,5; 2… m/s até v_grip e para na primeira velocidade em
/// que alguma roda fica com carga ≤ 0,01 N (v_tip = essa velocidade − 0,5).
[[nodiscard]] CornerSpeed max_safe_corner_speed(const Car& car, double radius,
                                                double slope) noexcept;

/// Velocidade máxima de cada segmento (initializeTrack): infinito nas retas.
/// Atenção: aqui a inclinação do segmento é tratada como porcentagem,
/// θ = atan(slope/100), como no MATLAB (docs/problemas_conhecidos.md, item 1).
[[nodiscard]] std::vector<CornerSpeed> corner_speeds(const Car& car,
                                                     const std::vector<Segment>& segments);

/// Velocidade da próxima curva depois do segmento `index` (base 0), ou infinito
/// se não houver mais curvas (find_next_corner_speed).
[[nodiscard]] double next_corner_speed(const std::vector<Segment>& segments,
                                       const std::vector<double>& corner_velocity,
                                       std::size_t index) noexcept;

}  // namespace fsae::track
