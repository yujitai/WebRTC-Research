/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file aimd_rate_control.h
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/03/16
* @brief 
*****************************************************************/


#ifndef _AIMD_RATE_CONTROL_H
#define _AIMD_RATE_CONTROL_H

#include <stdint.h>
#include <stddef.h> 

namespace webrtc {

enum class BandwidthUsage {
    kBwNormal = 0,
    kBwUnderusing = 1,
    kBwOverusing = 2,
    kLast
};


enum RateControlState { kRcHold, kRcIncrease, kRcDecrease };

enum RateControlRegion { kRcNearMax, kRcAboveMax, kRcMaxUnknown };

struct RateControlInput {
    RateControlInput(BandwidthUsage bw_state,
            // const absl::optional<uint32_t>& estimated_throughput_bps);
            const uint32_t& estimated_throughput_bps)
        : bw_state(bw_state), estimated_throughput_bps(estimated_throughput_bps) {}

    ~RateControlInput() = default;

    BandwidthUsage bw_state;
    uint32_t estimated_throughput_bps;
};

// A rate control implementation based on additive increases of
// bitrate when no over-use is detected and multiplicative decreases when
// over-uses are detected. When we think the available bandwidth has changes or
// is unknown, we will switch to a "slow-start mode" where we increase
// multiplicatively.
// 上面注释很重要!!!
class AimdRateControl {
public:
    AimdRateControl();
    ~AimdRateControl();

    uint32_t Update(const RateControlInput* input, int64_t now_ms);
    void SetEstimate(int bitrate_bps, int64_t now_ms);

private:
    // Update the target bitrate based on, among other things, the current rate
    // control state, the current target bitrate and the estimated throughput.
    // When in the "increase" state the bitrate will be increased either
    // additively or multiplicatively depending on the rate control region. When
    // in the "decrease" state the bitrate will be decreased to slightly below the
    // current throughput. When in the "hold" state the bitrate will be kept
    // constant to allow built up queues to drain.
    uint32_t ChangeBitrate(uint32_t current_bitrate, const RateControlInput& input, int64_t now_ms);

    uint32_t MultiplicativeRateIncrease(int64_t now_ms,
            int64_t last_ms,
            uint32_t current_bitrate_bps) const;
    uint32_t AdditiveRateIncrease(int64_t now_ms, int64_t last_ms) const;

    void ChangeState(const RateControlInput& input, int64_t now_ms);

    uint32_t _min_configured_bitrate_bps;
    uint32_t _max_configured_bitrate_bps;
    uint32_t _current_bitrate_bps;
    uint32_t _latest_estimated_throughput_bps;
    float _avg_max_bitrate_kbps;
    float _var_max_bitrate_kbps;
    RateControlState _rate_control_state;
    RateControlRegion _rate_control_region;
    int64_t _time_last_bitrate_change;
    int64_t _time_last_bitrate_decrease;
    int64_t _time_first_throughput_estimate;
    bool _bitrate_is_initialized;
    float _beta;
    int64_t _rtt;
    const bool _in_experiment_;
    const bool _smoothing_experiment_;
    const bool _in_initial_backoff_interval_experiment;
    int64_t _initial_backoff_interval_ms;
    int _last_decrease;
};

} // namespace webrtc

#endif // _AIMD_RATE_CONTROL_H


