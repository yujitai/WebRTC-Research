/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file random.h
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/03/13
* @brief 
*****************************************************************/


#ifndef _RANDOM_H
#define _RANDOM_H

#include <stdint.h>

#include <limits>

namespace webrtc {

class Random {
 public:
  // TODO(tommi): Change this so that the seed can be initialized internally,
  // e.g. by offering two ways of constructing or offer a static method that
  // returns a seed that's suitable for initialization.
  // The problem now is that callers are calling clock_->TimeInMicroseconds()
  // which calls TickTime::Now().Ticks(), which can return a very low value on
  // Mac and can result in a seed of 0 after conversion to microseconds.
  // Besides the quality of the random seed being poor, this also requires
  // the client to take on extra dependencies to generate a seed.
  // If we go for a static seed generator in Random, we can use something from
  // webrtc/rtc_base and make sure that it works the same way across platforms.
  // See also discussion here: https://codereview.webrtc.org/1623543002/
  explicit Random(uint64_t seed);

  // Return pseudo-random integer of the specified type.
  // We need to limit the size to 32 bits to keep the output close to uniform.
  template <typename T>
  T Rand() {
    /*static_assert(std::numeric_limits<T>::is_integer &&
                      std::numeric_limits<T>::radix == 2 &&
                      std::numeric_limits<T>::digits <= 32,
                  "Rand is only supported for built-in integer types that are "
                  "32 bits or smaller.");*/
    return static_cast<T>(NextOutput());
  }

  // Uniformly distributed pseudo-random number in the interval [0, t].
  uint32_t Rand(uint32_t t);

  // Uniformly distributed pseudo-random number in the interval [low, high].
  uint32_t Rand(uint32_t low, uint32_t high);

  // Uniformly distributed pseudo-random number in the interval [low, high].
  int32_t Rand(int32_t low, int32_t high);

  // Normal Distribution.
  double Gaussian(double mean, double standard_deviation);

  // Exponential Distribution.
  double Exponential(double lambda);

 private:
  // Outputs a nonzero 64-bit random number.
  uint64_t NextOutput() {
    state_ ^= state_ >> 12;
    state_ ^= state_ << 25;
    state_ ^= state_ >> 27;
    // RTC_DCHECK(state_ != 0x0ULL);
    return state_ * 2685821657736338717ull;
  }

  uint64_t state_;

  // RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(Random);
};

// Return pseudo-random number in the interval [0.0, 1.0).
template <>
float Random::Rand<float>();

// Return pseudo-random number in the interval [0.0, 1.0).
template <>
double Random::Rand<double>();

// Return pseudo-random boolean value.
template <>
bool Random::Rand<bool>();

} // namespace webrtc

#endif // _RANDOM_H


