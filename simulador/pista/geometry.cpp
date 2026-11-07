#include "pista/geometry.hpp"

#include <algorithm>
#include <cmath>

#include "matematica/interp.hpp"

namespace fsae::track {

TrackGeometry build_geometry(const std::vector<Segment>& segments) {
    constexpr int N = kPointsPerSegment;
    TrackGeometry g;
    const std::size_t total = segments.size() * static_cast<std::size_t>(N);
    g.x.reserve(total);
    g.y.reserve(total);
    g.z.reserve(total);
    g.heading.reserve(total);

    double px = 0, py = 0, pz = 0;
    double angle = 0;

    for (const auto& s : segments) {
        const double L = s.length;
        const double R = s.radius;
        const double ds = L / N;
        const double dz = ds * std::tan(s.slope);

        for (int j = 0; j < N; ++j) {
            double x_new = 0, y_new = 0;
            switch (s.type) {
                case SegmentType::Straight:
                    x_new = px + ds * std::cos(angle);
                    y_new = py + ds * std::sin(angle);
                    break;
                case SegmentType::Left: {
                    const double cx = px - R * std::sin(angle);
                    const double cy = py + R * std::cos(angle);
                    angle = angle + (L / R) / N;
                    x_new = cx + R * std::sin(angle);
                    y_new = cy - R * std::cos(angle);
                    break;
                }
                case SegmentType::Right: {
                    const double cx = px + R * std::sin(angle);
                    const double cy = py - R * std::cos(angle);
                    angle = angle - (L / R) / N;
                    x_new = cx - R * std::sin(angle);
                    y_new = cy + R * std::cos(angle);
                    break;
                }
            }
            const double z_new = pz + dz;
            px = x_new;
            py = y_new;
            pz = z_new;
            g.x.push_back(x_new);
            g.y.push_back(y_new);
            g.z.push_back(z_new);
            g.heading.push_back(angle);
        }
    }

    // Vetores de direção: round(linspace(1, n, 50)) em índices do MATLAB (base 1)
    constexpr std::size_t kNumVectors = 50;
    if (total > 0) {
        for (double idx : math::linspace(1.0, static_cast<double>(total), kNumVectors)) {
            const auto k = static_cast<std::size_t>(std::round(idx)) - 1;
            g.vector_index.push_back(k);
            g.u.push_back(std::cos(g.heading[k]));
            g.v.push_back(std::sin(g.heading[k]));
        }
    }
    return g;
}

int nearest_segment(const TrackGeometry& geometry, std::size_t segment_count, double x, double y) {
    std::size_t best = 0;
    double best_d = INFINITY;
    for (std::size_t i = 0; i < geometry.size(); ++i) {
        const double dx = geometry.x[i] - x;
        const double dy = geometry.y[i] - y;
        const double d = std::sqrt(dx * dx + dy * dy);
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    // MATLAB: ceil(idx_min / 150) com idx_min em base 1
    const auto seg = static_cast<long long>((best + 1 + kPointsPerSegment - 1) / kPointsPerSegment);
    return static_cast<int>(std::clamp<long long>(seg, 1, static_cast<long long>(segment_count)));
}

}  // namespace fsae::track
