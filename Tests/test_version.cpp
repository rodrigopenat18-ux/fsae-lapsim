#include <doctest/doctest.h>

#include "version.hpp"

TEST_CASE("version string is not empty") {
    CHECK_FALSE(fsae::version().empty());
}
