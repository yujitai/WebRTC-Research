/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file aimd_rate_control.cpp
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/03/16
* @brief 
*****************************************************************/


#include "aimd_rate_control.h"

#include <inttypes.h>
#include <algorithm>
#include <cassert>
#include <cmath>

#include <iostream>
using namespace std;

namespace webrtc {

static const int64_t kDefaultRttMs = 200;
static const int64_t kMaxFeedbackIntervalMs = 1000;
static const float kDefaultBackoffFactor = 0.85f;
static const int64_t kDefaultInitialBackOffIntervalMs = 200;

namespace congestion_controller {

int GetMinBitrateBps() {
    constexpr int kAudioMinBitrateBps = 5000;
    constexpr int kMinBitrateBps = 10000;
    /*if (webrtc::field_trial::IsEnabled("WebRTC-Audio-SendSideBwe") &&
            !webrtc::field_trial::IsEnabled("WebRTC-Audio-SendSideBwe-For-Video")) {
        return kAudioMinBitrateBps;
    }*/
    return kMinBitrateBps;
}

}  // namespace congestion_controller

AimdRateControl::AimdRateControl()
    : _min_configured_bitrate_bps(congestion_controller::GetMinBitrateBps()),
    _max_configured_bitrate_bps(30000000),
    _current_bitrate_bps(_max_configured_bitrate_bps),
    _latest_estimated_throughput_bps(_current_bitrate_bps),
    _avg_max_bitrate_kbps(-1.0f),
    _var_max_bitrate_kbps(0.4f),
    _rate_control_state(kRcHold),
    _rate_control_region(kRcMaxUnknown), // gcc 草案,The subsystem starts in the increase state.
    _time_last_bitrate_change(-1),
    _time_last_bitrate_decrease(-1),
    _time_first_throughput_estimate(-1),
    _bitrate_is_initialized(false),
    // _beta(webrtc::field_trial::IsEnabled(kBweBackOffFactorExperiment)
    //        ? ReadBackoffFactor() : kDefaultBackoffFactor),
    _beta(kDefaultBackoffFactor),
    _rtt(kDefaultRttMs),
    // _in_experiment(!AdaptiveThresholdExperimentIsDisabled()),
    // _in_experiment(true),
    _in_experiment(false),
    // _smoothing_experiment(webrtc::field_trial::IsEnabled("WebRTC-Audio-BandwidthSmoothing")),
    _smoothing_experiment(true),
    _in_initial_backoff_interval_experiment(false),
    // _in_initial_backoff_interval_experiment(webrtc::field_trial::IsEnabled(kBweInitialBackOffIntervalExperiment)),
    _initial_backoff_interval_ms(kDefaultInitialBackOffIntervalMs) 
{
    if (_in_initial_backoff_interval_experiment) {
        // _initial_backoff_interval_ms = ReadInitialBackoffIntervalMs();
        // RTC_LOG(LS_INFO) << "Using aimd rate control with initial back-off interval"
        //    << " " << initial_backoff_interval_ms_ << " ms.";
    }
    // RTC_LOG(LS_INFO) << "Using aimd rate control with back off factor " << beta_;
}

AimdRateControl::~AimdRateControl() {}

// 过载状态下判断是否进一步降低码率
bool AimdRateControl::TimeToReduceFurther(
        int64_t now_ms,
        uint32_t estimated_throughput_bps) const 
{
    const int64_t bitrate_reduction_interval =
        std::max<int64_t>(std::min<int64_t>(_rtt, 200), 10);
    if (now_ms - _time_last_bitrate_change >= bitrate_reduction_interval) {
        return true;
    }

    // 为何小于当前发送码率的一半进一步降低???
    /*
    if (ValidEstimate()) {
        // TODO(terelius/holmer): Investigate consequences of increasing
        // the threshold to 0.95 * LatestEstimate().
        const uint32_t threshold = static_cast<uint32_t>(0.5 * LatestEstimate());
        return estimated_throughput_bps < threshold;
    }*/
    return false;
}

uint32_t AimdRateControl::LatestEstimate() const {
    return _current_bitrate_bps;
}

uint32_t AimdRateControl::Update(const RateControlInput* input, int64_t now_ms) {
    /*
    if (!_bitrate_is_initialized) {
        const int64_t k_initialization_time_ms = 5000;
        // RTC_DCHECK_LE(kBitrateWindowMs, kInitializationTimeMs);
        if (_time_first_throughput_estimate < 0) {
            if (input->estimated_throughput_bps)
                _time_first_throughput_estimate = now_ms;
        } else if (now_ms - _time_first_throughput_estimate >
                k_initialization_time_ms &&
                input->estimated_throughput_bps) 
        {
            _current_bitrate_bps = input->estimated_throughput_bps;
            _bitrate_is_initialized = true;
        }
    }
    */

    // cout << "[Update] " << "BandwidthUsage=" << static_cast<int>(input->bw_state) 
    cout << "[Update] " << "bandwidth_state=normal" 
         << " estimated_throughput_bps=" << input->estimated_throughput_bps << "bps"
         << " current_bitrate_bps=" << _current_bitrate_bps << "bps" << endl;

    _current_bitrate_bps = ChangeBitrate(_current_bitrate_bps, *input, now_ms);

    /*cout << "[Update] " << "BandwidthUsage=" << static_cast<int>(input->bw_state) 
         << " estimated_throughput_bps=" << input->estimated_throughput_bps
         << " _current_bitrate_bps=" << _current_bitrate_bps << endl; */

    return _current_bitrate_bps;
}

// 初始化当前码率
void AimdRateControl::SetEstimate(int bitrate_bps, int64_t now_ms) {
    _bitrate_is_initialized = true;
    uint32_t prev_bitrate_bps = _current_bitrate_bps;
    _current_bitrate_bps = ClampBitrate(bitrate_bps, bitrate_bps);
    _time_last_bitrate_change = now_ms;
    if (_current_bitrate_bps < prev_bitrate_bps) {
        _time_last_bitrate_decrease = now_ms;
    }
}

// 该函数将新的码率控制到 (min_configured_bitrate_bps_, a*estimated_throughput_bps+b) 之间，避免发送端码率增长过快。
// pv13, Finally, it is important to notice that Ar(ti) cannot exceed 1.5R(ti).
// 1.发送端评估的码率超过反馈码率acked bitrate 1.5倍,那么会被限制到最大码率不能继续增长
// 2.如果acked bitrate降低(比如突然降低一半),那么发送端BWE码率不会被限制到当前acked bitrate的1.5x以内，但是也不会增长
uint32_t AimdRateControl::ClampBitrate(uint32_t new_bitrate_bps, uint32_t estimated_throughput_bps) const {
    // max_bitrate_bps 最大码率计算公式怎么得来的???
    // Don't change the bit rate if the send side is too far off.
    // We allow a bit more lag at very low rates to not too easily get stuck if
    // the encoder produces uneven outputs.
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
        _latest_estimated_throughput_bps = input.estimated_throughput_bps;

    // An over-use should always trigger us to reduce the bitrate, even though
    // we have not yet established our first estimate. By acting on the over-use,
    // we will end up with a valid estimate.
    if (!_bitrate_is_initialized && input.bw_state != BandwidthUsage::kBwOverusing)
        return _current_bitrate_bps;

    
    // 速率控制状态机
    ChangeState(input, now_ms);

    // Calculated here because it's used in multiple places.
    const float estimated_throughput_kbps = estimated_throughput_bps / 1000.0f;
    // Calculate the max bit rate std dev given the normalized variance and the current throughput bitrate.
    // 计算最近一段时间的码率标准差,即码率波动范围 // estimated_throughput_kbps应该在_avg_max_bitrate_kbps +/- 3 * std_max_bit_rate之间 
    // 乘以_avg_max_bitrate_kbps是因为之前计算方差用_avg_max_bitrate_kbps normalize归一化了 
    // 再乘回来
    const float std_max_bit_rate = sqrt(_var_max_bitrate_kbps * _avg_max_bitrate_kbps);

    /*cout << "[ChangeBitrate] estimated_throughput_kbps=" << estimated_throughput_kbps 
         << " std_max_bit_rate=" << std_max_bit_rate
         << " _var_max_bitrate_kbps=" << _var_max_bitrate_kbps
         << " _avg_max_bitrate_kbps=" << _avg_max_bitrate_kbps << endl;*/

    switch (_rate_control_state) {
        // hold状态不作处理,维持码率不变
        case kRcHold:
            break;

        case kRcIncrease:
            // 如果评估的网络吞吐量大于平均吞吐三个标准差, 认为均值不可靠, 复位
            // kRcMaxUnknown 状态:最高码率上界(link capacity)是未知的，设置乘性增加，放手增加

            // If R_hat(i) increases above three standard deviations of the average
            // max bitrate, we assume that the current congestion level has changed,
            // at which point we reset the average max bitrate and go back to the multiplicative increase state.
            if (_avg_max_bitrate_kbps >= 0 && estimated_throughput_kbps > _avg_max_bitrate_kbps + 3 * std_max_bit_rate) {
                _rate_control_region = kRcMaxUnknown;
                _avg_max_bitrate_kbps = -1.0;
            }
            // 码率已经接近最大值：增长需谨慎,加性增
            // On every update the delay-based estimate of the available bandwidth
            // is increased, either multiplicatively or additively, depending on its
            //    current state.
            if (_rate_control_region == kRcNearMax) {
                uint32_t additive_increase_bps = AdditiveRateIncrease(now_ms, _time_last_bitrate_change);
                cout << "[AdditiveRateIncrease] additive_increase_bps=" << additive_increase_bps << endl;
                new_bitrate_bps += additive_increase_bps;
            } else { // 乘性增加, gcc草案,The subsystem starts in the increase state.
                uint32_t multiplicative_increase_bps = MultiplicativeRateIncrease(
                        now_ms, _time_last_bitrate_change, new_bitrate_bps);
                // cout << "[MultiplicativeRateIncrease] multiplicative_increase_bps=" << multiplicative_increase_bps << endl;
                new_bitrate_bps += multiplicative_increase_bps;
            }

            _time_last_bitrate_change = now_ms;
            break;

        case kRcDecrease:
            // Set bit rate to something slightly lower than max to get rid of any self-induced delay.
            // 码率回退系数beta=kDefaultBackoffFactor=0.85
            new_bitrate_bps = static_cast<uint32_t>(_beta * estimated_throughput_bps + 0.5);
            // cout << "[MultiplicativeRateDecrease] new_bitrate_bps=" << new_bitrate_bps << endl;
            cout << "[MultiplicativeRateDecrease] beta=" << _beta << " decrease_rate=" << estimated_throughput_bps-new_bitrate_bps << "bps" << endl;
            if (new_bitrate_bps > _current_bitrate_bps) {
                // Avoid increasing the rate when over-using.
                if (_rate_control_region != kRcMaxUnknown) {
                    new_bitrate_bps = static_cast<uint32_t>(_beta * _avg_max_bitrate_kbps * 1000 + 0.5f);
                }
                new_bitrate_bps = std::min(new_bitrate_bps, _current_bitrate_bps);
            }
            // 过载,码率处于降低状态,说明此时发送码率已经接近当前网络最大码率上界(link capacity.)
            // 速率范围设置为接近上界,此时如果要增加码率只能加性增.
            _rate_control_region = kRcNearMax;

            if (_bitrate_is_initialized && estimated_throughput_bps < _current_bitrate_bps) {
                constexpr float kDegradationFactor = 0.9f;
                if (_smoothing_experiment &&
                        new_bitrate_bps < kDegradationFactor * _beta * _current_bitrate_bps) {
                    // If bitrate decreases more than a normal back off after overuse, it
                    // indicates a real network degradation. We do not let such a decrease
                    // to determine the bandwidth estimation period.
                    // _last_decrease = absl::nullopt;
                    // 如果码率降低的比正常回退多,那么可能网络退化,不用于BWE周期的计算
                    _last_decrease = 0;
                } else {
                    _last_decrease = _current_bitrate_bps - new_bitrate_bps;
                }
            }

            // 如果评估的网络吞吐量小于平均吞吐三个标准差, 认为均值不可靠, 复位
            if (estimated_throughput_kbps < _avg_max_bitrate_kbps - 3 * std_max_bit_rate) {
                _avg_max_bitrate_kbps = -1.0f;
            }

            _bitrate_is_initialized = true;
            // 更新链路容量方差, 为什么码率降低时才更新?
            // 降低说明过载，过载说明可能达到了网络链路最大吐吞量,此时的吞吐量评估值才能作为链路容量值进行方差的计算
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

    cout << "[ChangeBitrate] new_bitrate_bps=" << ClampBitrate(new_bitrate_bps, estimated_throughput_bps) << "bps"<< endl;
    return ClampBitrate(new_bitrate_bps, estimated_throughput_bps);
}


// gcc 草案During multiplicative increase, the estimate is increased by at most 8% per second.
// eta = 1.08^min(time_since_last_update_ms / 1000, 1.0)
// A_hat(i) = eta * A_hat(i-1)
uint32_t AimdRateControl::MultiplicativeRateIncrease(int64_t now_ms, int64_t last_ms, uint32_t current_bitrate_bps) const 
{
    // 系数与paper中1.05略有不同,与草案1.08相同,时间差作为指数
    double alpha = 1.08;
    if (last_ms > -1) {
        // auto time_since_last_update_ms = rtc::SafeMin<int64_t>(now_ms - last_ms, 1000);
        auto time_since_last_update_ms = now_ms - last_ms;
        alpha = pow(alpha, time_since_last_update_ms / 1000.0);
    }
    uint32_t multiplicative_increase_bps =
        std::max(current_bitrate_bps * (alpha - 1.0), 1000.0);

    cout << "[MultiplicativeRateIncrease] " << "time_since_last_update_ms=" << now_ms - last_ms << "ms" << " alpha=" << alpha
         << " multiplicative_increase_bps=" << multiplicative_increase_bps << "bps" << endl;
    return multiplicative_increase_bps;
}

// 加性码率增长
uint32_t AimdRateControl::AdditiveRateIncrease(int64_t now_ms, int64_t last_ms) const {
    return static_cast<uint32_t>((now_ms - last_ms) *
            GetNearMaxIncreaseRateBps() / 1000);
}

// 根据当前码率计算(每秒)应该增加的码率（在使用带宽已经接近linked capacity的场景下）
// 增加一个包的大小 <-> 一个responsetime, 一秒钟增加多少码率?
// 所谓加性增:在response_time时间内增加一个包的大小packet_size_bits, 换算成码率（即1秒增加多少bits）
int AimdRateControl::GetNearMaxIncreaseRateBps() const {
    // RTC_DCHECK_GT(current_bitrate_bps_, 0);
    // 和草案计算一样 
    // 每帧码率/每帧包数=每包码率=9.6Kbps
    cout << "current_bitrate_bps=" << _current_bitrate_bps << "bps" << endl;
    double bits_per_frame = static_cast<double>(_current_bitrate_bps) / 30.0;
    cout << "bits_per_frame=" << bits_per_frame << "bits" << endl;
    // 每帧包数向上取整
    double packets_per_frame = std::ceil(bits_per_frame / (8.0 * 1200.0)); // 至少为1
    cout << "packets_per_frame=" << packets_per_frame << endl;
    double avg_packet_size_bits = bits_per_frame / packets_per_frame;
    cout << "avg_packet_size_bits=" << avg_packet_size_bits << "bits" << endl;

    // 包从发送到接收rtcp transport fedback再到过载检测的时间??
    // Approximate the over-use estimator delay to 100 ms.

    // increase_rate=avg_packet_size_bits/(rtt+delay)

    // 和草案不一致
    // During the additive increase the estimate is increased with at most
    // half a packet per response_time interval. 
    const int64_t response_time = _in_experiment ? (_rtt + 100) * 2 : _rtt + 100;
    cout << "response_time_interval=" << response_time << "ms" << endl;
    constexpr double kMinIncreaseRateBps = 4000;
    cout << "[GetNearMaxIncreaseRateBps] increase_rate=" << static_cast<int>(std::max(kMinIncreaseRateBps, (avg_packet_size_bits * 1000) / response_time)) / 1000  << "Kbps" << endl;
    // 低码率场景下(比如30Kbps)至少增加4Kbps
    return static_cast<int>(std::max(kMinIncreaseRateBps, (avg_packet_size_bits * 1000) / response_time));
}

// Returns the expected time between overuse signals (assuming steady state).???
// BWE周期[0.5s/2s, 50s] ??? 默认3s
// 拥塞过载状态增加BWE周期，减少探测???
int AimdRateControl::GetExpectedBandwidthPeriodMs() const {
    const int kMinPeriodMs = _smoothing_experiment ? 500 : 2000;
    constexpr int kDefaultPeriodMs = 3000;
    constexpr int kMaxPeriodMs = 50000;

    int increase_rate = GetNearMaxIncreaseRateBps();

    // 未发生拥塞,使用默认BWE探测周期3s
    if (!_last_decrease)
        return _smoothing_experiment ? kMinPeriodMs : kDefaultPeriodMs;

    // 为什么要用_last_decrease/increase_rate???
    // 拥塞过载场景下,rtt增大,increase_rate减小,BWE探测周期变大
    return std::min(kMaxPeriodMs,
            std::max<int>(1000 * static_cast<int64_t>(_last_decrease) /
                increase_rate,
                kMinPeriodMs));
}

// 计算最大码率的方差
// 在码率减少时进行估计??? 解决
void AimdRateControl::UpdateMaxThroughputEstimate(float estimated_throughput_kbps) {
    // 草案, It is RECOMMENDED to measure this average and standard deviation with an
    // exponential moving average with the smoothing factor 0.95, as it is
    // expected that this average covers multiple occasions at which we are in the Decrease state.
    const float alpha = 0.05f;

    // 指数平滑平均最大码率
    // 码率的指数移动均值
    if (_avg_max_bitrate_kbps == -1.0f) {
        _avg_max_bitrate_kbps = estimated_throughput_kbps;
    } else {
        _avg_max_bitrate_kbps = (1 - alpha) * _avg_max_bitrate_kbps + alpha * estimated_throughput_kbps;
    }

    // 平滑最大码率方差
    // 计算码率方差、normalize 意欲何为? 归一化? 再平滑
    // 也只能这么解释了:Estimate the max bit rate variance and normalize the variance with the average max bit rate.
    const float norm = std::max(_avg_max_bitrate_kbps, 1.0f);
    _var_max_bitrate_kbps =
        (1 - alpha) * _var_max_bitrate_kbps +
        alpha * (_avg_max_bitrate_kbps - estimated_throughput_kbps) *
        (_avg_max_bitrate_kbps - estimated_throughput_kbps) / norm;
    
    // 设置最小和最大波动值
    // 0.4 ~= 14 kbit/s at 500 kbit/s
    if (_var_max_bitrate_kbps < 0.4f) {
        _var_max_bitrate_kbps = 0.4f;
    }
    // 2.5f ~= 35 kbit/s at 500 kbit/s
    if (_var_max_bitrate_kbps > 2.5f) {
        _var_max_bitrate_kbps = 2.5f;
    }

    cout << "[UpdateMaxThroughputEstimate] " << "avg_max_bitrate_kbps=" << _avg_max_bitrate_kbps << "Kbps" 
         << " var_max_bitrate_kbps=" << _var_max_bitrate_kbps << endl;
}

// 码率控制状态机转换
// 1. Underuse总是进入Hold状态。
// 2. Overuse总是进入Dec状态。
// 3. Normal状态
// 3.1 当前在Hold、incr状态，此时会进入Inc状态
// 3.2 当前在衰减状态，此时会进入hold状态
void AimdRateControl::ChangeState(const RateControlInput& input, int64_t now_ms) {
    switch(input.bw_state) {
        case BandwidthUsage::kBwNormal:
            // 我认为:在网络正常的情况下,如果原来码率处于Decrease状态,说明码率降的已经适应当前网络状况了，所以转向Hold状态；
            // 如果原来码率处于Hold/Increase状态，说明码率还有再提升的空间，所以转向Increase状态
            // 为什么没处理kRcDecrease的情况?
            if (_rate_control_state == kRcHold) {
                _time_last_bitrate_change = now_ms;
                _rate_control_state = kRcIncrease;
            }
            break;
        // 过载信号下码率控制状态均为kRcDecrease
        case BandwidthUsage::kBwOverusing:
            if (_rate_control_state != kRcDecrease) {
                _rate_control_state = kRcDecrease;
            }
            break;
        // 低载信号下码率控制状态均为kRcHold,为什么?
        // 1.网络链路低载说明网络链路带宽能力未充分利用,说明当前网络链路的带宽质量完全可以承载当前发送码率，不需要做任何改变，保持即可，
        // 2.当前网络链路上传输的数据少,远未达到预估码率,所以不需要再increase, 要么decrease要么hold,为了和过载时的decrease作区分,算法在这种情况下决定hold
        case BandwidthUsage::kBwUnderusing:
            _rate_control_state = kRcHold;
            break;
        default:
            assert(false);
    }
    // cout << "[ChangeState] " << "BandwidthUsage=" << static_cast<int>(input.bw_state)
    //      << " _rate_control_state=" << _rate_control_state << endl;
    cout << "[ChangeState] " << "rate_control_state=increase" << endl;
}

} // namespace webrtc


