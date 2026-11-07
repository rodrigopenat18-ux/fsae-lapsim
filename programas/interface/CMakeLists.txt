# Interface gráfica: Dear ImGui + ImPlot sobre GLFW/OpenGL 3.
# As dependências são baixadas pelo CMake na primeira configuração.

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.4
    GIT_SHALLOW    TRUE)
FetchContent_Declare(implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        v0.16
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(glfw)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()
FetchContent_GetProperties(implot)
if(NOT implot_POPULATED)
    FetchContent_Populate(implot)
endif()

find_package(OpenGL REQUIRED)

add_library(fsae_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp)
target_include_directories(fsae_imgui SYSTEM PUBLIC
    ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${implot_SOURCE_DIR})
target_link_libraries(fsae_imgui PUBLIC glfw OpenGL::GL)

add_executable(fsae_gui WIN32
    main.cpp
    app.cpp
    plots.cpp
    car_editor.cpp
    import_dialog.cpp
    theme.cpp
    platform.cpp)
target_link_libraries(fsae_gui PRIVATE fsae::core fsae_imgui fsae_compile_options)
if(NOT MSVC)
    # ImGui/ImPlot trabalham em float; as conversões double -> float da interface são intencionais.
    target_compile_options(fsae_gui PRIVATE -Wno-conversion -Wno-float-conversion)
endif()
if(WIN32)
    target_link_libraries(fsae_gui PRIVATE comdlg32 shell32)
endif()

if(FSAE_COPY_TO_ROOT)
    add_custom_command(TARGET fsae_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DSRC=$<TARGET_FILE:fsae_gui> -DDST_DIR=${PROJECT_SOURCE_DIR}
                -P ${PROJECT_SOURCE_DIR}/programas/copiar_para_raiz.cmake
        VERBATIM)
endif()
