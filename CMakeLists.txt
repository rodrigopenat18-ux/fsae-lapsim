cmake_minimum_required(VERSION 3.20)

project(fsae_lapsim
    VERSION 0.1.0
    DESCRIPTION "Formula SAE lap time simulator (C++ port of the MATLAB simulator)"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Opções
# ---------------------------------------------------------------------------
option(FSAE_BUILD_TESTS "Compila os testes unitários e de validação" ON)
option(FSAE_BUILD_APPS  "Compila o executável de linha de comando"  ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipo de build" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Flags comuns
#   Ponto flutuante: NÃO usar -ffast-math e desligar contração em FMA para
#   ficar o mais próximo possível da aritmética do MATLAB (validação).
# ---------------------------------------------------------------------------
add_library(fsae_compile_options INTERFACE)
if(MSVC)
    target_compile_options(fsae_compile_options INTERFACE /W4 /permissive- /fp:precise /utf-8)
else()
    target_compile_options(fsae_compile_options INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion
        -ffp-contract=off)
endif()

# MinGW (MSYS2): embute o runtime do GCC no .exe para rodar em qualquer Windows
# sem precisar das DLLs do MSYS2 no PATH.
if(MINGW)
    add_link_options(-static)
endif()

# ---------------------------------------------------------------------------
# Biblioteca principal
# ---------------------------------------------------------------------------
add_library(fsae_core
    src/version.cpp
    src/io/csv.cpp
    src/math/interp.cpp
)
add_library(fsae::core ALIAS fsae_core)
target_include_directories(fsae_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>)
target_link_libraries(fsae_core PRIVATE fsae_compile_options)

configure_file(cmake/version_config.hpp.in
               ${CMAKE_CURRENT_BINARY_DIR}/generated/fsae/version_config.hpp @ONLY)

# ---------------------------------------------------------------------------
# Executável
# ---------------------------------------------------------------------------
if(FSAE_BUILD_APPS)
    add_subdirectory(apps/fsae_sim)
endif()

# ---------------------------------------------------------------------------
# Testes
# ---------------------------------------------------------------------------
if(FSAE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
