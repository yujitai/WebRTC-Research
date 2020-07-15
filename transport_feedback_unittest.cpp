/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file transport_feedback_unittest.cpp
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/05/09
* @brief 
*****************************************************************/

#include <memory>
#include <limits>

#include "zybrtc/modules/rtp_rtcp/rtcp_packet/transport_feedback.h"

using namespace zybrtc;
using namespace rtcp;

/*
g++ transport_feedback_unittest.cpp -std=c++11 -I/home/yujitai/dev/zrtc/voip/libzybrtc/output/include -I/home/yujitai/dev/zrtc/voip/libzybrtc/output/include/zybrtc -I/home/yujitai/dev/zrtc/voip/librtcbase/output/include -L/home/yujitai/dev/zrtc/voip/libzybrtc/output/include /home/yujitai/dev/zrtc/voip/libzybrtc/output/lib/libzybrtc.a /home/yujitai/dev/zrtc/voip/librtcbase/output/lib/librtcbase.a -lpthread -lrt -g
*/

// TransportFeedback_Limits
void TestTransportFeedback01() {
    // Sequence number wrap above 0x8000.
    std::unique_ptr<TransportFeedback> packet(new TransportFeedback());
    packet->set_base(0, 0);
    cout << packet->add_received_packet(0x0, 0) << endl;
    cout << packet->add_received_packet(0x8000, 1000) << endl;

    packet.reset(new TransportFeedback());
    packet->set_base(0, 0);
    cout << packet->add_received_packet(0x0, 0) << endl;
    // 保证添加的包是最新的包
    cout << packet->add_received_packet(0x8000+1, 1000) << endl;

    // packet status count 最大 65535?
    // Packet status count max 0xFFFF.
    packet.reset(new TransportFeedback());
    packet->set_base(0, 0);
    cout << packet->add_received_packet(0x0, 0) << endl;
    cout << packet->add_received_packet(0x8000, 1000) << endl;
    cout << packet->add_received_packet(0xFFFE, 2000) << endl;
    cout << packet->add_received_packet(0xFFFF, 3000) << endl;

    // Too large delta.
    packet.reset(new TransportFeedback());
    packet->set_base(0, 0);
    // recv delta 最大为 16bits signed*250us [-8192.0, 8191.75]ms
    int64_t kMaxPositiveTimeDelta = std::numeric_limits<int16_t>::max() * TransportFeedback::k_delta_scale_factor;
    cout << packet->add_received_packet(1, kMaxPositiveTimeDelta+TransportFeedback::k_delta_scale_factor) << endl;
    cout << packet->add_received_packet(1, kMaxPositiveTimeDelta) << endl;
}

int main() {
    TestTransportFeedback01();

    return 0;
}
