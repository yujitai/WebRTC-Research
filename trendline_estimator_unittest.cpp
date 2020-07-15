/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file trendline_estimator_unittest.cpp
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/03/12
* @brief 
*****************************************************************/

#include <iostream>
using namespace std;

#include "random.h"
#include "trendline_estimator.h"

namespace webrtc {

constexpr size_t kWindowSize = 20;
// constexpr double kSmoothing = 0.0;
constexpr double kSmoothing = 0.9;
constexpr double kGain = 1;
constexpr int64_t kAvgTimeBetweenPackets = 10;
constexpr size_t kPacketCount = 2 * kWindowSize + 1;

void TestEstimator(double slope, double jitter_stddev, double tolerance) {
    TrendlineEstimator estimator(kWindowSize, kSmoothing, kGain);
    cout << "=========初始化TrendlineEstimator==========" << endl;
    cout << "滤波窗口大小=" << kWindowSize << " 一次指数平滑系数=" << kSmoothing << " 延迟趋势斜率增益=" << kGain << endl;

    cout << "=========构造数据包组发送时间与接收时间==========" << endl;
    Random random(0x1234567);
    int64_t send_times[kPacketCount];
    int64_t recv_times[kPacketCount];
    int64_t send_start_time = random.Rand(1000000);
    int64_t recv_start_time = random.Rand(1000000);
    for (size_t i = 0; i < kPacketCount; ++i) {
        send_times[i] = send_start_time + i * kAvgTimeBetweenPackets;
        // cout << "数据包组" << i << " 发送时间=" << send_times[i] << endl;
        double latency = i * kAvgTimeBetweenPackets / (1 - slope);
        double jitter = random.Gaussian(0, jitter_stddev);
        recv_times[i] = recv_start_time + latency + jitter;
        // cout << "数据包组" << i << " 接收时间=" << recv_times[i] << " 延迟=" << latency << " 抖动=" << jitter << endl;
    }

    cout << "==========启动TrendlineEstimator==========" << endl;
    for (size_t i = 1; i < kPacketCount; i++) {
        double recv_delta = recv_times[i] - recv_times[i - 1];
        double send_delta = send_times[i] - send_times[i - 1];
        estimator.Update(recv_delta, send_delta, recv_times[i]);
        // cout << "数据包组" << i << "-" << i-1 << " 发送时间差=" << send_delta << " 到达时间差=" << recv_delta << " 到达时间=" << recv_times[i] 
        //     << " 延迟梯度=" << recv_delta - send_delta << " 延迟趋势斜率=" << estimator.modified_trend() << " 期望延迟趋势斜率=" << slope << endl;
        if (i < kWindowSize) {
            // EXPECT_NEAR(estimator.modified_trend(), 0, 0.001);
        } else {
            // EXPECT_NEAR(estimator.modified_trend(), slope, tolerance);
        }
    }
}

} // namespace webrtc

int main() {
    // PerfectLineSlopeOneHalf
    webrtc::TestEstimator(0.5, 0, 0.001);
#if 0    
    // PerfectLineSlopeMinusOne
    webrtc::TestEstimator(-1, 0, 0.001);
    
    // PerfectLineSlopeZero
    webrtc::TestEstimator(0, 0, 0.001);

    // JitteryLineSlopeOneHalf
    webrtc::TestEstimator(0.5, webrtc::kAvgTimeBetweenPackets / 3.0, 0.01);

    // JitteryLineSlopeMinusOne
    webrtc::TestEstimator(-1, webrtc::kAvgTimeBetweenPackets / 3.0, 0.075);

    // JitteryLineSlopeZero
    webrtc::TestEstimator(0, webrtc::kAvgTimeBetweenPackets / 3.0, 0.02);
#endif
    return 0;
}


