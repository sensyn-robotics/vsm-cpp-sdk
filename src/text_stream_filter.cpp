// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <vsm/text_stream_filter.h>

using namespace vsm;

Text_stream_filter::Text_stream_filter(Io_stream::Ref stream,
                                       Request_completion_context::Ptr comp_ctx):
    stream(stream),
    comp_ctx(comp_ctx)
{
}

void
Text_stream_filter::Enable()
{
    Schedule_read();
}

void
Text_stream_filter::Disable(bool close_stream)
{
    if (close_stream) {
        stream->Close();
    }
    stream = nullptr;
    cur_entry = nullptr;
    comp_ctx = nullptr;
    read_op_waiter.Abort();
    read_op_waiter = Operation_waiter();
    read_is_scheduled = false;
    /* Cancel timers to avoid the execution of timeout handlers after
     * the filter is disabled.
     */
    for(auto& iter: entries) {
        if (iter.second.timer) {
            iter.second.timer->Cancel();
            iter.second.timer = nullptr;
        }
    }
}

void
Text_stream_filter::Schedule_read()
{
    if (!read_is_scheduled) {
        read_is_scheduled = true;
        read_op_waiter = stream->Read(1, 1,
                Make_read_callback(&Text_stream_filter::On_data_received,
                        Shared_from_this()),
                        comp_ctx);
    }
}

Text_stream_filter::Entry_handle
Text_stream_filter::Add_entry(const regex::regex &re, Match_handler handler,
                              std::chrono::milliseconds timeout,
                              size_t ctx_lines_before, size_t ctx_lines_after)
{
    Entry_handle handle = cur_handle++;
    Entry &e =
        entries.emplace(handle, Entry(handle, re, handler,
                                      ctx_lines_before,
                                      ctx_lines_after)).first->second;
    e.timeout = timeout;
    if (timeout.count()) {
        e.timer = Timer_processor::Get_instance()->Create_timer(
            timeout,
            Make_callback(&Text_stream_filter::Timeout_handler, this, &e),
            comp_ctx);
    }
    if (stream->Is_closed()) {
        /* To make sure, that stream close event is propagated to new entry. */
        Schedule_read();
    }
    return handle;
}

void
Text_stream_filter::Set_line_handler(Line_handler handler)
{
    line_handler = handler;
}

Text_stream_filter::Entry::~Entry()
{
    if (timer) {
        timer->Cancel();
        timer = nullptr;
    }
}

bool
Text_stream_filter::Timeout_handler(Entry *e)
{
    Fire_entry(*e, true);
    return false;
}

void
Text_stream_filter::Reset_state(Io_result result)
{
    cr_received = false;
    cur_entry = nullptr;
    lines_history.clear();
    line_buf.clear();
    for (auto &v: entries) {
        Entry &e = v.second;
        if (e.handler(nullptr, nullptr, result)) {
            VSM_EXCEPTION(Exception,
                          "Cannot re-schedule entry after unsuccessful operation");
        }
        ASSERT(entries.size());
    }
    entries.clear();
}

void
Text_stream_filter::On_data_received(Io_buffer::Ptr buf, Io_result result)
{
    read_is_scheduled = false;
    if (result == Io_result::OK) {
        std::string data = buf->Get_string();
        for (char c: data) {
            On_char_received(c);
        }
        Schedule_read();
    } else {
        Reset_state(Io_result::CLOSED);
        if (entries.size()) {
            /* New entries were added after stream is closed. Schedule read
             * to notify them about it.
             */
            Schedule_read();
        }
    }
}

void
Text_stream_filter::On_char_received(int c)
{
    if (c == '\r') {
        if (cr_received) {
            On_line_received(line_buf);
            line_buf.clear();
        } else {
            cr_received = true;
        }
    } else if (c == '\n') {
        cr_received = false;
        On_line_received(line_buf);
        line_buf.clear();
    } else if ((c < 0x20 && c != '\t' && c != '\f') ||
               c > 0x80) {
        /* Ignore non-ASCII and special characters except whitespaces. */
    } else {
        if (cr_received) {
            On_line_received(line_buf);
            line_buf.clear();
            cr_received = false;
        }
        line_buf += c;
        if (line_buf.size() > MAX_LINE_LENGTH) {
            /* Length exceeded, drop half of the buffer. */
            line_buf = line_buf.substr(MAX_LINE_LENGTH / 2);
        }
    }
}

void
Text_stream_filter::On_line_received(const std::string &line)
{
    if (line_handler && line_handler(&line)) {
        /* Captured by line handler. */
        return;
    }

    lines_history.emplace_back(line);
    if (lines_history.size() > MAX_HISTORY_LINES) {
        lines_history.pop_front();
    }

    if (cur_entry) {
        ASSERT(ctx_lines_after);
        cur_entry->lines.emplace_back(line);
        ctx_lines_after--;
        if (!ctx_lines_after) {
            Fire_entry(*cur_entry);
            cur_entry = nullptr;
        }
        return;
    }

    Entry *e = Match_entry(line);
    if (!e) {
        return;
    }
    /* Preceding context. */
    auto it = lines_history.end();
    it--;
    auto it_end = it;
    for (size_t i = 0; i < e->ctx_lines_before; i++) {
        it--;
    }
    for (; it != it_end; it++) {
        e->lines.emplace_back(*it);
    }
    /* Matched line. */
    e->lines.emplace_back(line);
    /* Succeeding context. */
    if (e->ctx_lines_after) {
        cur_entry = e;
        ctx_lines_after = e->ctx_lines_after;
        return;
    }
    Fire_entry(*e);
}

void
Text_stream_filter::Fire_entry(Entry &e, bool timed_out)
{
    if (e.timer) {
        if (!timed_out) {
            e.timer->Cancel();
        }
        e.timer = nullptr;
    }
    Io_result result =
        timed_out ? Io_result::TIMED_OUT : Io_result::OK;
    if (e.handler(&e.match, &e.lines, result)) {
        e.lines.clear();
        /* Restart timer if needed. */
        if (e.timeout.count()) {
            e.timer = Timer_processor::Get_instance()->Create_timer(
                e.timeout,
                Make_callback(&Text_stream_filter::Timeout_handler, this, &e),
                comp_ctx);
        }
    } else {
        if (cur_entry == &e) {
            cur_entry = nullptr;
        }
        entries.erase(e.handle);
    }
}

Text_stream_filter::Entry *
Text_stream_filter::Match_entry(const std::string &line)
{
    for (auto &value: entries) {
        Entry &e = value.second;
        /* Current line already in history buffer. */
        if (lines_history.size() < e.ctx_lines_before + 1) {
            continue;
        }
        if (regex::regex_match(line, e.match, e.re)) {
            return &e;
        }
    }
    return nullptr;
}
