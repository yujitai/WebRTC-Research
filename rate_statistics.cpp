/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file rate_statistics.cpp
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/03/25
* @brief 
*****************************************************************/


#include "rate_statistics.h"

#include <iostream>
using namespace std;

namespace webrtc {

RateStatistics::RateStatistics(int64_t window_size_ms, float scale)
    : _buckets(new Bucket[window_size_ms]()),
      _accumulated_count(0),
      _num_samples(0),
      _oldest_time(-window_size_ms),
      _oldest_index(0),
      _scale(scale),
      _max_window_size_ms(window_size_ms),
      _current_window_size_ms(_max_window_size_ms) {}


RateStatistics::~RateStatistics() {}

void RateStatistics::Reset() {
    _accumulated_count = 0;
    _num_samples = 0;
    _oldest_time = -_max_window_size_ms;
    _oldest_index = 0;
    _current_window_size_ms = _max_window_size_ms;
    for (int64_t i = 0; i < _max_window_size_ms; i++)
        _buckets[i] = Bucket();
}

bool RateStatistics::IsInitialized() const {
    return _oldest_time != -_max_window_size_ms;
}

// 1.在刚开始启动的window_size_ms中, 不会进行擦除旧数据的操作, window满后才开始清除旧的数据
// 2.从oldest_index=0开始擦除旧数据,oldest_index会翻转,范围是[0, window_size_ms)
// 3.oldest_time在首次采样初始化时设置为now_ms,作为基准时间
void RateStatistics::EraseOld(int64_t now_ms) {
    if (!IsInitialized())
        return;

    // @@@@@@:核心 now - old + 1 > win_size 开始滑动窗口左边沿,即擦除旧的数据
    // New oldest time that is included in data set.
    // new_oldest_time 计算方式,减去_current_window_size_ms是为了保证装满window再擦除数据
    int64_t new_oldest_time = now_ms - _current_window_size_ms + 1;
    // New oldest time is older than the current one, no need to cull data.
    if (new_oldest_time <= _oldest_time) {
        // 经过窗口大小时间,窗口满后，才开始擦除数据
        cout << "[EraseOld failed] _oldest_time=" << _oldest_time <<  " new_oldest_time=" << new_oldest_time << endl;
        return;
    }

    cout << "[EraseOld] _oldest_time=" << _oldest_time << " new_oldest_time=" << new_oldest_time 
         << " now_ms=" << now_ms << " _current_window_size_ms=" << _current_window_size_ms << endl;

    // 删除[oldest_time,new_oldest_time)区间下的bucket
    // oldest_time 和 old_index 的对应关系？？？
    // 时间窗口左边沿oldest_time对应旧的数据的index即old_index
    // Loop over buckets and remove too old data points.
    while (_num_samples > 0 && _oldest_time < new_oldest_time) {
        const Bucket& oldest_bucket = _buckets[_oldest_index];
        // RTC_DCHECK_GE(accumulated_count_, oldest_bucket.sum);
        // RTC_DCHECK_GE(num_samples_, oldest_bucket.samples);
        _accumulated_count -= oldest_bucket.sum;
        _num_samples -= oldest_bucket.samples;
        _buckets[_oldest_index] = Bucket();
        if (++_oldest_index >= _max_window_size_ms)
            _oldest_index = 0;
        ++_oldest_time;
    }

    // 更新左边沿
    _oldest_time = new_oldest_time;
}

// 核心: 时间的滑动窗口 大小=window_size_ms,左边沿=oldest_time,右边沿=now_ms 采样点打到循环buffer上
// 计算窗口时间内的码率, 窗口越大采样点越多,计算越精确
// 前期还未增长到窗口大小(ms),无法计算码率
void RateStatistics::Update(size_t count, int64_t now_ms) {
    if (now_ms < _oldest_time) {
        // Too old data is ignored.
        return;
    }

    // 核心：先滑动窗口左边沿删除旧数据保持窗口大小不变，再更新窗口数据
    EraseOld(now_ms);

    // oldest_time在首次采样初始化时设置为now_ms,作为基准时间,可以看做是一个时间滑动窗口的左边沿
    // First ever sample, reset window to start now.
    if (!IsInitialized()) 
        _oldest_time = now_ms;

    // index翻转, time一直增长, offset增长到max_window_size_ms_-1就不变了, index [0, windowsize-1] 
    // index 和 time 的计算关系
    uint32_t now_offset = static_cast<uint32_t>(now_ms - _oldest_time);
    // RTC_DCHECK_LT(now_offset, max_window_size_ms_);
    uint32_t index = _oldest_index + now_offset;
    if (index >= _max_window_size_ms)
        index -= _max_window_size_ms;

    cout << "[Update] index=" << index << " _oldest_index=" << _oldest_index << " now_offset=" << now_offset
         << " now_ms=" << now_ms << " _oldest_time=" << _oldest_time << endl;
    _buckets[index].sum += count;
    ++_buckets[index].samples;
    _accumulated_count += count;
    ++_num_samples;
}

uint32_t RateStatistics::Rate(int64_t now_ms) const {
    // Yeah, this const_cast ain't pretty, but the alternative is to declare most
    // of the members as mutable...
    const_cast<RateStatistics*>(this)->EraseOld(now_ms);

    // 前期还未增长到窗口大小,无法计算码率
    // If window is a single bucket or there is only one sample in a data set that
    // has not grown to the full window size, treat this as rate unavailable.
    int64_t active_window_size = now_ms - _oldest_time + 1;
    if (_num_samples == 0 || active_window_size <= 1 ||
            (_num_samples <= 1 && active_window_size < _current_window_size_ms)) {
        return 0;
    }

    // bytes/ms -> bits/s
    float scale = _scale / active_window_size;
    return static_cast<uint32_t>(_accumulated_count * scale + 0.5f);
}

} // namespace webrtc


