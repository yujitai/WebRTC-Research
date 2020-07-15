/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file rate_statistics.h
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/03/25
* @brief 
*****************************************************************/


#ifndef _RATE_STATISTICS_H
#define _RATE_STATISTICS_H

#include <stdint.h>
#include <stddef.h>

#include <memory>

namespace webrtc {

// 计算一段时间内的码率
class RateStatistics {
public:
    // 码率转换系数
    static constexpr float kBpsScale = 8000.0f;

    RateStatistics(int64_t max_window_size_ms, float scale);

    ~RateStatistics();

    // Reset instance to original state.
    void Reset();

    // Update rate with a new data point, moving averaging window as needed.
    void Update(size_t count, int64_t now_ms);

    // Note that despite this being a const method, it still updates the internal
    // state (moves averaging window), but it doesn't make any alterations that
    // are observable from the other methods, as long as supplied timestamps are
    // from a monotonic clock. Ie, it doesn't matter if this call moves the
    // window, since any subsequent call to Update or Rate would still have moved
    // the window as much or more.
    uint32_t Rate(int64_t now_ms) const;

private:
    void EraseOld(int64_t now_ms);
    bool IsInitialized() const;

    // 每ms对应一个bucket
    // Counters are kept in buckets (circular buffer), with one bucket per millisecond.
    struct Bucket {
        size_t sum;      // Sum of all samples in this bucket.
        size_t samples;  // Number of samples in this bucket.
    };
    std::unique_ptr<Bucket[]> _buckets;

    size_t _accumulated_count; // 总字节数
    size_t _num_samples; // 总样本个数 总包数
    int64_t _oldest_time;
    uint32_t _oldest_index;
    const float _scale;

    const int64_t _max_window_size_ms;
    int64_t _current_window_size_ms;
};

} // namespace webrtc

#endif // _RATE_STATISTICS_H


