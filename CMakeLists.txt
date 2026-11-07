cmake_minimum_required(VERSION 3.20)

project(fsae_lapsim
    VERSION 0.1.0
    DESCRIPTION "Formula SAE lap time simulator (C++ port of the MATLAB simulator)"
    LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Opções
# ---------------------------------------------------------------------------
option(FSAE_BUILD_TESTS "Compila os testes unitários e de validação" ON)
option(FSAE_BUILD_APPS  "Compila o executável de linha de comando"  ON)
option(FSAE_BUILD_GUI   "Compila a interface gráfica (fsae_gui)"    ON)

# Depois de compilar em Release, copia fsae_gui e fsae_sim para a raiz do repositório
# (ao lado de Data/), para abrir com duplo clique. As cópias ficam fora do Git.
if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Release")
    set(_fsae_copy_default ON)
else()
    set(_fsae_copy_default OFF)
endif()
option(FSAE_COPY_TO_ROOT "Copia os executáveis para a raiz após compilar (só Release)" ${_fsae_copy_default})

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
include(FetchContent)
# O CMake 4 removeu a compatibilidade com projetos que declaram versão mínima < 3.5.
# Esta variável só vale para as dependências baixadas (ignorada no CMake 3.x).
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(nlohmann_json)

# miniz: leitura de arquivos .xlsx (zip). Só os fontes da versão oficial.
FetchContent_Declare(miniz
    URL https://github.com/richgel999/miniz/releases/download/3.0.2/miniz-3.0.2.zip
    URL_HASH SHA256=ada38db0b703a56d3dd6d57bf84a9c5d664921d870d8fea4db153979fb5332c5
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_GetProperties(miniz)
if(NOT miniz_POPULATED)
    FetchContent_Populate(miniz)
endif()
add_library(fsae_miniz STATIC ${miniz_SOURCE_DIR}/miniz.c)
target_include_directories(fsae_miniz SYSTEM PUBLIC ${miniz_SOURCE_DIR})
set_target_properties(fsae_miniz PROPERTIES POSITION_INDEPENDENT_CODE ON)

# A física do simulador (biblioteca fsae::core)
add_subdirectory(Modules)

# ---------------------------------------------------------------------------
# Programas
# ---------------------------------------------------------------------------
if(FSAE_BUILD_APPS)
    add_subdirectory(Root/CLI)
endif()
if(FSAE_BUILD_GUI)
    add_subdirectory(Root/GUI)
endif()

# ---------------------------------------------------------------------------
# Testes
# ---------------------------------------------------------------------------
if(FSAE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
