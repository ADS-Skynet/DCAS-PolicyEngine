#pragma once

#include "dcas_policy_engine/types.hpp"

namespace dcas {

class SpeedBandEstimator {
public:
    SpeedBand Estimate(double jetracer_input_0_4) const {
        const double clamped = jetracer_input_0_4 < 0.0 ? 0.0 : (jetracer_input_0_4 > 0.4 ? 0.4 : jetracer_input_0_4);
        const double rho_v = clamped / 0.4;

        if (rho_v < 0.30) {
            return SpeedBand::LOW;
        }
        if (rho_v < 0.65) {
            return SpeedBand::MID;
        }
        return SpeedBand::HIGH;
    }
};

}  // namespace dcas
