/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file aimd_rate_control_unittest.cpp
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/03/16
* @brief 
*****************************************************************/


#include <iostream>
using namespace std;

#include "aimd_rate_control.h"

namespace webrtc {

class SimulatedClock {
public:
    explicit SimulatedClock(int64_t initial_time_us)
        : _time_us(initial_time_us) {}

    ~SimulatedClock() {}

    // Return a timestamp in milliseconds relative to some arbitrary source; the
    // source is fixed for this clock.
    int64_t TimeInMilliseconds() const {
        return (_time_us + 500) / 1000;
    }

private:
    int64_t _time_us;
};
SimulatedClock simulated_clock(123456);

// GetIncreaseRateAndBandwidthPeriod
void TestAimdRateControl() {
    AimdRateControl* aimd_rate_control = new AimdRateControl();

    // Smoothing experiment disabled
    constexpr int kBitrate = 300000;
    cout << "==========初始化AimdRateControl==========" << endl;
    aimd_rate_control->SetEstimate(kBitrate, simulated_clock.TimeInMilliseconds());
    RateControlInput input(BandwidthUsage::kBwOverusing, kBitrate);
    aimd_rate_control->Update(&input, simulated_clock.TimeInMilliseconds());

    delete aimd_rate_control;
}

} // namespace webrtc

int main() {

    webrtc::TestAimdRateControl();

    return 0;
}


