#pragma once

#include <cstddef>
#include <vector>

namespace fsae::math {

/// Interpolador cúbico de Hermite por partes que preserva a forma (PCHIP),
/// equivalente a `griddedInterpolant(x, y, 'pchip')` do MATLAB, inclusive a
/// extrapolação (usa o polinômio do primeiro/último intervalo).
///
/// Derivadas nos nós: média harmônica ponderada de Fritsch–Carlson e fórmula
/// de três pontos nas pontas (`pchipend`). Reproduz o MATLAB bit a bit.
class PchipInterpolant {
public:
    PchipInterpolant() = default;
    /// `x` estritamente crescente, `x.size() == y.size() >= 2`.
    PchipInterpolant(std::vector<double> x, std::vector<double> y);

    [[nodiscard]] double operator()(double xq) const noexcept;

    [[nodiscard]] const std::vector<double>& x() const noexcept { return x_; }
    [[nodiscard]] const std::vector<double>& y() const noexcept { return y_; }

private:
    std::vector<double> x_, y_;
    // Coeficientes por intervalo: v = ((c3*t + c2)*t + c1)*t + y_k, t = xq - x_k
    std::vector<double> c3_, c2_, c1_;
};

/// Interpolação linear por partes com extrapolação linear, equivalente a
/// `griddedInterpolant(x, y, 'linear')` e a `interp1(x, y, xq, 'linear', 'extrap')`.
class LinearInterpolant {
public:
    LinearInterpolant() = default;
    /// `x` estritamente crescente, `x.size() == y.size() >= 2`.
    LinearInterpolant(std::vector<double> x, std::vector<double> y);

    [[nodiscard]] double operator()(double xq) const noexcept;

    [[nodiscard]] const std::vector<double>& x() const noexcept { return x_; }
    [[nodiscard]] const std::vector<double>& y() const noexcept { return y_; }

private:
    std::vector<double> x_, y_;
};

/// Equivalente a `linspace(a, b, n)` do MATLAB.
/// `n == 0` devolve vazio; `n == 1` devolve `{b}`.
[[nodiscard]] std::vector<double> linspace(double a, double b, std::size_t n);

}  // namespace fsae::math
