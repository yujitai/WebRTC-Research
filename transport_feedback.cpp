/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file transport_feedback.cpp
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/03/18
* @brief 
*****************************************************************/


#include "transport_feedback.h"

namespace xxx {

bool TransportFeedback::AddReceivedPacket(uint16_t sequence_number, int64_t timestamp_us) {
    // 看不懂 ???
    // Convert to ticks and round
    int64_t delta_full = (timestamp_us - _last_timestamp_us) % kTimeWrapPeriodUs
    if (delta_full > kTimeWrapPeriodUs / 2) 
        delta_full -= kTimeWrapPeriodUs;
    delta_full += delta_full < 0 ? -(kDeltaScaleFactor / 2) : kDeltaScaleFactor / 2;
    delta_full /= kDeltaScaleFactor;

    int16_t delta = static_cast<int16_t>(delta_full);
    // If larger than 16bit signed, we can't represent it - need new fb packet.
    if (delta != delta_full) {
        // RTC_LOG(LS_WARNING) << "Delta value too large ( >= 2^16 ticks )";
        return false;
    }

    uint16_t next_seq_no = base_seq_no_ + num_seq_no_;
    if (sequence_number != next_seq_no) {
        uint16_t last_seq_no = next_seq_no - 1;
        if (!IsNewerSequenceNumber(sequence_number, last_seq_no))
            return false;
        // 包未收到 Packet not received, recv delta size=0
        for (; next_seq_no != sequence_number; ++next_seq_no)
            if (!AddDeltaSize(0))
                return false;
    }

    // 包收到 Packet received recv delta size = 1 or 2
    DeltaSize delta_size = (delta >= 0 && delta <= 0xff) ? 1 : 2;
    if (!AddDeltaSize(delta_size))
        return false;

    packets_.emplace_back(sequence_number, delta);
    last_timestamp_us_ += delta * kDeltaScaleFactor;
    size_bytes_ += delta_size;
    return true;
}

} // namespace xxx


