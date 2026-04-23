#include "dcas_policy_engine/policy_runtime.hpp"

namespace dcas {

PolicyRuntime::PolicyRuntime(std::uint64_t run_cycle_id) {
    step_c_latched_state_.run_cycle_id = run_cycle_id;
}

RuntimeTickOutput PolicyRuntime::Tick(const RuntimeTickInput& input) {
    RuntimeTickOutput output{};
    output.step_b = step_b_engine_.Evaluate(input.step_b, step_b_state_store_);

    StepCInput step_c_input = input.step_c;
    step_c_input.driver_state = output.step_b.next_state;
    step_c_input.reason = output.step_b.reason;
    step_c_input.reengagement_confirmed_200ms = output.step_b.reengagement_confirmed_200ms;
    step_c_input.latched_state = step_c_latched_state_;

    output.step_c = step_c_engine_.Evaluate(step_c_input);
    step_c_latched_state_ = output.step_c.next_latched_state;
    return output;
}

void PolicyRuntime::ResetForNewRunCycle(std::uint64_t run_cycle_id) {
    step_b_state_store_.ResetForNewRunCycle();
    step_c_latched_state_ = StepCLatchedState{};
    step_c_latched_state_.run_cycle_id = run_cycle_id;
}

}  // namespace dcas
