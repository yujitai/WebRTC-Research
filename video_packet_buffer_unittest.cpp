/*****************************************************************
* Copyright (C) 2020 Zuoyebang.com, Inc. All Rights Reserved.
* 
* @file video_packet_buffer_unittest.cpp
* @author yujitai(yujitai@zuoyebang.com)
* @date 2020/05/16
* @brief 
*****************************************************************/

// g++ video_packet_buffer_unittest.cpp -I googletest/mybuild/include/ -std=c++11 -I /home/yujitai/dev/zrtc/smt-server/libzybrtc/output/include -I /home/yujitai/dev/zrtc/voip/librtcbase/output/include -I /home/yujitai/dev/zrtc/smt-server/libzybrtc/output/include/zybrtc

#include <gtest/gtest.h>
#include <zybrtc/modules/video_coding/frame_object.h>
#include <zybrtc/modules/video_coding/packet_buffer.h>
#include <rtcbase/random.h>
#include <zybrtc/system_wrappers/include/clock.h>
#include <memory>
#include <iostream>
using namespace std;

using namespace zybrtc;
using namespace video_coding;
using namespace rtcbase;

enum IsKeyFrame { kKeyFrame, kDeltaFrame };
enum IsFirst { kFirst, kNotFirst };
enum IsLast { kLast, kNotLast };

class TestPacketBuffer : public ::testing::Test,
                         public zybrtc::video_coding::OnReceivedFrameCallback 
{
protected:
    explicit TestPacketBuffer()
        : _rand(0x7732213),
        _clock(new SimulatedClock(0)),
        _packet_buffer(PacketBuffer::create(_clock.get(), kStartSize, kMaxSize, this)) {}

    uint16_t Rand() { return _rand.rand<uint16_t>(); }

    void on_received_frame(std::unique_ptr<zybrtc::video_coding::RtpFrameObject> frame) override {
        uint16_t first_seq_num = frame->first_seq_num();
        if (_frames_from_callback.find(first_seq_num) !=
                _frames_from_callback.end()) {
            ADD_FAILURE() << "Already received frame with first sequence number "
              << first_seq_num << ".";
            return;
        }

        cout << "PacketBuffer 抛帧 frame_type=" << frame->frame_type() << endl;
        _frames_from_callback.insert(
                std::make_pair(frame->first_seq_num(), std::move(frame)));
    }

    bool Insert(uint16_t seq_num, IsKeyFrame keyframe, IsFirst first, IsLast last, int data_size = 0, uint8_t* data = nullptr, uint32_t timestamp = 123u) {
        zybrtc::VCMPacket packet;
        // packet.codec = k_video_codec_generic;
        packet.codec = k_video_codec_generic;
        packet.timestamp = timestamp;
        packet.seq_num = seq_num;
        packet.frame_type = keyframe == kKeyFrame ? k_video_frame_key : k_video_frame_delta;
        packet.is_first_packet_in_frame = first == kFirst;
        packet.marker_bit = last == kLast;
        packet.size_bytes = data_size;
        packet.data_ptr = data;

        return _packet_buffer->insert_packet(&packet);
    }

    void CheckFrame(uint16_t first_seq_num) {
        auto frame_it = _frames_from_callback.find(first_seq_num);
        ASSERT_FALSE(frame_it == _frames_from_callback.end())
            << "Could not find frame with first sequence number " << first_seq_num
            << ".";
    }

    void DeleteFrame(uint16_t first_seq_num) {
        auto frame_it = _frames_from_callback.find(first_seq_num);
        ASSERT_FALSE(frame_it == _frames_from_callback.end())
            << "Could not find frame with first sequence number " << first_seq_num
            << ".";
        _frames_from_callback.erase(frame_it);
    }


    static constexpr int kStartSize = 16;
    static constexpr int kMaxSize = 64;

    Random _rand;
    std::unique_ptr<SimulatedClock> _clock;
    std::shared_ptr<PacketBuffer> _packet_buffer;
    std::map<uint16_t, std::unique_ptr<RtpFrameObject>> _frames_from_callback;
};

#if 0
TEST_F(TestPacketBuffer, InsertOnePacket) {
    const uint16_t seq_num = Rand();
    EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
}

TEST_F(TestPacketBuffer, InsertMultiplePackets) {
    const uint16_t seq_num = Rand();
    EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
    EXPECT_TRUE(Insert(seq_num + 1, kKeyFrame, kFirst, kLast));
    EXPECT_TRUE(Insert(seq_num + 2, kKeyFrame, kFirst, kLast));
    EXPECT_TRUE(Insert(seq_num + 3, kKeyFrame, kFirst, kLast));
}

TEST_F(TestPacketBuffer, InsertDuplicatePacket) {
    const uint16_t seq_num = Rand();
    EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
    EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
}

TEST_F(TestPacketBuffer, SeqNumWrapOneFrame) {
  EXPECT_TRUE(Insert(0xFFFF, kKeyFrame, kFirst, kNotLast));
  EXPECT_TRUE(Insert(0x0, kKeyFrame, kNotFirst, kLast));

  CheckFrame(0xFFFF);
}

TEST_F(TestPacketBuffer, SeqNumWrapTwoFrames) {
    EXPECT_TRUE(Insert(0xFFFF, kKeyFrame, kFirst, kLast));
    EXPECT_TRUE(Insert(0x0, kKeyFrame, kFirst, kLast));

    CheckFrame(0xFFFF);
    CheckFrame(0x0);
}
#endif

#if 0
TEST_F(TestPacketBuffer, InsertOldPackets) {
    const uint16_t seq_num = Rand();

    EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
    EXPECT_TRUE(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast));
    EXPECT_TRUE(Insert(seq_num + 1, kKeyFrame, kNotFirst, kLast));
    ASSERT_EQ(2UL, _frames_from_callback.size());

    _frames_from_callback.erase(seq_num + 2);
    EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
    ASSERT_EQ(1UL, _frames_from_callback.size());

    _frames_from_callback.erase(_frames_from_callback.find(seq_num));
    ASSERT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
    EXPECT_TRUE(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast));

    _packet_buffer->clear_to(seq_num + 2);
    EXPECT_FALSE(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast));
    EXPECT_TRUE(Insert(seq_num + 3, kDeltaFrame, kFirst, kLast));
    ASSERT_EQ(2UL, _frames_from_callback.size());
}

TEST_F(TestPacketBuffer, NackCount) {
    const uint16_t seq_num = Rand();

    VCMPacket packet;
    packet.codec = k_video_codec_generic;
    packet.seq_num = seq_num;
    packet.frame_type = k_video_frame_key;
    packet.is_first_packet_in_frame = true;
    packet.marker_bit = false;
    packet.times_nacked = 0;

    _packet_buffer->insert_packet(&packet);

    packet.seq_num++;
    packet.is_first_packet_in_frame = false;
    packet.times_nacked = 1;
    _packet_buffer->insert_packet(&packet);

    packet.seq_num++;
    packet.times_nacked = 3;
    _packet_buffer->insert_packet(&packet);

    packet.seq_num++;
    packet.marker_bit = true;
    packet.times_nacked = 1;
    _packet_buffer->insert_packet(&packet);

    ASSERT_EQ(1UL, _frames_from_callback.size());
    RtpFrameObject* frame = _frames_from_callback.begin()->second.get();
    EXPECT_EQ(3, frame->times_nacked());
}

TEST_F(TestPacketBuffer, FrameSize) {
    const uint16_t seq_num = Rand();
    uint8_t* data1 = new uint8_t[5]();
    uint8_t* data2 = new uint8_t[5]();
    uint8_t* data3 = new uint8_t[5]();
    uint8_t* data4 = new uint8_t[5]();

    EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast, 5, data1));
    EXPECT_TRUE(Insert(seq_num + 1, kKeyFrame, kNotFirst, kNotLast, 5, data2));
    EXPECT_TRUE(Insert(seq_num + 2, kKeyFrame, kNotFirst, kNotLast, 5, data3));
    EXPECT_TRUE(Insert(seq_num + 3, kKeyFrame, kNotFirst, kLast, 5, data4));

    ASSERT_EQ(1UL, _frames_from_callback.size());
    EXPECT_EQ(20UL, _frames_from_callback.begin()->second->size());
}
#endif

#if 0
TEST_F(TestPacketBuffer, ThreePacketReorderingOneFrame) {
    const uint16_t seq_num = Rand();

    EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
    EXPECT_TRUE(Insert(seq_num + 2, kKeyFrame, kNotFirst, kLast));
    EXPECT_TRUE(Insert(seq_num + 1, kKeyFrame, kNotFirst, kNotLast));

    EXPECT_EQ(1UL, _frames_from_callback.size());
    CheckFrame(seq_num);
}
#endif

/*
TEST_F(TestPacketBuffer, yujitaitest) {
    const uint16_t seq_num = Rand();

    EXPECT_TRUE(Insert(seq_num, kDeltaFrame, kFirst, kNotLast));
    EXPECT_TRUE(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast));
    EXPECT_TRUE(Insert(seq_num + 3, kDeltaFrame, kFirst, kLast));

    EXPECT_EQ(2UL, _frames_from_callback.size());
}
*/

#if 0
// 帧乱序
TEST_F(TestPacketBuffer, FramesReordered) {
    const uint16_t seq_num = Rand();

    EXPECT_TRUE(Insert(seq_num + 1, kDeltaFrame, kFirst, kLast));
    // EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
    EXPECT_TRUE(Insert(seq_num + 3, kDeltaFrame, kFirst, kLast));
    EXPECT_TRUE(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast));
/*
    ASSERT_EQ(4UL, _frames_from_callback.size());
    CheckFrame(seq_num);
    CheckFrame(seq_num + 1);
    CheckFrame(seq_num + 2);
    CheckFrame(seq_num + 3);
    */
}
#endif

#if 1
class ReceivedFrameCallback : public zybrtc::video_coding::OnReceivedFrameCallback {
public:
    void on_received_frame(std::unique_ptr<zybrtc::video_coding::RtpFrameObject> frame) override {}
};

void test01() {
    const uint16_t seq_num = 1;

    ReceivedFrameCallback callback;
    std::shared_ptr<PacketBuffer> _packet_buffer = 
        PacketBuffer::create(new SimulatedClock(0), 16, 64, &callback);

    VCMPacket packet;
    // packet.codec = k_video_codec_generic;
    packet.codec = k_video_codec_h264;
    packet.seq_num = seq_num;
    packet.frame_type = k_video_frame_delta;
    packet.is_first_packet_in_frame = true;
    packet.marker_bit = true;
    _packet_buffer->insert_packet(&packet);

    packet.seq_num = seq_num+2;
    // packet.is_first_packet_in_frame = false;
    _packet_buffer->insert_packet(&packet);

    packet.seq_num = seq_num+1;
    _packet_buffer->insert_packet(&packet);
}
#endif

int main(int argc,char *argv[])
{
    test01();
    // testing::InitGoogleTest(&argc,argv);
    // return RUN_ALL_TESTS();
}

