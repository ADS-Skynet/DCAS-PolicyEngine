#pragma once

#include <algorithm>

#include "dcas_policy_engine/types.hpp"

namespace dcas {

class StateTimerStore {
public:
    explicit StateTimerStore(StepBStateStore initial = {}) : state_(initial) {}

    const StepBStateStore& Get() const { return state_; }

    void Replace(const StepBStateStore& state) { state_ = state; }

    void AdvanceInattentive(double delta_s) {
        state_.inattentive_elapsed_s += std::max(0.0, delta_s);
        state_.recover_elapsed_s = 0.0;
    }

    void AdvanceRecover(double delta_s) {
        state_.recover_elapsed_s += std::max(0.0, delta_s);
        state_.inattentive_elapsed_s = 0.0;
    }

    void Hold() {}

    void SetCurrentState(DriverState state) { state_.current_state = state; }

    void LatchAbsentRunCycle() {
        state_.absent_latched_run_cycle = true;
        state_.current_state = DriverState::ABSENT;
    }

    void ResetForNewRunCycle() {
        state_ = StepBStateStore{};
    }

private:
    StepBStateStore state_{};
};

}  // namespace dcas
