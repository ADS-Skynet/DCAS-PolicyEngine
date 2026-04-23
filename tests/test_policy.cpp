#include <cmath>
#include <cstdlib>
#include <iostream>

#include "dcas_policy_engine/policy_runtime.hpp"

namespace {

void ExpectTrue(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        std::exit(1);
    }
}

void ExpectNear(double actual, double expected, double epsilon, const char* message) {
    if (std::fabs(actual - expected) > epsilon) {
        std::cerr << "[FAIL] " << message << " (actual=" << actual << ", expected=" << expected << ")\n";
        std::exit(1);
    }
}

void TestStepBRequiresSameSnapshot() {
    dcas::PolicyRuntime runtime{};
    dcas::RuntimeTickInput tick{};
    tick.step_b.perception.is_attentive = false;
    tick.step_b.perception.is_attentive_ts_ms = 1000;
    tick.step_b.perception.reason = dcas::Reason::UNRESPONSIVE;
    tick.step_b.perception.reason_ts_ms = 1001;
    tick.step_b.delta_s = 1.0;

    const auto output = runtime.Tick(tick);
    ExpectTrue(output.step_b.next_state == dcas::DriverState::OK, "mismatched timestamps must not advance state");
    ExpectTrue(output.step_b.input_snapshot_ts_ms == 0, "invalid snapshot must not emit snapshot timestamp");
}

void TestStepBCriticalReasonLatchesAbsent() {
    dcas::PolicyRuntime runtime{};
    dcas::RuntimeTickInput tick{};
    tick.step_b.perception.is_attentive = false;
    tick.step_b.perception.is_attentive_ts_ms = 1000;
    tick.step_b.perception.reason = dcas::Reason::UNRESPONSIVE;
    tick.step_b.perception.reason_ts_ms = 1000;
    tick.step_b.delta_s = 0.1;

    const auto output = runtime.Tick(tick);
    ExpectTrue(output.step_b.next_state == dcas::DriverState::ABSENT, "critical reason must jump to ABSENT");
    ExpectTrue(output.step_b.absent_latched_run_cycle, "ABSENT must latch for the run cycle");
}

void TestStepBRecoversToOkAfterHold() {
    dcas::PolicyRuntime runtime{};
    dcas::RuntimeTickInput inattentive{};
    inattentive.step_b.perception.is_attentive = false;
    inattentive.step_b.perception.is_attentive_ts_ms = 1000;
    inattentive.step_b.perception.reason = dcas::Reason::PHONE;
    inattentive.step_b.perception.reason_ts_ms = 1000;
    inattentive.step_b.delta_s = 3.5;
    inattentive.step_b.ego_speed_mps = 1.0;
    runtime.Tick(inattentive);

    dcas::RuntimeTickInput recover{};
    recover.step_b.perception.is_attentive = true;
    recover.step_b.perception.is_attentive_ts_ms = 2000;
    recover.step_b.perception.reason = dcas::Reason::PHONE;
    recover.step_b.perception.reason_ts_ms = 2000;
    recover.step_b.delta_s = 1.3;

    const auto output = runtime.Tick(recover);
    ExpectTrue(output.step_b.next_state == dcas::DriverState::OK, "recover hold should return state to OK");
    ExpectTrue(output.step_b.reason == dcas::Reason::NONE, "attentive snapshot must normalize reason to none");
}

void TestStepCWarningMapsToEor() {
    dcas::StepCPolicyEngine engine{};
    dcas::StepCInput input{};
    input.driver_state = dcas::DriverState::WARNING;
    input.reason = dcas::Reason::PHONE;
    input.previous_lkas_mode = dcas::LkasMode::ON_ACTIVE;
    input.lkas_throttle = 0.8;

    const auto output = engine.Evaluate(input);
    ExpectTrue(output.hmi_action == dcas::HmiAction::EOR, "WARNING must map to EOR");
    ExpectNear(output.throttle_limit, 0.56, 1e-9, "WARNING should reduce throttle by base gain");
}

void TestStepCWarningCanClearEorAfter200msConfirmation() {
    dcas::StepCPolicyEngine engine{};
    dcas::StepCInput input{};
    input.driver_state = dcas::DriverState::WARNING;
    input.reason = dcas::Reason::PHONE;
    input.previous_lkas_mode = dcas::LkasMode::ON_ACTIVE;
    input.lkas_throttle = 0.8;
    input.reengagement_confirmed_200ms = true;

    const auto output = engine.Evaluate(input);
    ExpectTrue(output.hmi_action == dcas::HmiAction::INFO, "confirmed reengagement should clear EOR while state is held");
    ExpectNear(output.throttle_limit, 0.56, 1e-9, "WARNING throttle should remain conservative until state recovers");
}

void TestStepCNotebookInputAliveOnlyBlocksActivation() {
    dcas::StepCPolicyEngine engine{};
    dcas::StepCInput input{};
    input.driver_state = dcas::DriverState::OK;
    input.reason = dcas::Reason::NONE;
    input.previous_lkas_mode = dcas::LkasMode::ON_ACTIVE;
    input.notebook_input_alive = false;
    input.lkas_throttle = 0.8;

    const auto output = engine.Evaluate(input);
    ExpectTrue(output.next_lkas_mode == dcas::LkasMode::ON_INACTIVE, "lost notebook input should block activation, not force OFF");
    ExpectTrue(output.hmi_action == dcas::HmiAction::INFO, "activation gating alone should not escalate HMI");
}

void TestStepCAbsentActivatesMrmAndLockoutOnSecondActivation() {
    dcas::StepCPolicyEngine engine{};
    dcas::StepCInput input{};
    input.driver_state = dcas::DriverState::ABSENT;
    input.reason = dcas::Reason::UNRESPONSIVE;
    input.previous_lkas_mode = dcas::LkasMode::ON_ACTIVE;
    input.lkas_throttle = 0.6;
    input.latched_state.mrm_activation_count_run_cycle = 1;

    const auto output = engine.Evaluate(input);
    ExpectTrue(output.mrm_active, "ABSENT must activate MRM");
    ExpectTrue(output.next_latched_state.driver_override_lock, "second MRM activation should lock override");
    ExpectTrue(output.next_lkas_mode == dcas::LkasMode::OFF, "lockout should force LKAS off");
}

}  // namespace

int main() {
    TestStepBRequiresSameSnapshot();
    TestStepBCriticalReasonLatchesAbsent();
    TestStepBRecoversToOkAfterHold();
    TestStepCWarningMapsToEor();
    TestStepCWarningCanClearEorAfter200msConfirmation();
    TestStepCNotebookInputAliveOnlyBlocksActivation();
    TestStepCAbsentActivatesMrmAndLockoutOnSecondActivation();
    std::cout << "[PASS] dcas_policy_tests\n";
    return 0;
}
