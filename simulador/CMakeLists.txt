# Biblioteca do simulador (fsae::core): física, câmbio, pistas, simulação e leitura/gravação
# de arquivos. Cada .hpp fica ao lado do seu .cpp; os includes partem desta pasta,
# por exemplo #include "fisica/longitudinal.hpp".

add_library(fsae_core
    version.cpp
    arquivos/csv.cpp
    arquivos/car_json.cpp
    arquivos/results.cpp
    arquivos/table_import.cpp
    matematica/interp.cpp
    carro/engine.cpp
    carro/car.cpp
    fisica/load_transfer.cpp
    fisica/brake_thermal.cpp
    fisica/longitudinal.cpp
    cambio/gearbox.cpp
    simulacao/straight.cpp
    simulacao/lap.cpp
    pista/track.cpp
    pista/corner_speed.cpp
    pista/geometry.cpp
    pista/gpx.cpp
)
add_library(fsae::core ALIAS fsae_core)
target_include_directories(fsae_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>)
target_link_libraries(fsae_core PRIVATE fsae_compile_options nlohmann_json::nlohmann_json fsae_miniz)

configure_file(version_config.hpp.in
               ${CMAKE_CURRENT_BINARY_DIR}/generated/fsae/version_config.hpp @ONLY)
