# Biblioteca do simulador (fsae::core): física, câmbio, pistas, simulação e leitura/gravação
# de arquivos. Cada .hpp fica ao lado do seu .cpp; os includes partem desta pasta,
# por exemplo #include "Physics/longitudinal.hpp".

add_library(fsae_core
    version.cpp
    IO_Tools/csv.cpp
    IO_Tools/car_json.cpp
    IO_Tools/results.cpp
    IO_Tools/table_import.cpp
    Math/interp.cpp
    Vehicle/engine.cpp
    Vehicle/car.cpp
    Physics/load_transfer.cpp
    Physics/brake_thermal.cpp
    Physics/longitudinal.cpp
    Control/gearbox.cpp
    Simulation/straight.cpp
    Simulation/lap.cpp
    Simulation/quasi_static.cpp
    Tracks/track.cpp
    Tracks/corner_speed.cpp
    Tracks/geometry.cpp
    Tracks/gpx.cpp
)
add_library(fsae::core ALIAS fsae_core)
target_include_directories(fsae_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>)
target_link_libraries(fsae_core PRIVATE fsae_compile_options nlohmann_json::nlohmann_json fsae_miniz)

configure_file(version_config.hpp.in
               ${CMAKE_CURRENT_BINARY_DIR}/generated/fsae/version_config.hpp @ONLY)
