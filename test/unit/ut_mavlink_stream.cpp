// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <UnitTest++.h>
#include <ugcs/vsm/vsm.h>

using namespace ugcs::vsm;

typedef mavlink::Mavlink_kind_standard Mavlink_kind;

void
On_heartbeat(mavlink::Message<mavlink::MESSAGE_ID::HEARTBEAT>::Ptr,
        Mavlink_stream<Mavlink_kind>::Ptr mav_stream)
{
    mav_stream->Disable();
}

TEST(disable_in_handler)
{
    File_processor::Ptr fp = File_processor::Create();
    fp->Enable();

    auto stream = fp->Open("test_mavlink_stream.tmp", "w+");

    Mavlink_stream<Mavlink_kind>::Ptr mav_stream =
            Mavlink_stream<Mavlink_kind>::Create(stream);
    mav_stream->Bind_decoder_demuxer();
    mavlink::Pld_heartbeat hb;
    auto msg1 = Mavlink_encoder().Encode<Mavlink_kind>(hb, 1, 1);
    auto msg2 = Mavlink_encoder().Encode<Mavlink_kind>(hb, 1, 1);
    auto msgs = msg1->Concatenate(msg2);
    mav_stream->Get_demuxer().Register_handler<mavlink::MESSAGE_ID::HEARTBEAT, mavlink::Extension>(
            Mavlink_demuxer::Make_handler<mavlink::MESSAGE_ID::HEARTBEAT, mavlink::Extension>(
                    On_heartbeat, mav_stream));
    CHECK(mav_stream->Get_stream());
    mav_stream->Get_decoder().Decode(msgs);
    /* First message disables the stream, so second should be ignored. */
    CHECK_EQUAL(1, mav_stream->Get_decoder().Get_stats().handled);
    CHECK(!mav_stream->Get_stream());

    stream->Close();
    fp->Disable();
}
