#pragma once

#include "dcas_policy_engine/types.hpp"

namespace dcas {

struct StepBInput {
    DriverState current_state{DriverState::OK};
    bool is_attentive{true};
    Reason reason{Reason::NONE};
    double inattentive_elapsed_s{0.0};
    double recover_elapsed_s{0.0};
    bool absent_latched_run_cycle{false};
    bool input_stale{false};

    double t_warn_eff_s{2.0};
    double t_esc_eff_s{4.0};
    double t_absent_eff_s{8.0};
    double t_recover_hold_s{1.2};
};

struct StepBOutput {
    DriverState next_state{DriverState::OK};
    Reason reason_used{Reason::NONE};
    bool absent_latched_run_cycle{false};
};

StepBOutput evaluate_step_b(const StepBInput& input);

}  // namespace dcas
