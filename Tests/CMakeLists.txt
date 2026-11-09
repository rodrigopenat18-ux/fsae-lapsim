FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.12
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(doctest)

add_executable(fsae_tests
    test_main.cpp
    test_version.cpp
    test_math.cpp
    test_car.cpp
    test_physics.cpp
    test_gearbox.cpp
    test_longitudinal.cpp
    test_straight.cpp
    test_track.cpp
    test_lap.cpp
    test_results.cpp
    test_gpx.cpp
    test_table_import.cpp
    test_model_fixes.cpp
)
target_include_directories(fsae_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fsae_tests PRIVATE fsae::core doctest::doctest nlohmann_json::nlohmann_json
                                          fsae_compile_options)

# Pasta com os dados de referência gerados pelo MATLAB (Fase 1)
target_compile_definitions(fsae_tests PRIVATE
    FSAE_REFERENCE_DIR="${PROJECT_SOURCE_DIR}/MATLAB/Reference"
    FSAE_DATA_DIR="${PROJECT_SOURCE_DIR}/Data")

add_test(NAME fsae_tests COMMAND fsae_tests)

# Testes de fumaça do executável
if(FSAE_BUILD_APPS)
    add_test(NAME cli_help COMMAND fsae_sim help)
    add_test(NAME cli_run_skidpad
             COMMAND fsae_sim run --track ${PROJECT_SOURCE_DIR}/Data/Tracks/skidpad.csv --no-files)
    add_test(NAME cli_run_autox_json
             COMMAND fsae_sim run --track autox --json --no-files)
    set_tests_properties(cli_run_autox_json PROPERTIES
        ENVIRONMENT "FSAE_DATA_DIR=${PROJECT_SOURCE_DIR}/Data"
        PASS_REGULAR_EXPRESSION "\"Lap2_Time\": 45[.]5006")
    add_test(NAME cli_bad_option COMMAND fsae_sim run --track autox --bogus)
    set_tests_properties(cli_bad_option PROPERTIES WILL_FAIL TRUE)
endif()
