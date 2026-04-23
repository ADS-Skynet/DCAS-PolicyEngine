#pragma once

#include "dcas_policy_engine/types.hpp"

namespace dcas {

struct StepCInput {
    DriverState driver_state{DriverState::OK};
    Reason reason{Reason::NONE};
    bool notebook_input_alive{true};
    bool driver_override{false};
    double lkas_throttle{0.0};
    LkasMode previous_lkas_mode{LkasMode::OFF};
    LkasSwitchEvent lkas_switch_event{LkasSwitchEvent::NONE};
    bool reengagement_confirmed_200ms{false};
    ManoeuvreType current_manoeuvre_type{ManoeuvreType::NONE};
    StepCLatchedState latched_state{};
};

struct StepCOutput {
    double throttle_limit{0.0};
    HmiAction hmi_action{HmiAction::INFO};
    bool mrm_active{false};
    LkasMode next_lkas_mode{LkasMode::OFF};
    DashboardState dashboard_state{};
    StepCLatchedState next_latched_state{};
};

class StepCPolicyEngine {
public:
    StepCOutput Evaluate(const StepCInput& input) const;
};

}  // namespace dcas
