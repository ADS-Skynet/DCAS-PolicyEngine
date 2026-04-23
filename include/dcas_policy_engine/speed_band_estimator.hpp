#pragma once

#include "dcas_policy_engine/types.hpp"

namespace dcas {

class SpeedBandEstimator {
public:
    SpeedBand Estimate(double ego_speed_mps) const {
        if (ego_speed_mps < 8.33) {
            return SpeedBand::LOW;
        }
        if (ego_speed_mps < 22.22) {
            return SpeedBand::MID;
        }
        return SpeedBand::HIGH;
    }
};

}  // namespace dcas
