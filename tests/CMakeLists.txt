include(FetchContent)

# O CMake 4 removeu a compatibilidade com projetos que declaram versão mínima < 3.5.
# Esta variável só vale para as dependências baixadas abaixo (ignorada no CMake 3.x).
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.12
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(doctest)

add_executable(fsae_tests
    test_main.cpp
    test_version.cpp
    test_math.cpp
)
target_include_directories(fsae_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fsae_tests PRIVATE fsae::core doctest::doctest fsae_compile_options)

# Pasta com os dados de referência gerados pelo MATLAB (Fase 1)
target_compile_definitions(fsae_tests PRIVATE
    FSAE_REFERENCE_DIR="${PROJECT_SOURCE_DIR}/tests/reference")

add_test(NAME fsae_tests COMMAND fsae_tests)
