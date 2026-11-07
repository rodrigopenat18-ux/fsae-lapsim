#include "version.hpp"

#include "fsae/version_config.hpp"

namespace fsae {

std::string_view version() noexcept { return FSAE_VERSION_STRING; }

}  // namespace fsae
