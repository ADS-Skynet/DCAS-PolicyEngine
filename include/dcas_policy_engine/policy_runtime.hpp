#pragma once

#include "dcas_policy_engine/state_timer_store.hpp"
#include "dcas_policy_engine/step_b.hpp"
#include "dcas_policy_engine/step_c.hpp"

namespace dcas {

struct RuntimeTickInput {
    StepBInput step_b{};
    StepCInput step_c{};
};

struct RuntimeTickOutput {
    StepBOutput step_b{};
    StepCOutput step_c{};
};

class PolicyRuntime {
public:
    explicit PolicyRuntime(std::uint64_t run_cycle_id = 1);

    RuntimeTickOutput Tick(const RuntimeTickInput& input);
    void ResetForNewRunCycle(std::uint64_t run_cycle_id);

private:
    StepBTransitionEngine step_b_engine_{};
    StepCPolicyEngine step_c_engine_{};
    StateTimerStore step_b_state_store_{};
    StepCLatchedState step_c_latched_state_{};
};

}  // namespace dcas
