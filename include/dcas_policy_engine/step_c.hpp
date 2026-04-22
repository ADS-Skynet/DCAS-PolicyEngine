#pragma once

#include <optional>
#include <vector>

#include "dcas_policy_engine/types.hpp"

namespace dcas {

struct StepCInput {
    DriverState driver_state{DriverState::OK};
    Reason reason{Reason::NONE};
    double lkas_throttle{0.0};

    std::vector<Reason> active_reason_set{};
    std::optional<Reason> representative_reason{};

    bool driver_override{false};
    bool driver_override_lock_latched{false};
};

struct StepCOutput {
    double throttle_limit{0.0};
    HmiAction hmi_action{HmiAction::INFO};
    bool mrm_active{false};
    bool driver_override_lock{false};
    Reason representative_reason{Reason::UNKNOWN};
    std::vector<Reason> active_reason_set{};
};

StepCOutput evaluate_step_c(const StepCInput& input);

}  // namespace dcas
