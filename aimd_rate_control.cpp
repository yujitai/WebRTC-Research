/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file aimd_rate_control.cpp
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/03/16
* @brief 
*****************************************************************/


#include "aimd_rate_control.h"

#include <cassert>
#include <cmath>

namespace webrtc {

// TODO:跑通单元测试 继续研究
AimdRateControl::AimdRateControl()
    : _min_configured_bitrate_bps(congestion_controller::GetMinBitrateBps()),
    _max_configured_bitrate_bps(30000000),
    _current_bitrate_bps(_max_configured_bitrate_bps),
    _latest_estimated_throughput_bps(_current_bitrate_bps),
    _avg_max_bitrate_kbps(-1.0f),
    _var_max_bitrate_kbps(0.4f),
    _rate_control_state(kRcHold),
    _rate_control_region(kRcMaxUnknown),
    _time_last_bitrate_change(-1),
    _time_last_bitrate_decrease(-1),
    _time_first_throughput_estimate(-1),
    _bitrate_is_initialized(false),
    // _beta(webrtc::field_trial::IsEnabled(kBweBackOffFactorExperiment)
    //        ? ReadBackoffFactor() : kDefaultBackoffFactor),
    _beta(kDefaultBackoffFactor),
    _rtt(kDefaultRttMs),
    _in_experiment(!AdaptiveThresholdExperimentIsDisabled()),
    _smoothing_experiment(webrtc::field_trial::IsEnabled("WebRTC-Audio-BandwidthSmoothing")),
    _in_initial_backoff_interval_experiment(webrtc::field_trial::IsEnabled(kBweInitialBackOffIntervalExperiment)),
    _initial_backoff_interval_ms(kDefaultInitialBackOffIntervalMs) 
{
    if (_in_initial_backoff_interval_experiment) {
        _initial_backoff_interval_ms = ReadInitialBackoffIntervalMs();
        // RTC_LOG(LS_INFO) << "Using aimd rate control with initial back-off interval"
        //    << " " << initial_backoff_interval_ms_ << " ms.";
    }
    // RTC_LOG(LS_INFO) << "Using aimd rate control with back off factor " << beta_;
}

AimdRateControl::~AimdRateControl() {}

uint32_t AimdRateControl::Update(const RateControlInput* input,
                                 int64_t now_ms) 
{
#if 0
    if (!bitrate_is_initialized_) {
        const int64_t kInitializationTimeMs = 5000;
        RTC_DCHECK_LE(kBitrateWindowMs, kInitializationTimeMs);
        if (time_first_throughput_estimate_ < 0) {
            if (input->estimated_throughput_bps)
                time_first_throughput_estimate_ = now_ms;
        } else if (now_ms - time_first_throughput_estimate_ >
                kInitializationTimeMs &&
                input->estimated_throughput_bps) {
            current_bitrate_bps_ = *input->estimated_throughput_bps;
            bitrate_is_initialized_ = true;
        }
    }
#endif
    _current_bitrate_bps = ChangeBitrate(_current_bitrate_bps, *input, now_ms);
    return _current_bitrate_bps;
}

// 这个函数在什么时候调用,意义?
void AimdRateControl::SetEstimate(int bitrate_bps, int64_t now_ms) {
    _bitrate_is_initialized = true;
    uint32_t prev_bitrate_bps = _current_bitrate_bps;
    _current_bitrate_bps = ClampBitrate(bitrate_bps, bitrate_bps);
    time_last_bitrate_change = now_ms;
    if (_current_bitrate_bps < prev_bitrate_bps) {
        _time_last_bitrate_decrease = now_ms;
    }
}

uint32_t AimdRateControl::ClampBitrate(uint32_t new_bitrate_bps, uint32_t estimated_throughput_bps) const {
    // Don't change the bit rate if the send side is too far off.
    // We allow a bit more lag at very low rates to not too easily get stuck if
    // the encoder produces uneven outputs.
    // 最大码率计算公式怎么得来的?
    const uint32_t max_bitrate_bps = static_cast<uint32_t>(1.5f * estimated_throughput_bps) + 10000;
    if (new_bitrate_bps > _current_bitrate_bps && new_bitrate_bps > max_bitrate_bps) {
        new_bitrate_bps = std::max(_current_bitrate_bps, max_bitrate_bps);
    }
    new_bitrate_bps = std::max(new_bitrate_bps, _min_configured_bitrate_bps);
    return new_bitrate_bps;
}

// 输入过载检测信号
// 计算码率变化趋势
// 输出调整后的码率
uint32_t AimdRateControl::ChangeBitrate(uint32_t new_bitrate_bps,
                                        const RateControlInput& input,
                                        int64_t now_ms) 
{
    // uint32_t estimated_throughput_bps = input.estimated_throughput_bps.value_or(latest_estimated_throughput_bps_);
    uint32_t estimated_throughput_bps = input.estimated_throughput_bps;
    if (input.estimated_throughput_bps)
        _latest_estimated_throughput_bps = *input.estimated_throughput_bps;

    // An over-use should always trigger us to reduce the bitrate, even though
    // we have not yet established our first estimate. By acting on the over-use,
    // we will end up with a valid estimate.
    if (!_bitrate_is_initialized && input.bw_state != BandwidthUsage::kBwOverusing)
        return _current_bitrate_bps;

    ChangeState(input, now_ms);
    // Calculated here because it's used in multiple places.
    const float estimated_throughput_kbps = estimated_throughput_bps / 1000.0f;
    // Calculate the max bit rate std dev given the normalized variance and the current throughput bitrate.
    // 计算最近一段时间的码率标准差,即码率波动范围
    // estimated_throughput_kbps应该在_avg_max_bitrate_kbps +/- 3 * std_max_bit_rate之间
    const float std_max_bit_rate = sqrt(_var_max_bitrate_kbps * _avg_max_bitrate_kbps);

    switch (_rate_control_state) {
        // hold状态不作处理,维持码率不变
        case kRcHold:
            break;

        case kRcIncrease:
            // 如果评估的网络吞吐量大于平均吞吐三个标准差
            // 认为均值不可靠，复位
            // kRcMaxUnknown 状态为比最高码率上界未知，设置乘性增加，放手增加
            if (_avg_max_bitrate_kbps >= 0 && estimated_throughput_kbps > _avg_max_bitrate_kbps + 3 * std_max_bit_rate) {
                _rate_control_region = kRcMaxUnknown;
                _avg_max_bitrate_kbps = -1.0;
            }
            // 码率已经接近最大值：增长需谨慎,加性增
            if (_rate_control_region == kRcNearMax) {
                uint32_t additive_increase_bps = AdditiveRateIncrease(now_ms, _time_last_bitrate_change);
                new_bitrate_bps += additive_increase_bps;
            } else { // 乘性增加
                uint32_t _multiplicative_increase_bps = MultiplicativeRateIncrease(
                        now_ms, _time_last_bitrate_change, new_bitrate_bps);
                new_bitrate_bps += multiplicative_increase_bps;
            }

            _time_last_bitrate_change = now_ms;
            break;

        case kRcDecrease:
            // Set bit rate to something slightly lower than max to get rid of any self-induced delay.
            // 乘性减, 系数beta=kDefaultBackoffFactor=0.85
            new_bitrate_bps = static_cast<uint32_t>(_beta * estimated_throughput_bps + 0.5);
            if (new_bitrate_bps > _current_bitrate_bps) {
                // Avoid increasing the rate when over-using.
                if (_rate_control_region != kRcMaxUnknown) {
                    new_bitrate_bps = static_cast<uint32_t>(_beta * avg_max_bitrate_kbps_ * 1000 + 0.5f);
                }
                new_bitrate_bps = std::min(new_bitrate_bps, _current_bitrate_bps);
            }
            _rate_control_region = kRcNearMax;

            if (_bitrate_is_initialized && estimated_throughput_bps < _current_bitrate_bps) {
                constexpr float kDegradationFactor = 0.9f;
                if (_smoothing_experiment &&
                        new_bitrate_bps < kDegradationFactor * _beta * _current_bitrate_bps) {
                    // If bitrate decreases more than a normal back off after overuse, it
                    // indicates a real network degradation. We do not let such a decrease
                    // to determine the bandwidth estimation period.
                    _last_decrease = absl::nullopt;
                } else {
                    _last_decrease = _current_bitrate_bps - new_bitrate_bps;
                }
            }

            // 如果评估的网络吞吐量小于平均吞吐三个标准差
            // 认为均值不可靠，复位
            if (estimated_throughput_kbps < _avg_max_bitrate_kbps - 3 * std_max_bit_rate) {
                avg_max_bitrate_kbps_ = -1.0f;
            }

            _bitrate_is_initialized = true;
            // 更新码率方差
            UpdateMaxThroughputEstimate(estimated_throughput_kbps);
            // Stay on hold until the pipes are cleared.
            // 参考有限状态机图：降低码率后回到HOLD状态，如果网络状态仍然不好，在Overuse仍然会进入Dec状态。
            // 如果恢复，则不会是Overuse，会保持或增长。
            // dec 只能向 hold 变化
            _rate_control_state = kRcHold;
            _time_last_bitrate_change = now_ms;
            _time_last_bitrate_decrease = now_ms;
            break;

        default:
            assert(false);
    }

    return ClampBitrate(new_bitrate_bps, estimated_throughput_bps);
}


uint32_t AimdRateControl::MultiplicativeRateIncrease(int64_t now_ms, int64_t last_ms, uint32_t current_bitrate_bps) const 
{
    // 系数与paper中1.05略有不同,时间差作为系数,1.08为底数
    double alpha = 1.08;
    if (last_ms > -1) {
        auto time_since_last_update_ms =
            rtc::SafeMin<int64_t>(now_ms - last_ms, 1000);
        alpha = pow(alpha, time_since_last_update_ms / 1000.0);
    }
    uint32_t multiplicative_increase_bps =
        std::max(current_bitrate_bps * (alpha - 1.0), 1000.0);
    return multiplicative_increase_bps;
}

uint32_t AimdRateControl::AdditiveRateIncrease(int64_t now_ms, int64_t last_ms) const {
    return static_cast<uint32_t>((now_ms - last_ms) *
            GetNearMaxIncreaseRateBps() / 1000);
}

int AimdRateControl::GetNearMaxIncreaseRateBps() const {
    // RTC_DCHECK_GT(current_bitrate_bps_, 0);
    double bits_per_frame = static_cast<double>(current_bitrate_bps_) / 30.0;
    double packets_per_frame = std::ceil(bits_per_frame / (8.0 * 1200.0));
    double avg_packet_size_bits = bits_per_frame / packets_per_frame;

    // 看不懂 ???
    // Approximate the over-use estimator delay to 100 ms.
    const int64_t response_time = _in_experiment ? (_rtt + 100) * 2 : _rtt + 100;
    constexpr double kMinIncreaseRateBps = 4000;
    return static_cast<int>(std::max(kMinIncreaseRateBps, (avg_packet_size_bits * 1000) / response_time));
}

// 这个函数用于计算码率的方差
void AimdRateControl::UpdateMaxThroughputEstimate(float estimated_throughput_kbps) {
    const float alpha = 0.05f;
    // 平滑计算最近一段时间的平均码率
    if (avg_max_bitrate_kbps_ == -1.0f) {
        avg_max_bitrate_kbps_ = estimated_throughput_kbps;
    } else {
        avg_max_bitrate_kbps_ = (1 - alpha) * avg_max_bitrate_kbps_ + alpha * estimated_throughput_kbps;
    }

    // 平滑计算码率方差
    // Estimate the max bit rate variance and normalize the variance
    // with the average max bit rate.
    const float norm = std::max(avg_max_bitrate_kbps_, 1.0f);
    var_max_bitrate_kbps_ =
        (1 - alpha) * var_max_bitrate_kbps_ +
        alpha * (avg_max_bitrate_kbps_ - estimated_throughput_kbps) *
        (avg_max_bitrate_kbps_ - estimated_throughput_kbps) / norm;
    
    // 设置最小和最大波动值
    // 0.4 ~= 14 kbit/s at 500 kbit/s
    if (var_max_bitrate_kbps_ < 0.4f) {
        var_max_bitrate_kbps_ = 0.4f;
    }
    // 2.5f ~= 35 kbit/s at 500 kbit/s
    if (var_max_bitrate_kbps_ > 2.5f) {
        var_max_bitrate_kbps_ = 2.5f;
    }
}


void AimdRateControl::ChangeState(const RateControlInput& input,
                                  int64_t now_ms) 
{
    switch(input.bw_state) {
        case BandwidthUsage::kBwNormal:
            // 我认为:在网络正常的情况下,如果原来码率处于Decrease状态,说明码率降的已经适应当前网络状况了，所以转向Hold状态；
            // 如果原来码率处于Hold/Increase状态，说明码率还有再提升的空间，所以转向Increase状态
            // 为什么没处理kRcDecrease的情况?
            if (_rate_control_state == kRcHold) {
                _time_last_bitrate_change = now_ms;
                _rate_control_state = kRcIncrease;
            }
        // 过载信号下码率控制状态均为kRcDecrease
        case BandwidthUsage::kBwOverusing:
            if (_rate_control_state != kRcDecrease) {
                _rate_control_state = kRcDecrease;
            }
            break;
        // 低载信号下码率控制状态均为kRcHold
        // 为什么? 我认为:网络链路低载说明网络链路带宽能力未充分利用,
        // 当前网络链路上传输的数据少,远未达到预估码率,所以不需要再increase,
        // 要么decrease要么hold,为了和过载时的decrease作区分,算法在这种情况下决定hold
        case BandwidthUsage::kBwUnderusing:
            _rate_control_state = kRcHold;
            break;
        default:
            assert(false);
    }
}

} // namespace webrtc






































