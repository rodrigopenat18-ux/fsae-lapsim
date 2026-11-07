add_executable(fsae_sim main.cpp)
target_link_libraries(fsae_sim PRIVATE fsae::core fsae_compile_options)

if(FSAE_COPY_TO_ROOT)
    add_custom_command(TARGET fsae_sim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DSRC=$<TARGET_FILE:fsae_sim> -DDST_DIR=${PROJECT_SOURCE_DIR}
                -P ${PROJECT_SOURCE_DIR}/Root/copy_to_root.cmake
        VERBATIM)
endif()
