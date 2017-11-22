// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.
#include <ugcs/vsm/text_stream_filter.h>
#include <UnitTest++.h>

using namespace ugcs::vsm;

class Tester {
public:

    void
    Run(bool disable_in_handler)
    {
        this->disable_in_handler = disable_in_handler;
        File_processor::Ptr fp = File_processor::Create();
        fp->Enable();
        auto worker = Request_worker::Create("Text stream filter text worker");
        worker->Enable();

        auto stream = fp->Open("test_test_stream_stream.tmp", "w+");
        auto buffer = Io_buffer::Create("crap 1 dfg \n dfgdgf 2    \n dfeeee 3 ffff\n4 \n5\n");
        stream->Write(buffer);
        stream->Seek(0);
        filter = Text_stream_filter::Create(stream, worker);
        filter->Add_entry(
                regex::regex("^(.*)([:digit:])"),
                ugcs::vsm::Text_stream_filter::Make_match_handler(
                        &Tester::Handler, this));
        filter->Enable();

        while (!done) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!disable_in_handler) {
            filter->Disable();
        }
        stream->Close();
        fp->Disable();
        worker->Disable();
    }

    bool Handler(
            regex::smatch* match,
            Text_stream_filter::Lines_list*,
            Io_result result)
    {
        if (result == Io_result::OK) {
            count++;
            LOG_DEBUG("Matched: '%s'", (*match)[2].str().c_str());
            CHECK_EQUAL(count, std::stoi((*match)[2].str()));
            if (count > 2 && disable_in_handler) {
                filter->Disable();
                done = true;
                return false;
            } else {
                return true;
            }
        } else {
            done = true;
            return false;
        }
    }

    bool disable_in_handler;

    bool done = false;

    int count = 0;

    Text_stream_filter::Ptr filter;
};

/* Test for situation, when filter is disabled from the handler while there
 * are still more data exist in the filter buffer. This data should be just
 * dropped.
 */
TEST_FIXTURE(Tester, disable_in_handler)
{
    Run(true);
}

TEST_FIXTURE(Tester, disable_outside_handler)
{
    Run(false);
}
