#pragma once

#include "dcas_policy_engine/types.hpp"

namespace dcas {

class PerceptionAdapter {
public:
    NormalizedSnapshot Normalize(const PerceptionInput& input) const {
        if (input.is_attentive_ts_ms != input.reason_ts_ms) {
            return NormalizedSnapshot{};
        }

        NormalizedSnapshot snapshot{};
        snapshot.snapshot_valid = true;
        snapshot.is_attentive = input.is_attentive;
        snapshot.reason = input.is_attentive ? Reason::NONE : input.reason;
        snapshot.input_snapshot_ts_ms = input.is_attentive_ts_ms;
        return snapshot;
    }
};

}  // namespace dcas
