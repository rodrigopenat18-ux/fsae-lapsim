#include "Math/interp.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fsae::math {

namespace {

void validate_grid(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size()) throw std::invalid_argument("interp: x e y com tamanhos diferentes");
    if (x.size() < 2) throw std::invalid_argument("interp: sao necessarios pelo menos 2 pontos");
    for (std::size_t i = 1; i < x.size(); ++i) {
        if (!(x[i] > x[i - 1])) {
            throw std::invalid_argument("interp: x deve ser estritamente crescente");
        }
    }
}

/// Índice do intervalo [x_k, x_{k+1}) que contém xq; fora da faixa usa o
/// primeiro/último intervalo (mesma regra do ppval do MATLAB).
std::size_t find_interval(const std::vector<double>& x, double xq) noexcept {
    const auto it = std::upper_bound(x.begin(), x.end(), xq);
    std::size_t k = (it == x.begin()) ? 0 : static_cast<std::size_t>(it - x.begin()) - 1;
    const std::size_t last = x.size() - 2;
    return std::min(k, last);
}

double sign(double v) noexcept { return (v > 0.0) - (v < 0.0); }

/// pchipend do MATLAB: derivada na ponta por fórmula de três pontos,
/// com as correções que preservam a forma.
double pchip_end(double h1, double h2, double del1, double del2) noexcept {
    double d = ((2.0 * h1 + h2) * del1 - h1 * del2) / (h1 + h2);
    if (sign(d) != sign(del1)) {
        d = 0.0;
    } else if (sign(del1) != sign(del2) && std::abs(d) > std::abs(3.0 * del1)) {
        d = 3.0 * del1;
    }
    return d;
}

}  // namespace

// ---------------------------------------------------------------------------
// PCHIP
// ---------------------------------------------------------------------------
PchipInterpolant::PchipInterpolant(std::vector<double> x, std::vector<double> y)
    : x_(std::move(x)), y_(std::move(y)) {
    validate_grid(x_, y_);
    const std::size_t n = x_.size();

    std::vector<double> h(n - 1), del(n - 1);
    for (std::size_t k = 0; k + 1 < n; ++k) {
        h[k] = x_[k + 1] - x_[k];
        del[k] = (y_[k + 1] - y_[k]) / h[k];
    }

    // Derivadas nos nós: média harmônica ponderada clássica de Fritsch–Carlson.
    // Esta forma (e não a reescrita com dmin/dmax do pchip.m) é a que reproduz
    // o griddedInterpolant bit a bit — verificado contra MATLAB/Reference.
    std::vector<double> d(n, 0.0);
    if (n == 2) {
        d[0] = d[1] = del[0];
    } else {
        for (std::size_t k = 0; k + 2 < n; ++k) {
            if (sign(del[k]) * sign(del[k + 1]) > 0.0) {
                const double w1 = 2.0 * h[k + 1] + h[k];
                const double w2 = h[k + 1] + 2.0 * h[k];
                d[k + 1] = (w1 + w2) / (w1 / del[k] + w2 / del[k + 1]);
            }
        }
        d[0] = pchip_end(h[0], h[1], del[0], del[1]);
        d[n - 1] = pchip_end(h[n - 2], h[n - 3], del[n - 2], del[n - 3]);
    }

    // Coeficientes da forma de potências (pwch do MATLAB)
    c3_.resize(n - 1);
    c2_.resize(n - 1);
    c1_.resize(n - 1);
    for (std::size_t k = 0; k + 1 < n; ++k) {
        const double dzzdx = (del[k] - d[k]) / h[k];
        const double dzdxdx = (d[k + 1] - del[k]) / h[k];
        c3_[k] = (dzdxdx - dzzdx) / h[k];
        c2_[k] = 2.0 * dzzdx - dzdxdx;
        c1_[k] = d[k];
    }
}

double PchipInterpolant::operator()(double xq) const noexcept {
    const std::size_t k = find_interval(x_, xq);
    const double t = xq - x_[k];
    return ((c3_[k] * t + c2_[k]) * t + c1_[k]) * t + y_[k];
}

// ---------------------------------------------------------------------------
// Linear
// ---------------------------------------------------------------------------
LinearInterpolant::LinearInterpolant(std::vector<double> x, std::vector<double> y)
    : x_(std::move(x)), y_(std::move(y)) {
    validate_grid(x_, y_);
}

double LinearInterpolant::operator()(double xq) const noexcept {
    const std::size_t k = find_interval(x_, xq);
    const double y0 = y_[k];
    const double y1 = y_[k + 1];
    // Trecho plano: devolve o valor exato (o MATLAB faz o mesmo, inclusive na extrapolação).
    if (y0 == y1) return y0;
    // Ordem de operações que reproduz o MATLAB bit a bit (verificado contra MATLAB/Reference).
    const double t = (xq - x_[k]) / (x_[k + 1] - x_[k]);
    return (1.0 - t) * y0 + t * y1;
}

// ---------------------------------------------------------------------------
// linspace
// ---------------------------------------------------------------------------
std::vector<double> linspace(double a, double b, std::size_t n) {
    if (n == 0) return {};
    if (n == 1) return {b};
    std::vector<double> y(n);
    const double n1 = static_cast<double>(n - 1);
    const double span = b - a;
    for (std::size_t k = 0; k < n; ++k) {
        y[k] = a + (static_cast<double>(k) * span) / n1;
    }
    y.front() = a;
    y.back() = b;
    return y;
}

}  // namespace fsae::math
