add_executable(fsae_sim main.cpp)
target_link_libraries(fsae_sim PRIVATE fsae::core fsae_compile_options)
