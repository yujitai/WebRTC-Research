/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file trendline_estimator.cpp
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/03/13
* @brief 
*****************************************************************/


#include "trendline_estimator.h"

#include <math.h>

#include <iostream>
using namespace std;

namespace webrtc {

constexpr double kMaxAdaptOffsetMs = 15.0;
constexpr double kOverUsingTimeThreshold = 10;
constexpr int kMinNumDeltas = 60;
constexpr int kDeltaCounterMax = 1000;

double LinearFitSlope(const std::deque<std::pair<double, double>>& points) {
    if (points.size() <= 2) {
        cout << "invalid trend 0" << endl;
        return 0;
    }

    // 计算散列点的中心
    // Compute the "center of mass".
    double sum_x = 0;
    double sum_y = 0;
    for (const auto& point : points) {
        sum_x += point.first;
        sum_y += point.second;
    }
    double x_avg = sum_x / points.size(); 
    double y_avg = sum_y / points.size();

    // 计算直线斜率,公式如何推导的？已经找到推导过程，但要深究
    // Compute the slope k = \sum (x_i-x_avg)(y_i-y_avg) / \sum (x_i-x_avg)^2
    double numerator = 0;
    double denominator = 0;
    for (const auto& point : points) {
        numerator += (point.first - x_avg) * (point.second - y_avg);
        denominator += (point.first - x_avg) * (point.first - x_avg);
    }
    if (denominator == 0) 
        return 0;
    return numerator / denominator;
}

TrendlineEstimator::TrendlineEstimator(size_t window_size, double smoothing_coef, double threshold_gain)
    : _window_size(window_size),
      _smoothing_coef(smoothing_coef),
      _threshold_gain(threshold_gain),
      _num_of_deltas(0),
      _first_arrival_time_ms(-1),
      _accumulated_delay(0),
      _smoothed_delay(0),
      _delay_hist(),
      _k_up(0.0087),
      _k_down(0.039),
      _overusing_time_threshold(kOverUsingTimeThreshold),
      _threshold(12.5),
      _prev_modified_trend(NAN),
      _last_update_ms(-1),
      _prev_trend(0.0),
      _time_over_using(-1),
      _overuse_counter(0),
      _hypothesis(BandwidthUsage::kBwNormal) {}

TrendlineEstimator::~TrendlineEstimator() {}

void TrendlineEstimator::UpdateThreshold(double modified_trend, int64_t now_ms) {
    if (_last_update_ms == -1)
        _last_update_ms = now_ms;

    // 对于大的延迟趋势跳变不进行更新
    // 草案：del_var_th(i) SHOULD NOT be updated if this condition
    // holds: |m(i)| - del_var_th(i) > 15
    if (fabs(modified_trend) > _threshold + kMaxAdaptOffsetMs) {
        // Avoid adapting the threshold to big latency spikes, caused e.g.,
        // by a sudden capacity drop.
        _last_update_ms = now_ms;
        return;
    }

    // k_up > k_down why?
    const double k = fabs(modified_trend) < _threshold ? _k_down : _k_up;
    const int64_t kMaxTimeDeltaMs = 100;
    int64_t time_delta_ms = std::min(now_ms - _last_update_ms, kMaxTimeDeltaMs);
    // 阈值更新计算公式，为什么？
    // https://tools.ietf.org/html/draft-ietf-rmcat-gcc-02#section-5.1
    // 动态阈值公式: del_var_th(i) = del_var_th(i-1) + (t(i)-t(i-1)) * K(i) * (|m(i)|-del_var_th(i-1))
    // 为什么要动态？2点原因

    _threshold += k * (fabs(modified_trend) - _threshold) * time_delta_ms;
    //草案 It is also RECOMMENDED to clamp del_var_th(i) to the range [6, 600],
   // since a too small del_var_th(i) can cause the detector to become
   // overly sensitive.
    // _threshold = rtc::SafeClamp(_threshold, 6.f, 600.f);
    _last_update_ms = now_ms;
    cout << "更新动态阈值 threshold=" << _threshold << endl;
}

void TrendlineEstimator::Detect(double trend, double ts_delta, int64_t now_ms) {
    if (_num_of_deltas < 2) {
        _hypothesis = BandwidthUsage::kBwNormal;
        return;
    }

    // 为什么?
    // trendline乘以包组个数和增益得到调整后的斜率值与动态阈值比较
    const double modified_trend = std::min(_num_of_deltas, kMinNumDeltas) * trend * _threshold_gain;
    _prev_modified_trend = modified_trend;
    cout << "now_ms=" << now_ms << " modified_trend=" << modified_trend << " threshold=" << _threshold << endl; 

    if (modified_trend > _threshold) {
        // 计算带宽过载时长和次数
        if (_time_over_using == -1) {
            _time_over_using = ts_delta / 2;
        } else {
            _time_over_using += ts_delta;
        }
        _overuse_counter++;
        // TODO:不会立即更新
        if (_time_over_using > _overusing_time_threshold && _overuse_counter > 1) {
            if (trend >= _prev_trend) {
                _time_over_using = 0;
                _overuse_counter = 0;
                _hypothesis = BandwidthUsage::kBwOverusing;
            }
        }
    } else if (modified_trend < -_threshold) {
        _time_over_using = -1;
        _overuse_counter = 0;
        _hypothesis = BandwidthUsage::kBwUnderusing;
    } else {
        _time_over_using = -1;
        _overuse_counter = 0;
        _hypothesis = BandwidthUsage::kBwNormal;
    }
    _prev_trend = trend;

    cout << "BandwidthUsage=" << (int)_hypothesis << endl;
    // 每处理一个新包组信息，就会动态更新阈值
    UpdateThreshold(modified_trend, now_ms);
}

void TrendlineEstimator::Update(double recv_delta_ms, double send_delta_ms, int64_t arrival_time_ms) {

    // 计算延迟梯度
    const double delta_ms = recv_delta_ms - send_delta_ms;
    ++_num_of_deltas;
    _num_of_deltas = std::min(_num_of_deltas, kDeltaCounterMax);
    if (_first_arrival_time_ms == -1) 
        _first_arrival_time_ms = arrival_time_ms;

    // Exponential backoff filter.
    // 指数平滑算法
    // 计算累加延迟
    _accumulated_delay += delta_ms;

    // 计算平滑延迟 为什么这么计算?
    cout << "平滑系数=" << _smoothing_coef << " 上一次平滑延迟=" << _smoothed_delay 
         << " 累加延迟=" << _accumulated_delay << endl;
    _smoothed_delay = _smoothing_coef * _smoothed_delay + (1 - _smoothing_coef) * _accumulated_delay;
    cout << "新的平滑延迟=" << _smoothed_delay << endl;

    // 存储样本点, x轴为包组的到达用时, y轴为平滑后的延迟梯度和
    _delay_hist.push_back(std::make_pair(static_cast<double>(arrival_time_ms - _first_arrival_time_ms), _smoothed_delay));
    cout << "x=" << arrival_time_ms - _first_arrival_time_ms << " y=" << _smoothed_delay << endl;

    if (_delay_hist.size() > _window_size)
        _delay_hist.pop_front();
    double trend = _prev_trend;

    // 包组数达到窗口大小,计算延迟趋势
    if (_delay_hist.size() == _window_size) {
        // Update trend_ if it is possible to fit a line to the data. The delay
        // trend can be seen as an estimate of (send_rate - capacity)/capacity.
        // 0 < trend < 1   ->  the delay increases, queues are filling up
        //   trend == 0    ->  the delay does not change
        //   trend < 0     ->  the delay decreases, queues are being emptied
        // trend = LinearFitSlope(_delay_hist).value_or(trend);
        trend = LinearFitSlope(_delay_hist);
        cout << "trend=" << trend << endl;
    }

    // 带宽过载检测
    Detect(trend, send_delta_ms, arrival_time_ms);
}

} // namespace webrtc


