#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "fsae/io/csv.hpp"
#include "fsae/track/gpx.hpp"
#include "fsae/track/track.hpp"
#include "support/reference.hpp"

using fsae::test::check_against_reference;
using fsae::test::load_reference;
using fsae::test::reference_path;
namespace tr = fsae::track;

TEST_CASE("inclinacao a partir de GPX sintetico (gpx/, applyAltitudeFromGPX)") {
    const auto pts = tr::load_gpx(reference_path("gpx/synthetic.gpx"));
    CHECK(pts.size() == 1305);
    const auto track = tr::load_track(reference_path("gpx/segments_in.csv"));
    const auto r = tr::apply_gpx_altitude(track.segments, pts);

    const auto ref = load_reference("gpx/slopes.csv");
    REQUIRE(r.segments.size() == ref.rows());
    std::vector<double> slope;
    for (const auto& s : r.segments) slope.push_back(s.slope);
    // movmean, cosd e a soma acumulada podem diferir no último bit; o resultado
    // tem que bater com folga de 1e-12 relativo / 1e-12 absoluto.
    check_against_reference("alt_start", ref.column("alt_start"), r.alt_start, 1e-12);
    check_against_reference("alt_end", ref.column("alt_end"), r.alt_end, 1e-12);
    check_against_reference("delta_z", ref.column("delta_z"), r.delta_z, 1e-9, 1e-12);
    check_against_reference("slope", ref.column("slope"), slope, 1e-9, 1e-12);

    const auto z = fsae::io::read_key_value_csv(reference_path("gpx/z_error.csv"));
    CHECK(r.z_error_total == doctest::Approx(z.at("z_error_total")).epsilon(1e-12));

    // O resto do segmento não muda
    for (std::size_t i = 0; i < r.segments.size(); ++i) {
        CHECK(r.segments[i].length == track.segments[i].length);
        CHECK(r.segments[i].radius == track.segments[i].radius);
        CHECK(r.segments[i].type == track.segments[i].type);
    }
}

TEST_CASE("leitura de GPX: formatos e erros") {
    const std::string ok =
        "<gpx><trk><trkseg>"
        "<trkpt lat='-22.5' lon=\"-47.1\"><time>x</time><ele>10.5</ele></trkpt>\n"
        "<trkpt  lon=\"-47.2\" lat=\"-22.6\" >\n <ele> 11 </ele>\n</trkpt>"
        "</trkseg></trk></gpx>";
    const auto p = tr::parse_gpx(ok);
    REQUIRE(p.size() == 2);
    CHECK(p[0].lat == -22.5);
    CHECK(p[0].ele == 10.5);
    CHECK(p[1].lon == -47.2);
    CHECK(p[1].ele == 11.0);

    CHECK_THROWS_WITH((void)tr::parse_gpx("<trkpt lat=\"1\" lon=\"2\"></trkpt><trkpt lat=\"1\" lon=\"2\"><ele>1</ele></trkpt>"),
                      doctest::Contains("sem altitude"));
    CHECK_THROWS_WITH((void)tr::parse_gpx("<trkpt lon=\"2\"><ele>1</ele></trkpt>"),
                      doctest::Contains("lat ausente"));
    CHECK_THROWS_WITH((void)tr::parse_gpx("<gpx></gpx>"), doctest::Contains("menos de 2"));
}
