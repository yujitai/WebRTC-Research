/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file inter_arrival.h
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/03/14
* @brief 
*****************************************************************/


#ifndef _INTER_ARRIVAL_H
#define _INTER_ARRIVAL_H

#include <stddef.h>
#include <stdint.h>

namespace webrtc {

// size_delta 的作用???
// Helper class to compute the inter-arrival time delta and the size delta
// between two timestamp groups. A timestamp is a 32 bit unsigned number with
// a client defined rate.
class InterArrival {
public:
    static constexpr int kReorderedResetThreshold = 3;
    // 到达时间间隔超过3000ms,重置包组滤波器
    static constexpr int64_t kArrivalTimeOffsetThresholdMs = 3000;

    InterArrival(uint32_t timestamp_group_length_ticks,
                 double timestamp_to_ms_coeff,
                 bool enable_burst_grouping);

    bool ComputeDeltas(uint32_t timestamp, 
                       int64_t arrival_time_ms,
                       int64_t system_time_ms,
                       size_t packet_size,
                       uint32_t* timestamp_delta,
                       int64_t* arrival_time_delta_ms,
                       int* packet_size_delta);

private:
    struct TimestampGroup {
        TimestampGroup() 
            : size(0),
              first_timestamp(0),
              timestamp(0),
              first_arrival_ms(-1),
              complete_time_ms(-1) {}

        bool IsFirstPacket() const { return complete_time_ms == -1; }

        size_t size;
        uint32_t first_timestamp; // 首包发送时间
        uint32_t timestamp; // 发送时间
        int64_t first_arrival_ms; // 首包到达时间
        int64_t complete_time_ms; // 到达时间
        int64_t last_system_time_ms;
    };

    // Returns true if the packet with timestamp |timestamp| arrived in order.
    bool PacketInOrder(uint32_t timestamp);

    // Returns true if the last packet was the end of the current batch and the
    // packet with |timestamp| is the first of a new batch.
    bool NewTimestampGroup(int64_t arrival_time_ms, uint32_t timestamp) const;

    bool BelongsToBurst(int64_t arrival_time_ms, uint32_t timestamp) const;

    void Reset();

    const uint32_t kTimestampGroupLengthTicks;
    TimestampGroup _current_timestamp_group;
    TimestampGroup _prev_timestamp_group;
    double _timestamp_to_ms_coeff;
    bool _burst_grouping;
    int _num_consecutive_reordered_packets;
};

} // namespace webrtc

#endif // _INTER_ARRIVAL_H


