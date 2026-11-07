#include <doctest/doctest.h>

#include "fsae/version.hpp"

TEST_CASE("version string is not empty") {
    CHECK_FALSE(fsae::version().empty());
}
