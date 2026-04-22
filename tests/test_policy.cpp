#include <cstdlib>
#include <iostream>
#include <cmath>

#include "dcas_policy_engine/step_b.hpp"
#include "dcas_policy_engine/step_c.hpp"

namespace {

void expect_true(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        std::exit(1);
    }
}

void expect_near(double actual, double expected, double eps, const char* message) {
    if (std::fabs(actual - expected) > eps) {
        std::cerr << "[FAIL] " << message << " (actual=" << actual << ", expected=" << expected << ")\n";
        std::exit(1);
    }
}

void test_step_b_critical_immediate_absent() {
    dcas::StepBInput input{};
    input.current_state = dcas::DriverState::OK;
    input.is_attentive = false;
    input.reason = dcas::Reason::UNRESPONSIVE;
    input.inattentive_elapsed_s = 0.1;

    const auto out = dcas::evaluate_step_b(input);
    expect_true(out.next_state == dcas::DriverState::ABSENT, "StepB critical reason must jump to ABSENT");
    expect_true(out.absent_latched_run_cycle, "StepB ABSENT must latch run cycle");
}

void test_step_b_recover_from_warning_non_critical() {
    dcas::StepBInput input{};
    input.current_state = dcas::DriverState::WARNING;
    input.is_attentive = true;
    input.reason = dcas::Reason::PHONE;
    input.recover_elapsed_s = 1.3;
    input.t_recover_hold_s = 1.2;

    const auto out = dcas::evaluate_step_b(input);
    expect_true(out.next_state == dcas::DriverState::OK, "StepB should recover to OK when hold is satisfied");
}

void test_step_b_absent_latch_blocks_recovery() {
    dcas::StepBInput input{};
    input.current_state = dcas::DriverState::WARNING;
    input.is_attentive = true;
    input.absent_latched_run_cycle = true;
    input.recover_elapsed_s = 2.0;

    const auto out = dcas::evaluate_step_b(input);
    expect_true(out.next_state == dcas::DriverState::ABSENT, "StepB latch should keep ABSENT in same run cycle");
}

void test_step_c_intoxicated_locks_override() {
    dcas::StepCInput input{};
    input.driver_state = dcas::DriverState::ABSENT;
    input.reason = dcas::Reason::INTOXICATED;
    input.lkas_throttle = 0.6;

    const auto out = dcas::evaluate_step_c(input);
    expect_true(out.mrm_active, "StepC ABSENT must activate MRM");
    expect_true(out.driver_override_lock, "StepC intoxicated ABSENT must lock override");
}

void test_step_c_unresponsive_allows_override() {
    dcas::StepCInput input{};
    input.driver_state = dcas::DriverState::ABSENT;
    input.reason = dcas::Reason::UNRESPONSIVE;
    input.lkas_throttle = 0.6;

    const auto out = dcas::evaluate_step_c(input);
    expect_true(out.mrm_active, "StepC ABSENT must activate MRM");
    expect_true(!out.driver_override_lock, "StepC unresponsive ABSENT must allow override");
}

void test_step_c_escalation_drowsy_overlay() {
    dcas::StepCInput input{};
    input.driver_state = dcas::DriverState::ESCALATION;
    input.reason = dcas::Reason::DROWSY;
    input.lkas_throttle = 1.0;

    const auto out = dcas::evaluate_step_c(input);
    expect_true(out.hmi_action == dcas::HmiAction::DCA, "StepC ESCALATION should request DCA");
    expect_near(out.throttle_limit, 0.18, 1e-9, "StepC drowsy overlay should apply 10% more conservative throttle");
}

void test_step_c_driver_override_turns_off_control_path() {
    dcas::StepCInput input{};
    input.driver_state = dcas::DriverState::ESCALATION;
    input.reason = dcas::Reason::PHONE;
    input.lkas_throttle = 0.5;
    input.driver_override = true;
    input.driver_override_lock_latched = false;

    const auto out = dcas::evaluate_step_c(input);
    expect_true(out.hmi_action == dcas::HmiAction::INFO, "Driver override should switch to INFO");
    expect_true(!out.mrm_active, "Driver override should deactivate MRM on unlocked path");
    expect_true(out.throttle_limit == 0.0, "Driver override should disable control output");
}

}  // namespace

int main() {
    test_step_b_critical_immediate_absent();
    test_step_b_recover_from_warning_non_critical();
    test_step_b_absent_latch_blocks_recovery();
    test_step_c_intoxicated_locks_override();
    test_step_c_unresponsive_allows_override();
    test_step_c_escalation_drowsy_overlay();
    test_step_c_driver_override_turns_off_control_path();

    std::cout << "[PASS] All policy tests passed\n";
    return 0;
}
