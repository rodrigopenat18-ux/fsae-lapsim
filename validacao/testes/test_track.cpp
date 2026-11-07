#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "pista/corner_speed.hpp"
#include "pista/geometry.hpp"
#include "pista/track.hpp"
#include "carro/car.hpp"
#include "support/reference.hpp"

using fsae::Car;
using fsae::test::check_against_reference;
using fsae::test::load_reference;
namespace tr = fsae::track;

namespace {

/// sin/cos/tan/atan podem divergir no último bit em outras libm (ver test_physics.cpp).
/// Na geometria o ângulo é acumulado ponto a ponto, então a folga é maior.
constexpr double kLibm = 1e-12;

const Car& car() {
    static const Car c = Car::matlab_default();
    return c;
}

struct BuiltinTrack {
    const char* reference;  ///< pasta em validacao/gabarito/tracks
    const char* file;       ///< arquivo em dados/pistas
};

const BuiltinTrack kTracks[] = {
    {"autox", "autox.csv"},
    {"skidpad", "skidpad.csv"},
    {"acceleration", "acceleration.csv"},
    {"autox2024v1", "autox_2024_v1.csv"},
    {"autox2024v1_rev", "autox_2024_v1_reversed.csv"},
};

tr::Track load_builtin(const BuiltinTrack& t) {
    return tr::load_track(std::string(FSAE_DATA_DIR) + "/pistas/" + t.file);
}

}  // namespace

TEST_CASE("velocidade maxima em curva (corner_speed.csv)") {
    const auto ref = load_reference("unit/corner_speed.csv");
    const auto& r = ref.column("radius");
    const auto& s = ref.column("slope");
    std::vector<double> v;
    for (std::size_t i = 0; i < ref.rows(); ++i) {
        v.push_back(tr::max_safe_corner_speed(car(), r[i], s[i]).v_max);
    }
    check_against_reference("v_max", ref.column("v_max"), v, 0.0);
}

TEST_CASE("pistas embutidas: segmentos, curvas e geometria iguais ao MATLAB") {
    for (const auto& bt : kTracks) {
        const std::string file = bt.file;
        CAPTURE(file);
        const std::string dir = std::string("tracks/") + bt.reference + "/";
        const auto track = load_builtin(bt);
        const auto& seg = track.segments;

        // Segmentos do arquivo == matriz `segments` do MATLAB
        const auto ref_seg = load_reference(dir + "segments.csv");
        REQUIRE(seg.size() == ref_seg.rows());
        std::vector<double> len, rad, slope, type;
        for (const auto& s : seg) {
            len.push_back(s.length);
            rad.push_back(s.radius);
            slope.push_back(s.slope);
            type.push_back(static_cast<double>(static_cast<int>(s.type)));
        }
        check_against_reference(dir + "length", ref_seg.column("length"), len, 0.0);
        check_against_reference(dir + "radius", ref_seg.column("radius"), rad, 0.0);
        check_against_reference(dir + "slope", ref_seg.column("slope"), slope, 0.0);
        check_against_reference(dir + "type", ref_seg.column("type"), type, 0.0);

        // initializeTrack
        const auto ref_c = load_reference(dir + "corners.csv");
        std::vector<double> vc;
        for (const auto& c : tr::corner_speeds(car(), seg)) vc.push_back(c.v_max);
        check_against_reference(dir + "corner_velocity", ref_c.column("corner_velocity"), vc, kLibm);

        // buildTrack
        const auto g = tr::build_geometry(seg);
        const auto ref_g = load_reference(dir + "geometry.csv");
        REQUIRE(g.size() == ref_g.rows());
        // Erro absoluto: coordenadas perto de zero tornam o erro relativo enganoso.
        check_against_reference(dir + "x", ref_g.column("x"), g.x, kLibm, 1e-9);
        check_against_reference(dir + "y", ref_g.column("y"), g.y, kLibm, 1e-9);
        check_against_reference(dir + "z", ref_g.column("z"), g.z, kLibm, 1e-9);

        const auto ref_v = load_reference(dir + "track_vectors.csv");
        REQUIRE(g.u.size() == ref_v.rows());
        check_against_reference(dir + "u", ref_v.column("u"), g.u, kLibm, 1e-12);
        check_against_reference(dir + "v", ref_v.column("v"), g.v, kLibm, 1e-12);
    }
}

TEST_CASE("leitura de pista: tres formatos") {
    SUBCASE("CSV do projeto, com comentarios") {
        const auto t = tr::parse_track(
            "# comentario\nlength_m,radius_m,slope,type\n10,0,0,straight\n5.5,8.2,0.01,Left\n");
        REQUIRE(t.segments.size() == 2);
        CHECK(t.segments[1].type == tr::SegmentType::Left);
        CHECK(t.segments[1].radius == 8.2);
        CHECK(t.segments[1].slope == 0.01);
        CHECK(t.length() == 15.5);
    }
    SUBCASE("matriz do MATLAB") {
        const auto t = tr::parse_track("index,length,radius,slope,type\n1,5,0,0,0\n2,51.5,8.2,0,1\n");
        REQUIRE(t.segments.size() == 2);
        CHECK(t.segments[1].type == tr::SegmentType::Left);
        CHECK(t.segments[1].length == 51.5);
    }
    SUBCASE("trackExcel / OptimumLap (CRLF, maiusculas)") {
        const auto t = tr::parse_track("Left;1;83.875\r\nSTRAIGHT;2.12;0\r\n\r\nright;1.5;20\r\n");
        REQUIRE(t.segments.size() == 3);
        CHECK(t.segments[0].type == tr::SegmentType::Left);
        CHECK(t.segments[1].type == tr::SegmentType::Straight);
        CHECK(t.segments[2].type == tr::SegmentType::Right);
        CHECK(t.segments[2].slope == 0.0);
    }
    SUBCASE("ida e volta pelo CSV do projeto preserva os valores exatos") {
        for (const auto& bt : kTracks) {
            const auto a = load_builtin(bt);
            const auto b = tr::parse_track(tr::track_to_csv(a, "teste"));
            REQUIRE(a.segments.size() == b.segments.size());
            for (std::size_t i = 0; i < a.segments.size(); ++i) {
                CHECK(a.segments[i].length == b.segments[i].length);
                CHECK(a.segments[i].radius == b.segments[i].radius);
                CHECK(a.segments[i].slope == b.segments[i].slope);
                CHECK(a.segments[i].type == b.segments[i].type);
            }
        }
    }
}

TEST_CASE("leitura de pista: erros com numero da linha") {
    CHECK_THROWS_WITH((void)tr::parse_track("length_m,radius_m,slope,type\n10,0,0,curva\n"),
                      doctest::Contains("linha 2"));
    CHECK_THROWS_WITH((void)tr::parse_track("length_m,radius_m,slope,type\n10,5,0,straight\n"),
                      doctest::Contains("raio 0"));
    CHECK_THROWS_WITH((void)tr::parse_track("length_m,radius_m,slope,type\n10,0,0,left\n"),
                      doctest::Contains("raio maior que 0"));
    CHECK_THROWS_WITH((void)tr::parse_track("length_m,radius_m,slope,type\n-1,0,0,straight\n"),
                      doctest::Contains("positivo"));
    CHECK_THROWS_WITH((void)tr::parse_track("comprimento,raio\n10,0\n"), doctest::Contains("cabecalho"));
    CHECK_THROWS_WITH((void)tr::parse_track("Left;abc;10\n"), doctest::Contains("comprimento invalido"));
    CHECK_THROWS_WITH((void)tr::parse_track("# so comentario\n"), doctest::Contains("nenhum segmento"));
}

TEST_CASE("segmento de largada e proxima curva") {
    const auto t = load_builtin(kTracks[0]);  // autox, 15 segmentos
    const auto r = tr::rotate_to_start(t.segments, 4);
    CHECK(r.front().length == t.segments[3].length);
    CHECK(r.back().length == t.segments[2].length);
    CHECK(tr::rotate_to_start(t.segments, 1).front().length == t.segments[0].length);
    CHECK_THROWS((void)tr::rotate_to_start(t.segments, 0));
    CHECK_THROWS((void)tr::rotate_to_start(t.segments, 16));

    std::vector<double> vc;
    for (const auto& c : tr::corner_speeds(car(), t.segments)) vc.push_back(c.v_max);
    CHECK(tr::next_corner_speed(t.segments, vc, 0) == vc[1]);
    CHECK(tr::next_corner_speed(t.segments, vc, 13) == std::numeric_limits<double>::infinity());

    // Clique sobre um ponto do segmento 5 escolhe o segmento 5
    const auto g = tr::build_geometry(t.segments);
    const std::size_t k = 4 * tr::kPointsPerSegment + 70;
    CHECK(tr::nearest_segment(g, t.segments.size(), g.x[k], g.y[k]) == 5);
}
