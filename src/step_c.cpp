#include "dcas_policy_engine/step_c.hpp"

#include <algorithm>
#include <array>

namespace dcas {
namespace {

Reason resolve_representative_reason(const StepCInput& input, std::vector<Reason>& normalized_set) {
    std::array<Reason, 6> priority = {
        Reason::UNRESPONSIVE,
        Reason::INTOXICATED,
        Reason::DROWSY,
        Reason::PHONE,
        Reason::UNKNOWN,
        Reason::NONE,
    };

    normalized_set = input.active_reason_set;
    if (normalized_set.empty()) {
        if (input.representative_reason.has_value()) {
            normalized_set.push_back(*input.representative_reason);
        } else {
            normalized_set.push_back(input.reason);
        }
    }

    for (Reason candidate : priority) {
        if (std::find(normalized_set.begin(), normalized_set.end(), candidate) != normalized_set.end()) {
            return candidate;
        }
    }
    return Reason::UNKNOWN;
}

double base_gain_from_state(DriverState state) {
    switch (state) {
        case DriverState::OK: return 1.0;
        case DriverState::WARNING: return 0.6;
        case DriverState::ESCALATION: return 0.2;
        case DriverState::ABSENT: return 0.0;
    }
    return 0.0;
}

double overlay_gain_from_reason(Reason reason) {
    switch (reason) {
        case Reason::DROWSY: return 0.9;
        case Reason::INTOXICATED: return 0.8;
        default: return 1.0;
    }
}

}  // namespace

StepCOutput evaluate_step_c(const StepCInput& input) {
    StepCOutput output{};
    output.driver_override_lock = input.driver_override_lock_latched;

    std::vector<Reason> reasons;
    output.representative_reason = resolve_representative_reason(input, reasons);
    output.active_reason_set = reasons;

    if (input.driver_override && !output.driver_override_lock) {
        output.throttle_limit = 0.0;
        output.hmi_action = HmiAction::INFO;
        output.mrm_active = false;
        return output;
    }

    const double base = base_gain_from_state(input.driver_state);
    const double overlay = overlay_gain_from_reason(output.representative_reason);
    output.throttle_limit = std::max(0.0, input.lkas_throttle * base * overlay);

    if (input.driver_state == DriverState::ABSENT) {
        output.throttle_limit = 0.0;
        output.hmi_action = HmiAction::MRM;
        output.mrm_active = true;
        if (output.representative_reason == Reason::INTOXICATED) {
            output.driver_override_lock = true;
        }
        return output;
    }

    output.mrm_active = false;
    switch (input.driver_state) {
        case DriverState::OK:
            output.hmi_action = HmiAction::INFO;
            break;
        case DriverState::WARNING:
            output.hmi_action = HmiAction::EOR;
            break;
        case DriverState::ESCALATION:
            output.hmi_action = HmiAction::DCA;
            break;
        case DriverState::ABSENT:
            output.hmi_action = HmiAction::MRM;
            break;
    }

    return output;
}

}  // namespace dcas
