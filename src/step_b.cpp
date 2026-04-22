#include "dcas_policy_engine/step_b.hpp"

namespace dcas {

StepBOutput evaluate_step_b(const StepBInput& input) {
    const Reason normalized_reason = normalize_reason(input.reason, input.is_attentive);

    if (input.input_stale) {
        if (input.current_state == DriverState::OK) {
            return {DriverState::WARNING, normalized_reason, input.absent_latched_run_cycle};
        }
        return {input.current_state, normalized_reason, input.absent_latched_run_cycle};
    }

    if (input.current_state == DriverState::ABSENT || input.absent_latched_run_cycle) {
        return {DriverState::ABSENT, normalized_reason, true};
    }

    if (is_critical_reason(normalized_reason)) {
        return {DriverState::ABSENT, normalized_reason, true};
    }

    DriverState next_state = input.current_state;

    if (input.inattentive_elapsed_s >= input.t_absent_eff_s) {
        next_state = DriverState::ABSENT;
    } else if (input.inattentive_elapsed_s >= input.t_esc_eff_s) {
        next_state = DriverState::ESCALATION;
    } else if (input.inattentive_elapsed_s >= input.t_warn_eff_s) {
        next_state = DriverState::WARNING;
    }

    bool latch = false;
    if (next_state == DriverState::ABSENT) {
        return {DriverState::ABSENT, normalized_reason, true};
    }

    if ((input.current_state == DriverState::WARNING || input.current_state == DriverState::ESCALATION)
        && input.is_attentive) {
        if (input.recover_elapsed_s >= input.t_recover_hold_s) {
            return {DriverState::OK, normalized_reason, false};
        }
        if (input.recover_elapsed_s >= 0.2) {
            return {input.current_state, normalized_reason, false};
        }
    }

    return {next_state, normalized_reason, latch};
}

}  // namespace dcas
