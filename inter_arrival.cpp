/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file inter_arrival.cpp
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/03/14
* @brief 
*****************************************************************/

#include <algorithm>
#include "inter_arrival.h"
#include "module_common_types_public.h"
#include <cassert>
#include <iostream>
using namespace std;

namespace webrtc {

static const int kBurstDeltaThresholdMs = 5;
static const int kMaxBurstDurationMs = 100;

InterArrival::InterArrival(uint32_t timestamp_group_length_ticks,
                           double timestamp_to_ms_coeff,
                           bool enable_burst_grouping)
    : kTimestampGroupLengthTicks(timestamp_group_length_ticks),
    _current_timestamp_group(),
    _prev_timestamp_group(),
    _timestamp_to_ms_coeff(timestamp_to_ms_coeff),
    _burst_grouping(enable_burst_grouping),
    _num_consecutive_reordered_packets(0) {}

// 根据当前包发送时间与当前包组第一个包的发送时间的差(即时间戳是否增长)判断是否有序发送
// 比较的基准是当前包组的首包的发送时间,如果一组包的发送时间虽然是降序发送但是都大于首包的发送时间,
// 那么不会被认为乱序
bool InterArrival::PacketInOrder(uint32_t timestamp) {
    if (_current_timestamp_group.IsFirstPacket()) {
        return true;
    } else {
        // Assume that a diff which is bigger than half the timestamp interval
        // (32 bits) must be due to reordering. This code is almost identical to
        // that in IsNewerTimestamp() in module_common_types.h.
        uint32_t timestamp_diff = timestamp - _current_timestamp_group.first_timestamp;
        /*cout << "debug:timestamp=" << timestamp << " current_timestamp_group.first_timestamp=" 
          << _current_timestamp_group.first_timestamp << " timestamp_diff=" << timestamp_diff 
          << " timestamp_diff-0x80000000=" << timestamp_diff - 0x80000000 << endl;*/
        return timestamp_diff < 0x80000000;
    }
}

bool InterArrival::NewTimestampGroup(int64_t arrival_time_ms, uint32_t timestamp) const {
    if (_current_timestamp_group.IsFirstPacket()) {
        return false;
    } else if (BelongsToBurst(arrival_time_ms, timestamp)) { // 突发数据burst不认为是新包组
        return false;
    } else {
        uint32_t timestamp_diff = timestamp - _current_timestamp_group.first_timestamp;
        // 与当前包组第一个包的发送时间间隔大于kTimestampGroupLengthTicks=5ms,认为是新包组
        // 思考为什么是5ms?
        // rfc:The Pacer sends a group of packets to the network every burst_time interval. 
        // RECOMMENDED value for burst_time is 5 ms. 
        return timestamp_diff > kTimestampGroupLengthTicks;
    }   
}

// 如何判断突发数据burst？
// 可能被路由器等网络设备进行了聚合
bool InterArrival::BelongsToBurst(int64_t arrival_time_ms, uint32_t timestamp) const {
    if (!_burst_grouping) {
        return false;
    }
    assert(_current_timestamp_group.complete_time_ms >= 0);
    int64_t arrival_time_delta_ms =
        arrival_time_ms - _current_timestamp_group.complete_time_ms;
    uint32_t timestamp_diff = timestamp - _current_timestamp_group.timestamp;
    // timestamp_to_ms_coeff_, 将rtp时间戳或者absloute time转化为ms, rtp时间戳默认频率为90khz
    int64_t ts_delta_ms = _timestamp_to_ms_coeff * timestamp_diff + 0.5;
    if (ts_delta_ms == 0)
        return true;
    int propagation_delta_ms = arrival_time_delta_ms - ts_delta_ms;
    // 判断burst的算法:
    // 传输延迟梯度<0(突发大量数据几乎在同一时刻到达)
    // 到达时间间隔<=kBurstDeltaThresholdMs(5ms)
    // 与当前包组的首包到达时间差<kMaxBurstDurationMs(100ms)
    // rfc: A Packet which has an inter-arrival time less than burst_time and
    // an inter-group delay variation d(i) less than 0 is considered
    // being part of the current group of packets.
    if (propagation_delta_ms < 0 &&
            arrival_time_delta_ms <= kBurstDeltaThresholdMs &&
            arrival_time_ms - _current_timestamp_group.first_arrival_ms <
            kMaxBurstDurationMs) 
    {
        cout << "[收到突发数据] 延迟梯度=" << propagation_delta_ms << " 到达时间间隔=" << arrival_time_delta_ms 
             << " 与当前包组首包到达时间差=" << arrival_time_ms - _current_timestamp_group.first_arrival_ms << endl;
        return true;
    }

    return false;
}

void InterArrival::Reset() {
    _num_consecutive_reordered_packets = 0;
    _current_timestamp_group = TimestampGroup();
    _prev_timestamp_group = TimestampGroup();
}

bool InterArrival::ComputeDeltas(uint32_t timestamp,
                                 int64_t arrival_time_ms,
                                 int64_t system_time_ms,
                                 size_t packet_size,
                                 uint32_t* timestamp_delta,
                                 int64_t* arrival_time_delta_ms,
                                 int* packet_size_delta) 
{
    assert(timestamp_delta != NULL);
    assert(arrival_time_delta_ms != NULL);
    assert(packet_size_delta != NULL);

    bool calculated_deltas = false;
    // 如果是包组的首个包,先存储,不计算
    if (_current_timestamp_group.IsFirstPacket()) {
        cout << "*首个包组到来" << endl;
        // We don't have enough data to update the filter, so we store it until we
        // have two frames of data to process.
        _current_timestamp_group.timestamp = timestamp;
        _current_timestamp_group.first_timestamp = timestamp;
        _current_timestamp_group.first_arrival_ms = arrival_time_ms;
    } else if (!PacketInOrder(timestamp)) { // 包发送是否有序?
        cout << "[包发送时间乱序,返回false] timestamp=" << timestamp << " current_timestamp_group.first_timestamp="  
             << _current_timestamp_group.first_timestamp << endl;
        return false;
    } else if (NewTimestampGroup(arrival_time_ms, timestamp)) { // 新的包组到来,计算deltas
        cout << "*新的包组到来" << endl;
        // First packet of a later frame, the previous frame sample is ready.
        if (_prev_timestamp_group.complete_time_ms >= 0) {
            // 包组最后一个包的发送时间和到达时间
            *timestamp_delta = _current_timestamp_group.timestamp - _prev_timestamp_group.timestamp;
            *arrival_time_delta_ms = _current_timestamp_group.complete_time_ms - _prev_timestamp_group.complete_time_ms;

            // 到达时间间隔较系统时间间隔发生跳变,大于阈值3000ms,重置
            // Check system time differences to see if we have an unproportional jump
            // in arrival time. In that case reset the inter-arrival computations.
            int64_t system_time_delta_ms =
                _current_timestamp_group.last_system_time_ms -
                _prev_timestamp_group.last_system_time_ms;
            if (*arrival_time_delta_ms - system_time_delta_ms >=
                    kArrivalTimeOffsetThresholdMs) {
                cout << "[到达时间跳变,重置,返回false] The arrival time clock offset has changed (diff = "
                    << *arrival_time_delta_ms - system_time_delta_ms
                    << " ms), resetting." << endl;
                Reset();
                return false;
            }

            // 乱序包组被重新排序,到达时间差<0,当前包组到达时间比上一个包组到达时间小,阈值=3
            if (*arrival_time_delta_ms < 0) {
                // The group of packets has been reordered since receiving its local arrival timestamp.
                ++_num_consecutive_reordered_packets;
                if (_num_consecutive_reordered_packets >= kReorderedResetThreshold) {
                    cout << "[重排序的包,到达时间乱序,到达时间间隔<0,重置] Packets are being reordered on the path from the "
                        "socket to the bandwidth estimator. Ignoring." << endl;/* this "
                        "packet for bandwidth estimation, resetting." << endl;;*/
                    Reset();
                    return false;
                }
                // cout << _current_timestamp_group.complete_time_ms << " " << _prev_timestamp_group.complete_time_ms << endl;
                cout << "[重排序的包,到达时间乱序,到达时间间隔<0,返回false] arrival_time_delta_ms=" << *arrival_time_delta_ms << endl;
                return false;
            } else {
                _num_consecutive_reordered_packets = 0;
            }
            assert(*arrival_time_delta_ms >= 0);
            *packet_size_delta = static_cast<int>(_current_timestamp_group.size) -
                static_cast<int>(_prev_timestamp_group.size);
            calculated_deltas = true;
        }
        _prev_timestamp_group = _current_timestamp_group;
        // The new timestamp is now the current frame.
        _current_timestamp_group.first_timestamp = timestamp;
        _current_timestamp_group.timestamp = timestamp;
        _current_timestamp_group.first_arrival_ms = arrival_time_ms;
        _current_timestamp_group.size = 0;
    } else { // 当前包组的包
        cout << "*当前包组的包" << endl;
        // ???
        _current_timestamp_group.timestamp = LatestTimestamp(_current_timestamp_group.timestamp, timestamp);
        // _current_timestamp_group.timestamp = timestamp;
    }

    // Accumulate the frame size.
    _current_timestamp_group.size += packet_size;
    _current_timestamp_group.complete_time_ms = arrival_time_ms;
    _current_timestamp_group.last_system_time_ms = system_time_ms;

    cout << "[计算Deltas]" << " 计算结果=" << calculated_deltas << " 发送时间=" << timestamp 
         << " 到达时间=" << arrival_time_ms << " 系统时间=" << system_time_ms << " 数据包大小=" << packet_size 
         << " 发送时间间隔=" << *timestamp_delta << " 到达时间间隔=" << *arrival_time_delta_ms 
         << " 包组大小差值=" << *packet_size_delta << endl;

    return calculated_deltas;
}

} // namespace webrtc


