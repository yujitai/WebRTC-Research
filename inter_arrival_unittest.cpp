/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file inter_arrival_unittest.cpp
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/03/14
* @brief 
*****************************************************************/

#include <iostream>
using namespace std;

#include "inter_arrival.h"

namespace webrtc {

enum {
  kTimestampGroupLengthUs = 5000,
  kMinStep = 20,
  kTriggerNewGroupUs = kTimestampGroupLengthUs + kMinStep,
  kBurstThresholdMs = 5,
  kAbsSendTimeFraction = 18,
  kAbsSendTimeInterArrivalUpshift = 8,
  kInterArrivalShift = kAbsSendTimeFraction + kAbsSendTimeInterArrivalUpshift,
};

const double kRtpTimestampToMs = 1.0 / 90.0;
const double kAstToMs = 1000.0 / static_cast<double>(1 << kInterArrivalShift);

void TestInterArrival01() {
    InterArrival* inter_arrival = new InterArrival(kTimestampGroupLengthUs / 1000, 1.0, true);
    cout << "==========初始化InterArrival==========" << endl;
    cout << "包组大小(ms)=" << kTimestampGroupLengthUs / 1000 << " 时间戳转换系数=1.0" << " 开启burst" << endl;

    const size_t kPacketSize = 1000;
    uint32_t send_time_ms = 10000;
    int64_t arrival_time_ms = 20000;
    int64_t system_time_ms = 30000;

    uint32_t send_delta;
    int64_t arrival_delta;
    int size_delta;

    bool calculated_deltas = inter_arrival->ComputeDeltas(send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, 
            &send_delta, &arrival_delta, &size_delta);

    const int kTimeDeltaMs = 30;
    send_time_ms += kTimeDeltaMs;
    arrival_time_ms += kTimeDeltaMs;
    system_time_ms += kTimeDeltaMs;

    calculated_deltas = inter_arrival->ComputeDeltas(send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, 
            &send_delta, &arrival_delta, &size_delta);

    send_time_ms += kTimeDeltaMs;
    arrival_time_ms += kTimeDeltaMs + InterArrival::kArrivalTimeOffsetThresholdMs;
    system_time_ms += kTimeDeltaMs;
    calculated_deltas = inter_arrival->ComputeDeltas(send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, 
            &send_delta, &arrival_delta, &size_delta);

    send_time_ms += kTimeDeltaMs;
    arrival_time_ms += kTimeDeltaMs;
    system_time_ms += kTimeDeltaMs;
    // The previous arrival time jump should now be detected and cause a reset.
    calculated_deltas = inter_arrival->ComputeDeltas(send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, 
            &send_delta, &arrival_delta, &size_delta);

    // The two next packets will not give a valid delta since we're in the initial state.
    for (int i = 0; i < 2; i++) {
        send_time_ms += kTimeDeltaMs;
        arrival_time_ms += kTimeDeltaMs;
        system_time_ms += kTimeDeltaMs;
        calculated_deltas = inter_arrival->ComputeDeltas(send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, 
                &send_delta, &arrival_delta, &size_delta);
    }

    send_time_ms += kTimeDeltaMs;
    arrival_time_ms += kTimeDeltaMs;
    system_time_ms += kTimeDeltaMs;

    calculated_deltas = inter_arrival->ComputeDeltas(send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, 
            &send_delta, &arrival_delta, &size_delta);

    delete inter_arrival;
}

void TestInterArrival02() {
    InterArrival* inter_arrival = new InterArrival(kTimestampGroupLengthUs / 1000, 1.0, true);
    cout << "==========初始化InterArrival";
    cout << " 包组大小(ms)=" << kTimestampGroupLengthUs / 1000;
    cout << " 时间戳转换系数=1.0" << 
    cout << " 开启burst==========" << endl;

    // group1
    const size_t kPacketSize = 1000;
    uint32_t send_time_ms = 10000;
    int64_t arrival_time_ms = 20000;
    int64_t system_time_ms = 30000;
    uint32_t send_delta;
    int64_t arrival_delta;
    int size_delta;
    bool calculated_deltas = inter_arrival->ComputeDeltas(send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, 
            &send_delta, &arrival_delta, &size_delta);

    
    // group2
    const int kTimeDeltaMs = 30;
    send_time_ms += kTimeDeltaMs;
    arrival_time_ms += kTimeDeltaMs;
    system_time_ms += kTimeDeltaMs;
    calculated_deltas = inter_arrival->ComputeDeltas(send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, 
            &send_delta, &arrival_delta, &size_delta);

    // group3
    send_time_ms += kTimeDeltaMs;
    arrival_time_ms += kTimeDeltaMs;
    system_time_ms += kTimeDeltaMs;
    calculated_deltas = inter_arrival->ComputeDeltas(send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, 
            &send_delta, &arrival_delta, &size_delta);

    cout << "==========突发数据与到达时间乱序测试==========" << endl;
    // Three out of order will fail, after that we will be reset and two more will
    // fail before we get our first valid delta after the reset.
    // arrival_time_ms += 3000; // 测试到达时间相较于系统时间跳变
    arrival_time_ms -= 1000; // 测试到达时间乱序,到达时间间隔为负
    // group4~group9
    for (int i = 0; i < InterArrival::kReorderedResetThreshold + 3; ++i) {
        send_time_ms += kTimeDeltaMs;
        arrival_time_ms += kTimeDeltaMs;
        system_time_ms += kTimeDeltaMs;
        // The previous arrival time jump should now be detected and cause a reset.
        calculated_deltas = inter_arrival->ComputeDeltas(send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, 
                &send_delta, &arrival_delta, &size_delta);
    }

    // group10
    send_time_ms += kTimeDeltaMs;
    arrival_time_ms += kTimeDeltaMs;
    system_time_ms += kTimeDeltaMs;
    calculated_deltas = inter_arrival->ComputeDeltas(send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, 
            &send_delta, &arrival_delta, &size_delta);

    delete inter_arrival;
}

static uint32_t MakeRtpTimestamp(int64_t us) {
    return static_cast<uint32_t>(static_cast<uint64_t>(us * 90 + 500) / 1000);
}

void TestInterArrival03() {
    InterArrival* inter_arrival_rtp = new InterArrival(MakeRtpTimestamp(kTimestampGroupLengthUs), kRtpTimestampToMs, true);

    // group1
    int64_t arrival_time = 17;
    int64_t timestamp = 0;

    uint32_t delta_timestamp = 101;
    int64_t delta_arrival_time_ms = 303;
    int delta_packet_size = 909;
    bool computed = inter_arrival_rtp->ComputeDeltas(
            MakeRtpTimestamp(timestamp), arrival_time, arrival_time, 1,
            &delta_timestamp, &delta_arrival_time_ms, &delta_packet_size);
    int64_t g1_timestamp = timestamp;
    int64_t g1_arrival_time = arrival_time;

    // group2
    arrival_time += 11;
    timestamp += kTriggerNewGroupUs;
    computed = inter_arrival_rtp->ComputeDeltas(
            MakeRtpTimestamp(timestamp), arrival_time, arrival_time, 2,
            &delta_timestamp, &delta_arrival_time_ms, &delta_packet_size);
    for (int i = 0; i < 10; ++i) {
        arrival_time += kBurstThresholdMs + 1;
        timestamp += kMinStep;
        computed = inter_arrival_rtp->ComputeDeltas(
                MakeRtpTimestamp(timestamp), arrival_time, arrival_time, 1,
                &delta_timestamp, &delta_arrival_time_ms, &delta_packet_size);
    }
    int64_t g2_timestamp = timestamp;
    int64_t g2_arrival_time = arrival_time;

    // This packet is out of order and should be dropped.
    arrival_time = 281;
    computed = inter_arrival_rtp->ComputeDeltas(
            MakeRtpTimestamp(g1_timestamp), arrival_time, arrival_time, 100,
            &delta_timestamp, &delta_arrival_time_ms, &delta_packet_size);

    // group3
    arrival_time = 500;
    timestamp = 2 * kTriggerNewGroupUs;
    computed = inter_arrival_rtp->ComputeDeltas(
            MakeRtpTimestamp(timestamp), arrival_time, arrival_time, 100,
            &delta_timestamp, &delta_arrival_time_ms, &delta_packet_size);

    delete inter_arrival_rtp;
}

void TestInterArrival04() {
    InterArrival* inter_arrival_rtp = new InterArrival(MakeRtpTimestamp(kTimestampGroupLengthUs), kRtpTimestampToMs, true);

    int64_t arrival_time = 17;
    int64_t timestamp = 0;

    uint32_t delta_timestamp = 101;
    int64_t delta_arrival_time_ms = 303;
    int delta_packet_size = 909;
    bool computed = inter_arrival_rtp->ComputeDeltas(
            timestamp, arrival_time, arrival_time, 1,
            &delta_timestamp, &delta_arrival_time_ms, &delta_packet_size);
    int64_t g1_timestamp = timestamp;
    int64_t g1_arrival_time = arrival_time;

    timestamp += kTriggerNewGroupUs;
    arrival_time += 11;
    computed = inter_arrival_rtp->ComputeDeltas(
            MakeRtpTimestamp(timestamp), arrival_time, arrival_time, 2,
            &delta_timestamp, &delta_arrival_time_ms, &delta_packet_size);
    timestamp += 10 * kMinStep;
    int64_t g2_timestamp = timestamp;
    for (int i = 0; i < 10; ++i) {
        // These packets arrive with timestamps in decreasing order but are
        // nevertheless accumulated to group because their timestamps are higher
        // than the initial timestamp of the group.
        arrival_time += kBurstThresholdMs + 1;
        computed = inter_arrival_rtp->ComputeDeltas(
                MakeRtpTimestamp(timestamp), arrival_time, arrival_time, 1,
                &delta_timestamp, &delta_arrival_time_ms, &delta_packet_size);
        timestamp -= kMinStep;
    }
    int64_t g2_arrival_time = arrival_time;
    
    // However, this packet is deemed out of order and should be dropped.
    // 因为时间戳小于当前包组首包时间戳
    arrival_time = 281;
    timestamp = g1_timestamp;
    computed = inter_arrival_rtp->ComputeDeltas(
                MakeRtpTimestamp(timestamp), arrival_time, arrival_time, 100,
                &delta_timestamp, &delta_arrival_time_ms, &delta_packet_size);

    // group3
    timestamp = 2 * kTriggerNewGroupUs;
    arrival_time = 500;
    computed = inter_arrival_rtp->ComputeDeltas(
            MakeRtpTimestamp(timestamp), arrival_time, arrival_time, 100,
            &delta_timestamp, &delta_arrival_time_ms, &delta_packet_size);

    delete inter_arrival_rtp;
}

void TestInterArrival05() {
    InterArrival* inter_arrival_rtp = new InterArrival(MakeRtpTimestamp(kTimestampGroupLengthUs), kRtpTimestampToMs, true);
    
    // group1
    int64_t g1_arrival_time = 17;

    uint32_t delta_timestamp = 101;
    int64_t delta_arrival_time_ms = 303;
    int delta_packet_size = 909;
    bool computed = inter_arrival_rtp->ComputeDeltas(
            0, g1_arrival_time, g1_arrival_time, 1,
            &delta_timestamp, &delta_arrival_time_ms, &delta_packet_size);

    // group2
    int64_t timestamp = kTriggerNewGroupUs;
    int64_t arrival_time = 100;  // Simulate no packets arriving for 100 ms.
    for (int i = 0; i < 10; ++i) {
        // A bunch of packets arriving in one burst (within 5 ms apart).
        timestamp += 30000;
        arrival_time += kBurstThresholdMs;
        computed = inter_arrival_rtp->ComputeDeltas(
            MakeRtpTimestamp(timestamp), arrival_time, arrival_time, 1,
            &delta_timestamp, &delta_arrival_time_ms, &delta_packet_size);
    }
    int64_t g2_arrival_time = arrival_time;
    int64_t g2_timestamp = timestamp;

    // group3
    timestamp += 30000;
    arrival_time += kBurstThresholdMs + 1;
    computed = inter_arrival_rtp->ComputeDeltas(
            MakeRtpTimestamp(timestamp), arrival_time, arrival_time, 100,
            &delta_timestamp, &delta_arrival_time_ms, &delta_packet_size);

    delete inter_arrival_rtp;
}

} // namespace webrtc

int main() {
    // PositiveArrivalTimeJump
    // webrtc::TestInterArrival01();

    // NegativeArrivalTimeJump
    webrtc::TestInterArrival02();

    // OutOfOrderPacket
    // webrtc::TestInterArrival03();
    
    // OutOfOrderWithinGroup
    // webrtc::TestInterArrival04();

    // TwoBursts
    // webrtc::TestInterArrival05();

    return 0;   
}


