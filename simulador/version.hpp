#pragma once

#include <string_view>

namespace fsae {

/// Versão da biblioteca no formato "MAJOR.MINOR.PATCH".
std::string_view version() noexcept;

}  // namespace fsae
