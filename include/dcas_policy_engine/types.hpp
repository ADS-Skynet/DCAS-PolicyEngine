#pragma once

#include <string>

namespace dcas {

enum class DriverState {
    OK,
    WARNING,
    ESCALATION,
    ABSENT,
};

enum class Reason {
    PHONE,
    DROWSY,
    UNRESPONSIVE,
    INTOXICATED,
    NONE,
    UNKNOWN,
};

enum class HmiAction {
    INFO,
    EOR,
    DCA,
    MRM,
};

inline std::string to_string(DriverState state) {
    switch (state) {
        case DriverState::OK: return "OK";
        case DriverState::WARNING: return "WARNING";
        case DriverState::ESCALATION: return "ESCALATION";
        case DriverState::ABSENT: return "ABSENT";
    }
    return "UNKNOWN_STATE";
}

inline std::string to_string(Reason reason) {
    switch (reason) {
        case Reason::PHONE: return "phone";
        case Reason::DROWSY: return "drowsy";
        case Reason::UNRESPONSIVE: return "unresponsive";
        case Reason::INTOXICATED: return "intoxicated";
        case Reason::NONE: return "none";
        case Reason::UNKNOWN: return "unknown";
    }
    return "unknown";
}

inline std::string to_string(HmiAction action) {
    switch (action) {
        case HmiAction::INFO: return "INFO";
        case HmiAction::EOR: return "EOR";
        case HmiAction::DCA: return "DCA";
        case HmiAction::MRM: return "MRM";
    }
    return "INFO";
}

inline bool is_critical_reason(Reason reason) {
    return reason == Reason::UNRESPONSIVE || reason == Reason::INTOXICATED;
}

inline Reason normalize_reason(Reason input_reason, bool is_attentive) {
    return is_attentive ? Reason::NONE : input_reason;
}

}  // namespace dcas
