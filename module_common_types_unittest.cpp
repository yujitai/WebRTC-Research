/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file module_common_types_unittest.cpp
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/04/24
* @brief 
*****************************************************************/


#include "module_common_types_public.h"
using namespace webrtc;

void TestSequenceNumberUnwrapper01() {
    int64_t seq = 0;
    SequenceNumberUnwrapper unwrapper;

    const int kMaxIncrease = 0x8000 - 1;
    // const int kNumWraps = 4;
    const int kNumWraps = 2;
#if 1
    for (int i = 0; i <= kNumWraps * 2; ++i) {
        int64_t unwrapped = unwrapper.Unwrap(static_cast<uint16_t>(seq & 0xFFFF));
        cout << "seq=" << (seq & 0xFFFF) << " unwrapped=" << unwrapped << endl;
        seq += kMaxIncrease;
    }
#endif

#if 0
    unwrapper.UpdateLast(0);
    for (int seq = 0; seq < kNumWraps * 0xFFFF; ++seq) {
        int64_t unwrapped = unwrapper.Unwrap(static_cast<uint16_t>(seq & 0xFFFF));
        cout << "seq=" << (seq & 0xFFFF) << " unwrapped=" << unwrapped << endl;
    }
#endif
}

void TestSequenceNumberUnwrapper02() {
    SequenceNumberUnwrapper unwrapper;

    const int kMaxDecrease = 0x8000 - 1;
    // const int kNumWraps = 4;
    const int kNumWraps = 2;
    int64_t seq = kNumWraps * 2 * kMaxDecrease;

#if 1
    unwrapper.UpdateLast(seq);
    for (int i = kNumWraps * 2; i >= 0; --i) {
        int64_t unwrapped = unwrapper.Unwrap(static_cast<uint16_t>(seq & 0xFFFF));
        cout << "seq=" << (seq & 0xFFFF) << " unwrapped=" << unwrapped << endl;
        seq -= kMaxDecrease;
    }
#endif

#if 0
    seq = kNumWraps * 0xFFFF;
    unwrapper.UpdateLast(seq);
    for (; seq >= 0; --seq) {
        int64_t unwrapped = unwrapper.Unwrap(static_cast<uint16_t>(seq & 0xFFFF));
        cout << "seq=" << (seq & 0xFFFF) << " unwrapped=" << unwrapped << endl;
    }
#endif
}


int main() {
    // 测试用例
#if 1
    cout << "=====Test Equal=====" << endl;
    cout << "result=" << IsNewerSequenceNumber(0x0001, 0x0001) << endl;

    cout << "=====Test NoWrap=====" << endl;
    cout << "result=" << IsNewerSequenceNumber(0xFFFF, 0xFFFE) << endl;
    cout << "result=" << IsNewerSequenceNumber(0x0001, 0x0000) << endl; 
    cout << "result=" << IsNewerSequenceNumber(0x0100, 0x00FF) << endl; 

    // 向前回绕认为是新的数字
    // 新的数字远小于前一个数字
    cout << "=====Test ForwardWrap=====" << endl;
    cout << "result=" << IsNewerSequenceNumber(0x0000, 0xFFFF) << endl; 
    cout << "result=" << IsNewerSequenceNumber(0x0000, 0xFF00) << endl;
    cout << "result=" << IsNewerSequenceNumber(0x00FF, 0xFFFF) << endl;
    cout << "result=" << IsNewerSequenceNumber(0x00FF, 0xFF00) << endl;

    // 向后回绕认为不是新的数字
    // 新的数字远大于前一个数字
    cout << "=====Test BackwardWrap=====" << endl;
    cout << "result=" << IsNewerSequenceNumber(0xFFFF, 0x0000) << endl;
    cout << "result=" << IsNewerSequenceNumber(0xFF00, 0x0000) << endl;
    cout << "result=" << IsNewerSequenceNumber(0xFFFF, 0x00FF) << endl;
    cout << "result=" << IsNewerSequenceNumber(0xFF00, 0x00FF) << endl;

    cout << "=====Test HalfWayApart=====" << endl;
    cout << "result=" << IsNewerSequenceNumber(0x8000, 0x0000) << endl;
    cout << "result=" << IsNewerSequenceNumber(0x0000, 0x8000) << endl;
#endif

/*
    // NoWrap
    cout << webrtc::LatestSequenceNumber(0xFFFF, 0xFFFE) << endl;
    cout << webrtc::LatestSequenceNumber(0x0001, 0x0000) << endl;
    cout << webrtc::LatestSequenceNumber(0x0100, 0x00FF) << endl;

    cout << webrtc::LatestSequenceNumber(0xFFFE, 0xFFFF) << endl;
    cout << webrtc::LatestSequenceNumber(0x0000, 0x0001) << endl;
    cout << webrtc::LatestSequenceNumber(0x00FF, 0x0100) << endl;
*/

    // 测试用例
#if 0
    webrtc::SequenceNumberUnwrapper unwrapper;
    cout << unwrapper.Unwrap(0) << endl;
    cout << unwrapper.Unwrap(0x8000) << endl;
    cout << unwrapper.Unwrap(0) << endl; // 向后回绕

    cout << unwrapper.Unwrap(0x8000) << endl;
    cout << unwrapper.Unwrap(0xFFFF) << endl;
    cout << unwrapper.Unwrap(0) << endl; // 向前回绕
    cout << unwrapper.Unwrap(0xFFFF) << endl;
    cout << unwrapper.Unwrap(0x8000) << endl;
    cout << unwrapper.Unwrap(0) << endl;

    cout << unwrapper.Unwrap(0xFFFF) << endl;
#endif

    cout << "=====ForwardWraps=====" << endl;
    // TestSequenceNumberUnwrapper01();


    cout << "=====BackwardWraps=====" << endl;
    // TestSequenceNumberUnwrapper02();

    return 0;
}

