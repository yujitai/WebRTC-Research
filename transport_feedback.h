/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file transport_feedback.h
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/03/18
* @brief 
*****************************************************************/


#ifndef _TRANSPORT_FEEDBACK_H
#define _TRANSPORT_FEEDBACK_H

namespace webrtc {

namespace rtcp {

class TransportFeedback : public Rtpfb {
public:
    class ReceivedPacket {
        public:
            ReceivedPacket(uint16_t sequence_number, int16_t delta_ticks) 
                : _sequence_number(sequence_number), _delta_ticks(delta_ticks) {}
            ReceivedPacket(const ReceivedPacket&) = default;
            ReceivedPacket& operator=(const ReceivedPacket&) = default;

            uint16_t sequence_number() const { return _sequence_number; }
            int16_t delta_ticks() const { return _delta_ticks; }
            int32_t delta_us() const { return _delta_ticks * kDeltaScaleFactor; }

        private:
            uint16_t _sequence_number;
            int16_t _delta_ticks;
    };
};

} // namespace rtcp

} // namespace webrtc

#endif // _TRANSPORT_FEEDBACK_H


