#include <iostream>

#include "dcas_policy_engine/step_b.hpp"
#include "dcas_policy_engine/step_c.hpp"
#include "dcas_policy_engine/types.hpp"

int main() {
    dcas::StepBInput b_in{};
    b_in.current_state = dcas::DriverState::WARNING;
    b_in.is_attentive = false;
    b_in.reason = dcas::Reason::UNRESPONSIVE;
    b_in.inattentive_elapsed_s = 2.5;

    const dcas::StepBOutput b_out = dcas::evaluate_step_b(b_in);

    dcas::StepCInput c_in{};
    c_in.driver_state = b_out.next_state;
    c_in.reason = b_out.reason_used;
    c_in.lkas_throttle = 0.5;
    c_in.driver_override_lock_latched = false;

    const dcas::StepCOutput c_out = dcas::evaluate_step_c(c_in);

    std::cout << "StepB next_state: " << dcas::to_string(b_out.next_state) << "\n";
    std::cout << "StepB reason_used: " << dcas::to_string(b_out.reason_used) << "\n";
    std::cout << "StepC hmi_action: " << dcas::to_string(c_out.hmi_action) << "\n";
    std::cout << "StepC throttle_limit: " << c_out.throttle_limit << "\n";
    std::cout << "StepC mrm_active: " << (c_out.mrm_active ? "true" : "false") << "\n";
    std::cout << "StepC driver_override_lock: " << (c_out.driver_override_lock ? "true" : "false") << "\n";

    return 0;
}
