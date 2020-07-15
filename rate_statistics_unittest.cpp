/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file rate_statistics_unittest.cpp
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/03/25
* @brief 
*****************************************************************/

#include <iostream>
using namespace std;

#include "rate_statistics.h"
#include <rtcbase/time_utils.h>

namespace webrtc {

const int64_t kWindowMs = 500;
const float kBpsScale = 8000.0f;

void TestRateStatistics01() {
    RateStatistics stats(kWindowMs, kBpsScale);

    int64_t now_ms = 0;
    const uint32_t kPacketSize = 1500u;
#if 0

    stats.Update(kPacketSize, now_ms++);
    cout << stats.Rate(now_ms) << endl;

    stats.Update(kPacketSize, now_ms);
    cout << stats.Rate(now_ms) << endl;

    stats.Reset();
    cout << stats.Rate(now_ms) << endl;
#endif

    now_ms = 500;
    const int kInterval = 10;
    for (int i = 0; i < 1000; ++i) {
        if (i % kInterval == 0) // 每10ms更新一次数据
            stats.Update(kPacketSize, now_ms);

        // Approximately 1200 kbps expected. Not exact since when packets
        // are removed we will jump 10 ms to the next packet.
        if (i > kInterval) {
            cout << "[Rate] " << stats.Rate(now_ms) << endl;
            uint32_t samples = i / kInterval + 1;
            uint64_t total_bits = samples * kPacketSize * 8;
            uint32_t rate_bps = static_cast<uint32_t>((1000 * total_bits) / (i + 1));
            // cout << "rate_bps=" << rate_bps << endl;
        }
        now_ms += 1;
    }
}

void TestRateStatistics02() {
	RateStatistics stats(kWindowMs, kBpsScale);
	const uint32_t kPacketSize = 1200u;
	for (int seq = 1; seq <= 50; seq++) {
		stats.Update(kPacketSize, rtcbase::time_millis());	
        cout << "[Rate] " << stats.Rate(rtcbase::time_millis()) << endl;
	}
}

} // namespace webrtc

int main() {
    //webrtc::TestRateStatistics01();
    webrtc::TestRateStatistics02();
    return 0;
}


