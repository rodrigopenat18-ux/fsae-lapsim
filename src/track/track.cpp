#include "fsae/track/track.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fsae::track {

namespace {

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::vector<std::string> split(const std::string& line, char delim) {
    std::vector<std::string> out;
    std::string field;
    std::istringstream ss(line);
    while (std::getline(ss, field, delim)) out.push_back(trim(field));
    if (!line.empty() && line.back() == delim) out.emplace_back();
    return out;
}

[[noreturn]] void fail(const std::string& name, std::size_t line, const std::string& msg) {
    throw std::runtime_error("Pista " + name + ", linha " + std::to_string(line) + ": " + msg);
}

double to_number(const std::string& s, const std::string& what, const std::string& name,
                 std::size_t line) {
    if (s.empty()) fail(name, line, what + " vazio");
    errno = 0;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end != s.c_str() + s.size() || errno == ERANGE || !std::isfinite(v)) {
        fail(name, line, what + " invalido: \"" + s + "\"");
    }
    return v;
}

SegmentType to_type(const std::string& s, const std::string& name, std::size_t line) {
    const std::string t = lower(s);
    if (t == "straight" || t == "0") return SegmentType::Straight;
    if (t == "left" || t == "1") return SegmentType::Left;
    if (t == "right" || t == "2") return SegmentType::Right;
    fail(name, line, "tipo de segmento desconhecido: \"" + s + "\" (use straight, left ou right)");
}

void validate(const Segment& s, const std::string& name, std::size_t line) {
    if (!(s.length > 0)) fail(name, line, "comprimento deve ser positivo");
    if (s.radius < 0) fail(name, line, "raio nao pode ser negativo");
    if (s.type == SegmentType::Straight && s.radius != 0) {
        fail(name, line, "reta (straight) deve ter raio 0");
    }
    if (s.type != SegmentType::Straight && s.radius == 0) {
        fail(name, line, "curva (left/right) precisa de raio maior que 0");
    }
}

std::string shortest(double v) {
    char buf[64];
    const auto r = std::to_chars(buf, buf + sizeof buf, v);
    return std::string(buf, r.ptr);
}

}  // namespace

double Track::length() const noexcept {
    double total = 0;
    for (const auto& s : segments) total += s.length;
    return total;
}

Track parse_track(const std::string& text, const std::string& name) {
    Track track;
    track.name = name;

    std::istringstream in(text);
    std::string raw;
    std::size_t line_no = 0;
    enum class Format { Unknown, Native, Matlab, Semicolon } format = Format::Unknown;

    while (std::getline(in, raw)) {
        ++line_no;
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;

        if (format == Format::Unknown) {
            if (line.find(';') != std::string::npos) {
                format = Format::Semicolon;
            } else {
                const auto h = split(lower(line), ',');
                const std::vector<std::string> native = {"length_m", "radius_m", "slope", "type"};
                const std::vector<std::string> matlab = {"index", "length", "radius", "slope", "type"};
                if (h == native) {
                    format = Format::Native;
                } else if (h == matlab) {
                    format = Format::Matlab;
                } else {
                    fail(name, line_no,
                         "cabecalho esperado \"length_m,radius_m,slope,type\", "
                         "\"index,length,radius,slope,type\" (matriz do MATLAB) ou formato "
                         "\"Tipo;Comprimento;Raio\" do trackExcel");
                }
                continue;
            }
        }

        Segment s;
        if (format == Format::Native) {
            const auto f = split(line, ',');
            if (f.size() != 4) fail(name, line_no, "esperadas 4 colunas (length_m,radius_m,slope,type)");
            s.length = to_number(f[0], "comprimento", name, line_no);
            s.radius = to_number(f[1], "raio", name, line_no);
            s.slope = to_number(f[2], "inclinacao", name, line_no);
            s.type = to_type(f[3], name, line_no);
        } else if (format == Format::Matlab) {
            const auto f = split(line, ',');
            if (f.size() != 5) fail(name, line_no, "esperadas 5 colunas (index,length,radius,slope,type)");
            s.length = to_number(f[1], "comprimento", name, line_no);
            s.radius = to_number(f[2], "raio", name, line_no);
            s.slope = to_number(f[3], "inclinacao", name, line_no);
            s.type = to_type(f[4], name, line_no);
        } else {
            const auto f = split(line, ';');
            if (f.size() != 3) fail(name, line_no, "esperadas 3 colunas (Tipo;Comprimento;Raio)");
            s.type = to_type(f[0], name, line_no);
            s.length = to_number(f[1], "comprimento", name, line_no);
            s.radius = to_number(f[2], "raio", name, line_no);
            s.slope = 0;
        }
        validate(s, name, line_no);
        track.segments.push_back(s);
    }
    if (track.segments.empty()) {
        throw std::runtime_error("Pista " + name + ": nenhum segmento encontrado");
    }
    return track;
}

Track load_track(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Nao foi possivel abrir " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    Track t = parse_track(ss.str(), path.filename().string());
    t.name = path.stem().string();
    return t;
}

std::string track_to_csv(const Track& track, const std::string& header_comment) {
    std::ostringstream out;
    if (!header_comment.empty()) {
        std::istringstream lines(header_comment);
        std::string l;
        while (std::getline(lines, l)) out << "# " << l << "\n";
    }
    out << "length_m,radius_m,slope,type\n";
    for (const auto& s : track.segments) {
        out << shortest(s.length) << "," << shortest(s.radius) << "," << shortest(s.slope) << ","
            << to_string(s.type) << "\n";
    }
    return out.str();
}

void save_track(const Track& track, const std::filesystem::path& path,
                const std::string& header_comment) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Nao foi possivel gravar " + path.string());
    out << track_to_csv(track, header_comment);
}

std::vector<Segment> rotate_to_start(const std::vector<Segment>& segments, int start_segment) {
    const int n = static_cast<int>(segments.size());
    if (start_segment < 1 || start_segment > n) {
        throw std::invalid_argument("segmento de largada deve estar entre 1 e " + std::to_string(n));
    }
    std::vector<Segment> out(segments);
    std::rotate(out.begin(), out.begin() + (start_segment - 1), out.end());
    return out;
}

const char* to_string(SegmentType type) noexcept {
    switch (type) {
        case SegmentType::Left: return "left";
        case SegmentType::Right: return "right";
        case SegmentType::Straight: break;
    }
    return "straight";
}

}  // namespace fsae::track
