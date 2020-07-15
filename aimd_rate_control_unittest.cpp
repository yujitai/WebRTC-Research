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

constexpr double kFractionAfterOveruse = 0.85;

// constexpr int64_t kClockInitialTime = 123456;
constexpr int64_t kClockInitialTime = 123;

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

    void AdvanceTimeMilliseconds(int64_t milliseconds) {
        AdvanceTimeMicroseconds(1000 * milliseconds);
    }

    void AdvanceTimeMicroseconds(int64_t microseconds) {
        _time_us += microseconds;
    }

private:
    int64_t _time_us;
};
SimulatedClock simulated_clock(123456);

// MinNearMaxIncreaseRateOnLowBandwith(4Kbps)
void TestAimdRateControl01() {
    AimdRateControl* aimd_rate_control = new AimdRateControl();

    constexpr int kBitrate = 30000;
    aimd_rate_control->SetEstimate(kBitrate, simulated_clock.TimeInMilliseconds());
    cout << "GetNearMaxIncreaseRateBps=" << aimd_rate_control->GetNearMaxIncreaseRateBps() << endl;

    delete aimd_rate_control;
}

// NearMaxIncreaseRateIs5kbpsOn90kbpsAnd200msRtt
void TestAimdRateControl02() {
    AimdRateControl* aimd_rate_control = new AimdRateControl();

    constexpr int kBitrate = 90000;
    aimd_rate_control->SetEstimate(kBitrate, simulated_clock.TimeInMilliseconds());
    aimd_rate_control->GetNearMaxIncreaseRateBps();
    // cout << "GetNearMaxIncreaseRateBps=" << aimd_rate_control->GetNearMaxIncreaseRateBps() << endl;
    // 5Kbps

    delete aimd_rate_control;
}

// GetIncreaseRateAndBandwidthPeriod
void TestAimdRateControl03() {
    AimdRateControl* aimd_rate_control = new AimdRateControl();

    // Smoothing experiment disabled
    constexpr int kBitrate = 300000;
    cout << "==========初始化AimdRateControl==========" << endl;
    aimd_rate_control->SetEstimate(kBitrate, simulated_clock.TimeInMilliseconds());
    RateControlInput input(BandwidthUsage::kBwOverusing, kBitrate);
    aimd_rate_control->Update(&input, simulated_clock.TimeInMilliseconds());
    cout << "GetNearMaxIncreaseRateBps=" << aimd_rate_control->GetNearMaxIncreaseRateBps() << endl;
    cout << "GetExpectedBandwidthPeriodMs=" << aimd_rate_control->GetExpectedBandwidthPeriodMs() << endl;

    delete aimd_rate_control;
}

/*
// BweLimitedByAckedBitrate
void TestAimdRateControl04() {
    AimdRateControl* aimd_rate_control = new AimdRateControl();

    constexpr int kAckedBitrate = 10000;
    // cout << "==========初始化AimdRateControl==========" << endl;
    aimd_rate_control->SetEstimate(kAckedBitrate, simulated_clock.TimeInMilliseconds());
    
    while (simulated_clock.TimeInMilliseconds() - kClockInitialTime < 20000) {
        cout << "======now_ms=" << simulated_clock.TimeInMilliseconds() << endl;
        RateControlInput input(BandwidthUsage::kBwNormal, kAckedBitrate);
        aimd_rate_control->Update(&input, simulated_clock.TimeInMilliseconds());
        simulated_clock.AdvanceTimeMilliseconds(100);
    }

    delete aimd_rate_control;
}
*/

void TestAimdRateControl04() {
    AimdRateControl* aimd_rate_control = new AimdRateControl();

    constexpr int kAckedBitrate = 10000;
    aimd_rate_control->SetEstimate(kAckedBitrate, simulated_clock.TimeInMilliseconds());
    
    while (simulated_clock.TimeInMilliseconds() - kClockInitialTime < 20000) {
        cout << "======now_ms=" << simulated_clock.TimeInMilliseconds() << endl;
        RateControlInput input(BandwidthUsage::kBwNormal, kAckedBitrate);
        aimd_rate_control->Update(&input, simulated_clock.TimeInMilliseconds());
        simulated_clock.AdvanceTimeMilliseconds(1000);
    }

    delete aimd_rate_control;
}

// BweNotLimitedByDecreasingAckedBitrate
void TestAimdRateControl05() {
    AimdRateControl* aimd_rate_control = new AimdRateControl();

    constexpr int kAckedBitrate = 100000;
    cout << "==========初始化AimdRateControl==========" << endl;
    aimd_rate_control->SetEstimate(kAckedBitrate, simulated_clock.TimeInMilliseconds());

    while (simulated_clock.TimeInMilliseconds() - 123456 < 20000) {
        RateControlInput input(BandwidthUsage::kBwNormal, kAckedBitrate);
        aimd_rate_control->Update(&input, simulated_clock.TimeInMilliseconds());
        simulated_clock.AdvanceTimeMilliseconds(100);
        cout << "========================================" << endl;
    }

    // If the acked bitrate decreases the BWE shouldn't be reduced to 1.5x
    // what's being acked, but also shouldn't get to increase more.
    uint32_t prev_estimate = aimd_rate_control->LatestEstimate();
    RateControlInput input(BandwidthUsage::kBwNormal, kAckedBitrate/2);
    aimd_rate_control->Update(&input, simulated_clock.TimeInMilliseconds());
    uint32_t new_estimate = aimd_rate_control->LatestEstimate();
    cout << "prev_estimate=" << prev_estimate << " new_estimate=" << new_estimate << endl;

    delete aimd_rate_control;
}

// BandwidthPeriodIsNotAboveMaxSmoothingExp
void TestAimdRateControl06() {
    AimdRateControl* aimd_rate_control = new AimdRateControl();

    // Smoothing experiment enabled
    constexpr int kInitialBitrate = 50000000;
    // cout << "==========初始化AimdRateControl==========" << endl;

    aimd_rate_control->SetEstimate(kInitialBitrate, simulated_clock.TimeInMilliseconds());
    //  Make a large (10 Mbps) bitrate drop to 10 kbps.
    constexpr int kAckedBitrate = 40000000 / kFractionAfterOveruse;
    RateControlInput input(BandwidthUsage::kBwOverusing, kAckedBitrate);
    aimd_rate_control->Update(&input, simulated_clock.TimeInMilliseconds());

    // cout << "GetExpectedBandwidthPeriodMs=" << aimd_rate_control->GetExpectedBandwidthPeriodMs() << endl;

    delete aimd_rate_control;
}

// SendingRateBoundedWhenThroughputNotEstimated
// discard
void TestAimdRateControl07() {
    AimdRateControl* aimd_rate_control = new AimdRateControl();

    constexpr int kInitialBitrateBps = 123000;
    RateControlInput input1(BandwidthUsage::kBwNormal, kInitialBitrateBps);
    aimd_rate_control->Update(&input1, simulated_clock.TimeInMilliseconds());

    simulated_clock.AdvanceTimeMilliseconds(5000 + 1);
    RateControlInput input2(BandwidthUsage::kBwNormal, kInitialBitrateBps);
    aimd_rate_control->Update(&input2, simulated_clock.TimeInMilliseconds());

    for (int i = 0; i < 100; i++) {
        RateControlInput input(BandwidthUsage::kBwNormal, 0);
        aimd_rate_control->Update(&input, simulated_clock.TimeInMilliseconds());
        simulated_clock.AdvanceTimeMilliseconds(100);
    }

    delete aimd_rate_control;
}

// ExpectedPeriodAfter20kbpsDropAnd5kbpsIncrease 
void TestAimdRateControl08() {
    AimdRateControl* aimd_rate_control = new AimdRateControl();
    constexpr int kInitialBitrate = 110000; // 110kbps
    aimd_rate_control->SetEstimate(kInitialBitrate, simulated_clock.TimeInMilliseconds());
    simulated_clock.AdvanceTimeMilliseconds(100);
    // Make the bitrate drop by 20 kbps to get to 90 kbps.
    // The rate increase at 90 kbps should be 5 kbps, so the period should be 4 s.
    constexpr int kAckedBitrate =
        (kInitialBitrate - 20000) / kFractionAfterOveruse;

    RateControlInput input(BandwidthUsage::kBwOverusing, kAckedBitrate);
    aimd_rate_control->Update(&input, simulated_clock.TimeInMilliseconds());

    // 测试过载之后下一次的BWE探测周期
    aimd_rate_control->GetNearMaxIncreaseRateBps(); // 90kbps/30/1packets/(rtt+delay)=5kbps
    aimd_rate_control->GetExpectedBandwidthPeriodMs(); // (110kbps-90kbps)/5kbps=4s

    delete aimd_rate_control;
}

} // namespace webrtc

int main() {
    // webrtc::TestAimdRateControl01();
    // webrtc::TestAimdRateControl02();
    // webrtc::TestAimdRateControl03();
    webrtc::TestAimdRateControl04();
    // webrtc::TestAimdRateControl05();
    // webrtc::TestAimdRateControl06();
    // webrtc::TestAimdRateControl07();
    // webrtc::TestAimdRateControl08();

    return 0;
}


