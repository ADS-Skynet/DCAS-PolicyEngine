#include <iostream>

#include "dcas_policy_engine/policy_runtime.hpp"

int main() {
    dcas::PolicyRuntime runtime{};

    dcas::RuntimeTickInput tick{};
    tick.step_b.perception.is_attentive = false;
    tick.step_b.perception.is_attentive_ts_ms = 1000;
    tick.step_b.perception.reason = dcas::Reason::DROWSY;
    tick.step_b.perception.reason_ts_ms = 1000;
    tick.step_b.jetracer_input_0_4 = 0.2;
    tick.step_b.delta_s = 2.5;

    tick.step_c.previous_lkas_mode = dcas::LkasMode::ON_ACTIVE;
    tick.step_c.lkas_switch_event = dcas::LkasSwitchEvent::NONE;
    tick.step_c.notebook_input_alive = true;
    tick.step_c.driver_override = false;
    tick.step_c.lkas_throttle = 0.5;

    const dcas::RuntimeTickOutput output = runtime.Tick(tick);

    std::cout << "StepB next_state: " << dcas::to_string(output.step_b.next_state) << "\n";
    std::cout << "StepB reason: " << dcas::to_string(output.step_b.reason) << "\n";
    std::cout << "StepC hmi_action: " << dcas::to_string(output.step_c.hmi_action) << "\n";
    std::cout << "StepC next_lkas_mode: " << dcas::to_string(output.step_c.next_lkas_mode) << "\n";
    std::cout << "StepC throttle_limit: " << output.step_c.throttle_limit << "\n";
    return 0;
}
