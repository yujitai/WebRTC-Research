/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file trendline_estimator.h
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/03/12
* @brief 
*****************************************************************/


#ifndef _TRENDLINE_ESTIMATOR_H
#define _TRENDLINE_ESTIMATOR_H

#include <stddef.h>
#include <stdint.h>

#include <deque>

namespace webrtc {

enum class BandwidthUsage {
    kBwNormal = 0,
    kBwUnderusing = 1,
    kBwOverusing = 2,
    kLast
};

class TrendlineEstimator {
public:
    // window_size: 样本窗口,决定计算trend line的散列点数量
    // smoothing_coef: 平滑系数,控制平滑延迟的程度
    // threshold_gain:trendline阈值增益,乘以trendline slope, 用于接下来的过载检测

    // |window_size| is the number of points required to compute a trend line.
    // |smoothing_coef| controls how much we smooth out the delay before fitting
    // the trend line. |threshold_gain| is used to scale the trendline slope for
    // comparison to the old threshold. Once the old estimator has been removed
    // (or the thresholds been merged into the estimators), we can just set the
    // threshold instead of setting a gain.
    TrendlineEstimator(size_t window_size, double smoothing_coef, double threshold_gain);

    ~TrendlineEstimator();
    
    // Update the estimator with a new sample. The deltas should represent deltas
    // between timestamp groups as defined by the InterArrival class.
    void Update(double recv_delta_ms, double send_delta_ms, int64_t arrival_time_ms);

    BandwidthUsage State() const;

    // Used in unit tests.
    double modified_trend() const { return _prev_trend * _threshold_gain; }

private:
    void Detect(double trend, double ts_delta, int64_t now_ms);
    void UpdateThreshold(double modified_offset, int64_t now_ms);

    // Parameters.
    const size_t _window_size;
    const double _smoothing_coef;
    const double _threshold_gain;
    // Used by the existing threshold.
    int _num_of_deltas;
    // Keep the arrival times small by using the change from the first packet.
    int64_t _first_arrival_time_ms;
    // Exponential backoff filtering.
    double _accumulated_delay;
    double _smoothed_delay;
    // Linear least squares regression.
    std::deque<std::pair<double, double> > _delay_hist;


    // trendline阈值动态更新系数
    const double _k_up;
    const double _k_down;
    // 过载时长阈值,避免立即更新为过载状态
    double _overusing_time_threshold;
    double _threshold;
    double _prev_modified_trend;
    int64_t _last_update_ms;
    double _prev_trend;
    // 过载时长
    double _time_over_using;
    // 过载次数
    int _overuse_counter;
    BandwidthUsage _hypothesis;
};

} // namespace webrtc


#endif // _TRENDLINE_ESTIMATOR_H


