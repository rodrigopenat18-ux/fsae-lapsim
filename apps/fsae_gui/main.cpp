// fsae_gui — interface gráfica do simulador de volta FSAE (Dear ImGui + ImPlot).
//
// Opções de linha de comando (para desenvolvimento/documentação):
//   --screenshot <arquivo.bmp>  roda uma simulação, grava a tela e fecha
//   --tab <nome>                aba aberta (ex.: "Mapa da pista")
//   --track <nome>              pista inicial (ex.: autox)
//   --size <largura> <altura>   tamanho da janela

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

#include "app.hpp"

namespace {

void glfw_error(int code, const char* msg) { std::fprintf(stderr, "GLFW %d: %s\n", code, msg); }

/// Grava o framebuffer atual como BMP de 24 bits.
bool write_bmp(const std::string& path, int w, int h) {
    std::vector<unsigned char> px(static_cast<std::size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    const int row = (w * 3 + 3) & ~3;
    const std::uint32_t data_size = static_cast<std::uint32_t>(row * h);
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    auto u16 = [&](std::uint16_t v) { f.put(static_cast<char>(v & 0xff)); f.put(static_cast<char>(v >> 8)); };
    auto u32 = [&](std::uint32_t v) { for (int i = 0; i < 4; ++i) f.put(static_cast<char>((v >> (8 * i)) & 0xff)); };
    f.put('B'); f.put('M');
    u32(54 + data_size); u32(0); u32(54);
    u32(40); u32(static_cast<std::uint32_t>(w)); u32(static_cast<std::uint32_t>(h)); u16(1); u16(24); u32(0);
    u32(data_size); u32(2835); u32(2835); u32(0); u32(0);
    std::vector<char> line(static_cast<std::size_t>(row), 0);
    for (int y = 0; y < h; ++y) {  // OpenGL já entrega de baixo para cima, como o BMP
        for (int x = 0; x < w; ++x) {
            const unsigned char* p = &px[(static_cast<std::size_t>(y) * w + x) * 3];
            line[static_cast<std::size_t>(x * 3 + 0)] = static_cast<char>(p[2]);
            line[static_cast<std::size_t>(x * 3 + 1)] = static_cast<char>(p[1]);
            line[static_cast<std::size_t>(x * 3 + 2)] = static_cast<char>(p[0]);
        }
        f.write(line.data(), row);
    }
    return static_cast<bool>(f);
}

void setup_style() {
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0;
    s.FrameRounding = 4;
    s.GrabRounding = 4;
    s.TabRounding = 4;
    s.ChildRounding = 4;
    s.FramePadding = ImVec2(8, 5);
    s.ItemSpacing = ImVec2(8, 6);
    s.Colors[ImGuiCol_WindowBg] = ImVec4(0.102f, 0.102f, 0.098f, 1.0f);  // #1a1a19
    s.Colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.115f, 1.0f);
    ImPlot::StyleColorsDark();
    ImPlotStyle& p = ImPlot::GetStyle();
    p.LineWeight = 2.0f;
    p.Colors[ImPlotCol_PlotBg] = ImVec4(0.102f, 0.102f, 0.098f, 1.0f);
    p.Colors[ImPlotCol_FrameBg] = ImVec4(0, 0, 0, 0);
    p.Colors[ImPlotCol_AxisGrid] = ImVec4(1, 1, 1, 0.08f);
}

void load_font(float size) {
    ImGuiIO& io = ImGui::GetIO();
    const char* candidates[] = {"C:/Windows/Fonts/segoeui.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                                "/System/Library/Fonts/SFNS.ttf"};
    for (const char* f : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(f, ec)) {
            // Faixa padrão: Latin-1 (inclui acentos do português)
            if (io.Fonts->AddFontFromFileTTF(f, size, nullptr, io.Fonts->GetGlyphRangesDefault())) return;
        }
    }
    io.Fonts->AddFontDefault();
}

}  // namespace

int main(int argc, char** argv) {
    std::string screenshot, tab, track = "autox";
    bool demo_compare = false;
    int width = 1600, height = 950;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--screenshot" && i + 1 < argc) screenshot = argv[++i];
        else if (a == "--tab" && i + 1 < argc) tab = argv[++i];
        else if (a == "--track" && i + 1 < argc) track = argv[++i];
        else if (a == "--demo-compare") demo_compare = true;
        else if (a == "--size" && i + 2 < argc) {
            width = std::atoi(argv[++i]);
            height = std::atoi(argv[++i]);
        }
    }

    glfwSetErrorCallback(glfw_error);
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    if (!screenshot.empty()) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(width, height, "FSAE Lap Simulator", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // não grava imgui.ini na pasta atual
    float xscale = 1, yscale = 1;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    load_font(17.0f * xscale);
    setup_style();
    ImGui::GetStyle().ScaleAllSizes(xscale);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    fsae::gui::App app;
    if (!screenshot.empty()) {
        app.load_track_by_name(track);
        app.run_now();
        if (demo_compare) app.demo_second_run();
    }

    int frames = 0;
    while (!glfwWindowShouldClose(window)) {
        if (screenshot.empty()) glfwWaitEventsTimeout(0.5);  // não gasta CPU parado
        else glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if (!tab.empty() && frames < 2) app.select_tab(tab);
        app.frame();
        ImGui::Render();
        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.102f, 0.102f, 0.098f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (!screenshot.empty() && ++frames == 6) {
            const bool ok = write_bmp(screenshot, w, h);
            std::printf("%s %s\n", ok ? "Captura gravada:" : "Falha ao gravar", screenshot.c_str());
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
