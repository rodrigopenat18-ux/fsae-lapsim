#include "fsae/vehicle/engine.hpp"

#include <algorithm>
#include <utility>

namespace fsae {

EngineMap::EngineMap(std::vector<double> rpm, std::vector<double> torque_nm,
                     std::vector<double> bsfc_rpm, std::vector<double> bsfc_g_kwh)
    : torque_(std::move(rpm), std::move(torque_nm)),
      bsfc_(std::move(bsfc_rpm), std::move(bsfc_g_kwh)) {
    // dataEngine.m: rpm_min = min(DadosRot_rpm); rpm_max = max(DadosRot_rpm)
    const auto& x = torque_.x();
    rpm_min_ = *std::min_element(x.begin(), x.end());
    rpm_max_ = *std::max_element(x.begin(), x.end());
}

}  // namespace fsae
