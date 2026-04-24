#pragma once

#include "dcas_policy_engine/perception_adapter.hpp"
#include "dcas_policy_engine/speed_band_estimator.hpp"
#include "dcas_policy_engine/state_timer_store.hpp"
#include "dcas_policy_engine/threshold_scheduler.hpp"
#include "dcas_policy_engine/types.hpp"

namespace dcas {

struct StepBInput {
    PerceptionInput perception{};
    double jetracer_input_0_4{0.0};
    double delta_s{0.0};
    bool stale_fail_safe_enabled{false};
};

struct StepBOutput {
    DriverState next_state{DriverState::OK};
    Reason reason{Reason::NONE};
    bool absent_latched_run_cycle{false};
    std::int64_t input_snapshot_ts_ms{0};
    bool reengagement_confirmed_200ms{false};
};

class StepBTransitionEngine {
public:
    explicit StepBTransitionEngine(
        PerceptionAdapter adapter = {},
        SpeedBandEstimator speed_band_estimator = {},
        ThresholdScheduler threshold_scheduler = {});

    StepBOutput Evaluate(const StepBInput& input, StateTimerStore& state_store) const;

private:
    PerceptionAdapter adapter_{};
    SpeedBandEstimator speed_band_estimator_{};
    ThresholdScheduler threshold_scheduler_{};
};

}  // namespace dcas
