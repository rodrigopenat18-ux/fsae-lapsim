#include "platform.hpp"

#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;

namespace fsae::gui {

std::string to_utf8(const fs::path& p) {
#if defined(__cpp_char8_t)
    const auto s = p.u8string();
    return std::string(s.begin(), s.end());
#else
    return p.u8string();
#endif
}

#ifdef _WIN32
namespace {

std::wstring widen(const char* s) {
    const int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n > 0 ? n - 1 : 0), L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s, -1, w.data(), n);
    return w;
}

/// Filtro no formato do Windows: "desc\0ext\0Todos\0*.*\0\0".
std::wstring make_filter(const char* desc, const char* ext) {
    std::wstring f = widen(desc) + L" (" + widen(ext) + L")";
    f.push_back(L'\0');
    f += widen(ext);
    f.push_back(L'\0');
    f += L"Todos os arquivos (*.*)";
    f.push_back(L'\0');
    f += L"*.*";
    f.push_back(L'\0');
    f.push_back(L'\0');
    return f;
}

}  // namespace

std::optional<fs::path> open_file_dialog(const char* title, const char* filter_desc,
                                         const char* filter_ext) {
    wchar_t buf[MAX_PATH * 4] = L"";
    const std::wstring filter = make_filter(filter_desc, filter_ext);
    const std::wstring wtitle = widen(title);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = buf;
    ofn.nMaxFile = sizeof buf / sizeof buf[0];
    ofn.lpstrTitle = wtitle.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return std::nullopt;
    return fs::path(buf);
}

std::optional<fs::path> save_file_dialog(const char* title, const char* filter_desc,
                                         const char* filter_ext, const char* default_name) {
    wchar_t buf[MAX_PATH * 4] = L"";
    const std::wstring def = widen(default_name);
    wcsncpy(buf, def.c_str(), sizeof buf / sizeof buf[0] - 1);
    const std::wstring filter = make_filter(filter_desc, filter_ext);
    const std::wstring wtitle = widen(title);
    std::wstring def_ext = widen(filter_ext);
    if (def_ext.rfind(L"*.", 0) == 0) def_ext = def_ext.substr(2);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = buf;
    ofn.nMaxFile = sizeof buf / sizeof buf[0];
    ofn.lpstrTitle = wtitle.c_str();
    ofn.lpstrDefExt = def_ext.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&ofn)) return std::nullopt;
    return fs::path(buf);
}

void open_folder(const fs::path& dir) {
    ShellExecuteW(nullptr, L"open", dir.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

fs::path executable_dir() {
    wchar_t buf[MAX_PATH * 4];
    const DWORD n = GetModuleFileNameW(nullptr, buf, sizeof buf / sizeof buf[0]);
    if (n == 0) return fs::current_path();
    return fs::path(std::wstring(buf, n)).parent_path();
}

#else

std::optional<fs::path> open_file_dialog(const char*, const char*, const char*) { return std::nullopt; }
std::optional<fs::path> save_file_dialog(const char*, const char*, const char*, const char*) {
    return std::nullopt;
}
void open_folder(const fs::path&) {}
fs::path executable_dir() {
    std::error_code ec;
    const auto p = fs::read_symlink("/proc/self/exe", ec);
    return ec ? fs::current_path() : p.parent_path();
}

#endif

fs::path settings_path() {
#ifdef _WIN32
    if (const wchar_t* appdata = _wgetenv(L"APPDATA")) return fs::path(appdata) / "fsae_lapsim" / "gui.ini";
#else
    if (const char* home = std::getenv("HOME")) return fs::path(home) / ".config" / "fsae_lapsim" / "gui.ini";
#endif
    return executable_dir() / "fsae_gui.ini";
}

std::vector<fs::path> data_dirs() {
    std::vector<fs::path> dirs;
    if (const char* env = std::getenv("FSAE_DATA_DIR")) dirs.emplace_back(env);
    dirs.emplace_back(fs::current_path() / "Data");
    fs::path dir = executable_dir();
    for (int up = 0; up < 6 && !dir.empty(); ++up) {
        dirs.push_back(dir / "Data");
        if (dir == dir.parent_path()) break;
        dir = dir.parent_path();
    }
    return dirs;
}

fs::path find_data_subdir(const std::string& sub) {
    for (const auto& d : data_dirs()) {
        std::error_code ec;
        if (fs::is_directory(d / sub, ec)) return d / sub;
    }
    return {};
}

}  // namespace fsae::gui
