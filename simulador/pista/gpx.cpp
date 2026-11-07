#include "pista/gpx.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "matematica/interp.hpp"

namespace fsae::track {

namespace {

constexpr double kPi = 3.141592653589793;

[[noreturn]] void fail(const std::string& name, std::size_t point, const std::string& msg) {
    throw std::runtime_error("GPX " + name + ", ponto " + std::to_string(point) + ": " + msg);
}

double parse_double(const std::string& s, const std::string& name, std::size_t point,
                    const char* what) {
    errno = 0;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    while (end && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) ++end;
    if (s.empty() || end == s.c_str() || *end != '\0' || errno == ERANGE || !std::isfinite(v)) {
        fail(name, point, std::string(what) + " invalido: \"" + s + "\"");
    }
    return v;
}

/// Valor do atributo `attr="..."` (ou com aspas simples) dentro de `tag`.
bool attribute(const std::string& tag, const std::string& attr, std::string& out) {
    std::size_t pos = 0;
    while ((pos = tag.find(attr, pos)) != std::string::npos) {
        const bool boundary = pos > 0 && (tag[pos - 1] == ' ' || tag[pos - 1] == '\t' ||
                                          tag[pos - 1] == '\n' || tag[pos - 1] == '\r');
        std::size_t p = pos + attr.size();
        while (p < tag.size() && tag[p] == ' ') ++p;
        if (boundary && p < tag.size() && tag[p] == '=') {
            ++p;
            while (p < tag.size() && tag[p] == ' ') ++p;
            if (p < tag.size() && (tag[p] == '"' || tag[p] == '\'')) {
                const char q = tag[p];
                const auto end = tag.find(q, p + 1);
                if (end == std::string::npos) return false;
                out = tag.substr(p + 1, end - p - 1);
                return true;
            }
        }
        pos += attr.size();
    }
    return false;
}

}  // namespace

std::vector<GpxPoint> parse_gpx(const std::string& xml, const std::string& name) {
    std::vector<GpxPoint> pts;
    std::size_t pos = 0;
    while ((pos = xml.find("<trkpt", pos)) != std::string::npos) {
        const std::size_t n = pts.size() + 1;
        const auto tag_end = xml.find('>', pos);
        if (tag_end == std::string::npos) fail(name, n, "tag <trkpt> sem fechamento");
        const std::string tag = xml.substr(pos, tag_end - pos);
        GpxPoint p;
        std::string v;
        if (!attribute(tag, "lat", v)) fail(name, n, "atributo lat ausente");
        p.lat = parse_double(v, name, n, "lat");
        if (!attribute(tag, "lon", v)) fail(name, n, "atributo lon ausente");
        p.lon = parse_double(v, name, n, "lon");

        const bool self_closing = tag_end > 0 && xml[tag_end - 1] == '/';
        const auto close = self_closing ? tag_end : xml.find("</trkpt>", tag_end);
        if (close == std::string::npos) fail(name, n, "</trkpt> ausente");
        const auto ele = xml.find("<ele>", tag_end);
        if (self_closing || ele == std::string::npos || ele > close) {
            fail(name, n, "ponto sem altitude (<ele>)");
        }
        const auto ele_end = xml.find("</ele>", ele);
        if (ele_end == std::string::npos || ele_end > close) fail(name, n, "</ele> ausente");
        p.ele = parse_double(xml.substr(ele + 5, ele_end - ele - 5), name, n, "ele");
        pts.push_back(p);
        pos = close;
    }
    if (pts.size() < 2) throw std::runtime_error("GPX " + name + ": menos de 2 pontos <trkpt>");
    return pts;
}

std::vector<GpxPoint> load_gpx(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Nao foi possivel abrir " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return parse_gpx(ss.str(), path.filename().string());
}

GpxSlopeResult apply_gpx_altitude(const std::vector<Segment>& segments,
                                  const std::vector<GpxPoint>& points, int smoothing_window) {
    if (segments.empty()) throw std::invalid_argument("pista sem segmentos");
    if (points.size() < 2) throw std::invalid_argument("GPX com menos de 2 pontos");
    if (smoothing_window < 1) throw std::invalid_argument("janela de suavizacao deve ser >= 1");
    const std::size_t N = points.size();

    // 1. movmean(ele, k): janela centrada, encolhe nas pontas
    const auto k = static_cast<std::size_t>(smoothing_window);
    const std::size_t before = k / 2;
    const std::size_t after = (k % 2 == 1) ? k / 2 : k / 2 - 1;
    std::vector<double> ele_smooth(N);
    for (std::size_t i = 0; i < N; ++i) {
        const std::size_t a = i >= before ? i - before : 0;
        const std::size_t b = std::min(N - 1, i + after);
        double sum = 0;
        for (std::size_t j = a; j <= b; ++j) sum += points[j].ele;
        ele_smooth[i] = sum / static_cast<double>(b - a + 1);
    }

    // 2. Projeção local e distância acumulada
    const double lat0 = points[0].lat;
    const double lon0 = points[0].lon;
    constexpr double R = 6371000;
    const double cos_lat0 = std::cos(lat0 * kPi / 180);
    std::vector<double> dist(N, 0.0);
    double px = 0, py = 0;
    for (std::size_t i = 0; i < N; ++i) {
        const double x = R * ((kPi / 180) * (points[i].lon - lon0)) * cos_lat0;
        const double y = R * ((kPi / 180) * (points[i].lat - lat0));
        if (i > 0) {
            const double dx = x - px;
            const double dy = y - py;
            dist[i] = dist[i - 1] + std::sqrt(dx * dx + dy * dy);
        }
        px = x;
        py = y;
    }

    // unique(dist): distâncias crescentes, mantendo a primeira ocorrência
    std::vector<double> ud, ue;
    for (std::size_t i = 0; i < N; ++i) {
        if (!ud.empty() && dist[i] == ud.back()) continue;
        ud.push_back(dist[i]);
        ue.push_back(ele_smooth[i]);
    }
    if (ud.size() < 2) throw std::invalid_argument("GPX sem deslocamento (todos os pontos iguais)");

    // 3. Altitude nas junções
    const math::LinearInterpolant alt(ud, ue);
    GpxSlopeResult r;
    r.segments = segments;
    double total_length = 0;
    for (const auto& s : segments) total_length += s.length;
    std::vector<double> junction_alt;
    junction_alt.reserve(segments.size() + 1);
    double d = 0;
    junction_alt.push_back(alt(0.0));
    for (const auto& s : segments) {
        d += s.length;
        junction_alt.push_back(alt(d));
    }

    // 4–5. Correção do fechamento e inclinação
    r.z_error_total = junction_alt.back() - junction_alt.front();
    for (std::size_t i = 0; i < segments.size(); ++i) {
        const double L = segments[i].length;
        const double dz = junction_alt[i + 1] - junction_alt[i];
        const double correction = -(r.z_error_total * (L / total_length));
        const double dz_corr = dz + correction;
        r.alt_start.push_back(junction_alt[i]);
        r.alt_end.push_back(junction_alt[i + 1]);
        r.delta_z.push_back(dz_corr);
        r.segments[i].slope = std::atan(dz_corr / L);
    }
    return r;
}

}  // namespace fsae::track
