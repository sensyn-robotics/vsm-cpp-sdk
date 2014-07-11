// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file text_stream_filter.h
 */
#ifndef TEXT_STREAM_FILTER_H_
#define TEXT_STREAM_FILTER_H_

#include <ugcs/vsm/vsm.h>

#include <ugcs/vsm/regex.h>

namespace ugcs {
namespace vsm {

/** Class for convenient filtering of a text stream using regular expressions. */
class Text_stream_filter: public std::enable_shared_from_this<Text_stream_filter> {
    DEFINE_COMMON_CLASS(Text_stream_filter, Text_stream_filter)
public:
    /** Maximal number of lines saved before the matched line. */
    constexpr static size_t MAX_HISTORY_LINES = 10;
    /** Maximal number of characters in one line which is supported. */
    constexpr static size_t MAX_LINE_LENGTH = 512;
    /** Maximal number of bytes to read at once. */
    constexpr static size_t MAX_READ = 64;
    /** List of accumulated lines. */
    typedef std::vector<std::string> Lines_list;
    /** Match handler called when entry matched the input line.
     * @return true to perceive the handler in the filter, false to remove it.
     *      Timeout (if any) is restarted when "true" is returned. It is allowed
     *      to return true only for OK and TIMED_OUT result codes.
     */
    typedef Callback_proxy<bool, regex::smatch *, Lines_list *, Io_result> Match_handler;
    /** Received line handler.
     * @return true if line is captured by the handler and should not be fed
     *      to filter entries.
     */
    typedef Callback_proxy<bool, const std::string *> Line_handler;
    /** Manipulation handle for an entry. */
    typedef unsigned long Entry_handle;

    /** Builder for match handlers. */
    DEFINE_CALLBACK_BUILDER(Make_match_handler,
                            (regex::smatch *, Text_stream_filter::Lines_list *,  Io_result),
                            (nullptr, nullptr, Io_result::OK));

    /** Builder for line handler. */
    DEFINE_CALLBACK_BUILDER(Make_line_handler, (const std::string *), (nullptr));

    /** Construct filter bound to a stream. */
    Text_stream_filter(Io_stream::Ref stream,
                       Request_completion_context::Ptr comp_ctx,
                       size_t max_read = MAX_READ);

    /** Enable the filter. */
    void
    Enable();

    /** Disable the filter. */
    void
    Disable(bool close_stream = true);

    /** Add matching entry to the filter. Should not be called, when related
     * stream is already closed.
     *
     * @param re Regular expression which is matched against each line. Handler
     *      is fired when match is found and enough context lines received.
     * @param handler Handler to invoke. In case of timeout it is fired with
     *      result argument equal to Io_result::TIMED_OUT. In this case
     *      lines list and match object can contain some data if timeout
     *      occurred when waiting for required succeeding context lines.
     * @param timeout Timeout value. Zero if no timeout needed.
     * @param ctx_lines_before Number of context lines to capture before the
     *      matching line. Cannot be greater than MAX_HISTORY_LINES.
     * @param ctx_lines_after Number of context lines to capture after the
     *      matching lines.
     * @return Entry handle.
     */
    Entry_handle
    Add_entry(const regex::regex &re, Match_handler handler,
              std::chrono::milliseconds timeout = std::chrono::milliseconds::zero(),
              size_t ctx_lines_before = 0, size_t ctx_lines_after = 0);

    /** Set handler for pre-filtering all received lines. */
    void
    Set_line_handler(Line_handler handler);

private:
    Io_stream::Ref stream;
    Request_completion_context::Ptr comp_ctx;

    /** Represents user match entry. */
    class Entry {
    public:
        Entry_handle handle;
        /** Regular expression for this entry. */
        regex::regex re;
        /** User provided handler. */
        Match_handler handler;
        /** Matched data when match found. */
        regex::smatch match;
        /** Matched lines. */
        Lines_list lines;
        /** Number of lines to capture before and after matched line. */
        size_t ctx_lines_before, ctx_lines_after;
        /** Timeout, zero if not specified. */
        std::chrono::milliseconds timeout = std::chrono::milliseconds::zero();
        /** Timeout timer handle if timeout required. */
        Timer_processor::Timer::Ptr timer = nullptr;

        Entry(Entry_handle handle, const regex::regex &re, Match_handler handler,
              size_t ctx_lines_before, size_t ctx_lines_after):
            handle(handle), re(re), handler(handler),
            ctx_lines_before(ctx_lines_before), ctx_lines_after(ctx_lines_after)
        {}

        ~Entry();
    };

    /** Line buffer. */
    std::string line_buf;
    /** History lines. */
    std::list<std::string> lines_history;
    /** Handle value for a new entry. */
    Entry_handle cur_handle = 1;
    /** Active entries. */
    std::map<Entry_handle, Entry> entries;
    /** CR character received, expecting line termination. */
    bool cr_received = false;
    /** Currently matched entry if receiving succeeding context lines. */
    Entry *cur_entry = nullptr;
    /** Number of succeeding lines to received after currently matched entry. */
    size_t ctx_lines_after;
    /** Active line handler if any. */
    Line_handler line_handler;
    /** Last read operation. */
    Operation_waiter read_op_waiter;
    /** true if read operation is scheduled. */
    bool read_is_scheduled = false;
    /** Maximum size to read at once. */
    size_t max_read;

    bool
    Timeout_handler(Entry *e);

    /** Incoming raw data handler. */
    void
    On_data_received(Io_buffer::Ptr buf, Io_result result);

    /** Character received. */
    void
    On_char_received(int c);

    /** Complete line received. */
    void
    On_line_received(const std::string &line);

    /** Schedule next read operation for the input stream. */
    void
    Schedule_read();

    void
    Reset_state(Io_result);

    void
    Fire_entry(Entry &e, bool timed_out = false);

    /** Find entry which matches the line. Return nullptr if not found. */
    Entry *
    Match_entry(const std::string &line);
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* TEXT_STREAM_FILTER_H_ */
