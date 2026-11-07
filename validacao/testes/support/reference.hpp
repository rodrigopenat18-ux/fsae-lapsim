#pragma once

// Utilitários de teste para comparar saídas do C++ com o gabarito do MATLAB.

#include <doctest/doctest.h>

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "arquivos/csv.hpp"

namespace fsae::test {

inline std::filesystem::path reference_path(const std::string& relative) {
    return std::filesystem::path(FSAE_REFERENCE_DIR) / relative;
}

inline io::CsvTable load_reference(const std::string& relative) {
    return io::read_numeric_csv(reference_path(relative));
}

/// Estatísticas de diferença entre o esperado (MATLAB) e o obtido (C++).
struct ErrorStats {
    std::size_t count = 0;
    std::size_t exact = 0;        ///< valores idênticos bit a bit
    std::size_t failures = 0;     ///< fora da tolerância
    double max_abs = 0.0;
    double max_rel = 0.0;
    std::size_t worst_index = 0;  ///< índice com maior erro relativo

    [[nodiscard]] std::string summary(const std::string& label) const {
        std::ostringstream os;
        os.precision(3);
        os << label << ": " << count << " valores, " << exact << " identicos, max |erro| = "
           << max_abs << ", max erro rel = " << max_rel << " (indice " << worst_index << ")";
        return os.str();
    }
};

/// |obtido − esperado| <= atol + rtol·|esperado|; NaN==NaN e ±Inf iguais contam como iguais.
inline bool close_enough(double expected, double actual, double rtol, double atol) {
    if (std::isnan(expected) || std::isnan(actual)) return std::isnan(expected) && std::isnan(actual);
    if (std::isinf(expected) || std::isinf(actual)) return expected == actual;
    return std::abs(actual - expected) <= atol + rtol * std::abs(expected);
}

inline ErrorStats compare(const std::vector<double>& expected, const std::vector<double>& actual,
                          double rtol, double atol = 0.0) {
    ErrorStats s;
    REQUIRE(expected.size() == actual.size());
    s.count = expected.size();
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const double e = expected[i];
        const double a = actual[i];
        if (e == a || (std::isnan(e) && std::isnan(a))) {
            ++s.exact;
            continue;
        }
        if (!close_enough(e, a, rtol, atol)) ++s.failures;
        if (std::isfinite(e) && std::isfinite(a)) {
            const double abs_err = std::abs(a - e);
            const double rel_err = abs_err / std::max(std::abs(e), 1e-300);
            if (abs_err > s.max_abs) s.max_abs = abs_err;
            if (rel_err > s.max_rel) {
                s.max_rel = rel_err;
                s.worst_index = i;
            }
        }
    }
    return s;
}

/// Compara e registra o resultado no doctest (mensagem sempre visível com `-s`).
inline void check_against_reference(const std::string& label, const std::vector<double>& expected,
                                    const std::vector<double>& actual, double rtol,
                                    double atol = 0.0) {
    const ErrorStats s = compare(expected, actual, rtol, atol);
    MESSAGE(s.summary(label));
    CHECK_MESSAGE(s.failures == 0, s.summary(label));
}

}  // namespace fsae::test
