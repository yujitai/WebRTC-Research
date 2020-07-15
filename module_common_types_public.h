/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file modile_common_types_pbulic.h
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/03/20
* @brief 
*****************************************************************/


#ifndef _MODULE_COMMON_TYPES_PUBLIC_H
#define _MODULE_COMMON_TYPES_PUBLIC_H

#include <limits>
#include <stdint.h>
#include <iostream>
using namespace std;

namespace webrtc {

// 必须是无符号数
template <typename U>
inline bool IsNewer(U value, U prev_value) {
  static_assert(!std::numeric_limits<U>::is_signed, "U must be unsigned");
  // kBreakpoint is the half-way mark for the type U. For instance, for a
  // uint16_t it will be 0x8000, and for a uint32_t, it will be 0x8000000.
  constexpr U kBreakpoint = (std::numeric_limits<U>::max() >> 1) + 1;

  cout << "[IsNewer] value=" << value << " prev_value=" << prev_value 
      << " value-prev_value=" << static_cast<U>(value-prev_value) << " kBreakpoint=" << kBreakpoint << endl;

  // Distinguish between elements that are exactly kBreakpoint apart.
  // If t1>t2 and |t1-t2| = kBreakpoint: IsNewer(t1,t2)=true,
  // IsNewer(t2,t1)=false
  // rather than having IsNewer(t1,t2) = IsNewer(t2,t1) = false.

  // kBreakpoint一定是正数，这种场景发生时一定是value>prev_value
  if (value - prev_value == kBreakpoint) {
    return value > prev_value;
  }

  // cout << "fuck value=" << value << " prev_value=" << prev_value 
  //    << " value-prev_value=" << static_cast<U>(value-prev_value) << " kBreakpoint=" << kBreakpoint << endl;

  // 肯定不能相等
  // 1.距离小于一半 
  // 2.距离等于一半(value>prev_value的情况下)
  // 认为value>prev_value value为最新的包号
  return value != prev_value &&
         static_cast<U>(value - prev_value) < kBreakpoint;
}

// NB: Doesn't fulfill strict weak ordering requirements.
//     Mustn't be used as std::map Compare function.
inline bool IsNewerSequenceNumber(uint16_t sequence_number,
                                  uint16_t prev_sequence_number) {
  return IsNewer(sequence_number, prev_sequence_number);
}

// NB: Doesn't fulfill strict weak ordering requirements.
//     Mustn't be used as std::map Compare function.
inline bool IsNewerTimestamp(uint32_t timestamp, uint32_t prev_timestamp) {
  return IsNewer(timestamp, prev_timestamp);
}

inline uint16_t LatestSequenceNumber(uint16_t sequence_number1,
                                     uint16_t sequence_number2) {
  return IsNewerSequenceNumber(sequence_number1, sequence_number2)
             ? sequence_number1
             : sequence_number2;
}

inline uint32_t LatestTimestamp(uint32_t timestamp1, uint32_t timestamp2) {
  return IsNewerTimestamp(timestamp1, timestamp2) ? timestamp1 : timestamp2;
}

// 将数字解封装得到更大的类型的数字
// Utility class to unwrap a number to a larger type. The numbers will never be
// unwrapped to a negative value.
template <typename U>
class Unwrapper {
  static_assert(!std::numeric_limits<U>::is_signed, "U must be unsigned");
  static_assert(std::numeric_limits<U>::max() <=
                    std::numeric_limits<uint32_t>::max(),
                "U must not be wider than 32 bits");

 public:
  // Get the unwrapped value, but don't update the internal state.
  int64_t UnwrapWithoutUpdate(U value) const {
    if (!last_value_)
      return value;

    constexpr int64_t kMaxPlusOne =
        static_cast<int64_t>(std::numeric_limits<U>::max()) + 1;

    // value 截断值
    // last_value_ 大类型值
    // cropped_last U 类型截断值
    U cropped_last = static_cast<U>(last_value_);
    // U cropped_last = last_value_ & 0xFFFF;
    // cout << "cropped_last=" << cropped_last << endl;
    int64_t delta = value - cropped_last;
    // 用截断值做比较
    if (IsNewer(value, cropped_last)) {
      if (delta < 0) {
        delta += kMaxPlusOne;  // Wrap forwards. // 向前回绕, 新的数字但是delta小于0说明这是向前回绕 delta+kMaxPlusOne
        // cout << "处理向前回绕" << endl;
      }
    } else if (delta > 0 && (last_value_ + delta - kMaxPlusOne) >= 0) {
      // If value is older but delta is positive, this is a backwards
      // wrap-around. However, don't wrap backwards past 0 (unwrapped).
      delta -= kMaxPlusOne; // 向后回绕,旧的数据而且delta为正数, delta-kMaxPlusOne
      // cout << "处理向后回绕" << endl;
    }

    return last_value_ + delta;
  }

  // Only update the internal state to the specified last (unwrapped) value.
  void UpdateLast(int64_t last_value) { last_value_ = last_value; }

  // Unwrap the value and update the internal state.
  int64_t Unwrap(U value) {
    int64_t unwrapped = UnwrapWithoutUpdate(value);
    UpdateLast(unwrapped);
    return unwrapped;
  }

 private:
  // absl::optional<int64_t> last_value_;
  int64_t last_value_ = 0;
};

using SequenceNumberUnwrapper = Unwrapper<uint16_t>;
using TimestampUnwrapper = Unwrapper<uint32_t>;

} // namespace webrtc

#endif // _MODULE_COMMON_TYPES_PUBLIC_H


