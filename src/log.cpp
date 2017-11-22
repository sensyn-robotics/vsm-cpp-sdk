// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Description:
 *   Log class partial implementation (platform independent).
 */

#include <ugcs/vsm/log.h>
#include <ugcs/vsm/platform.h>
#include <ugcs/vsm/file_processor.h>
#include <algorithm>
#include <locale>
#include <set>
#include <sstream>
#include <iostream>

using namespace ugcs::vsm;

volatile Log *Log::log = nullptr;

constexpr ssize_t Log::MIN_CUSTOM_LOG_FILE_SIZE;

constexpr ssize_t Log::DEFAULT_MAX_CUSTOM_LOG_FILE_SIZE;
constexpr const char* const Log::LOG_FILE_ROTATOR_SUFFIX_FORMAT;
constexpr const char* const Log::LOG_FILE_ROTATOR_FIND_PATTERN;

Log::Log()
{
    plat_logger = Platform_logger::Create(*this);
}

Log::~Log()
{
    if (custom_log_file) {
        std::fclose(custom_log_file);
        custom_log_file = nullptr;
    }
}

const char *
Log::Get_level_str(Level level)
{
    const char *levels[] = {
        "DBG",
        "INF",
        "WRN",
        "ERR"
    };
    static_assert(sizeof(levels) / sizeof(levels[0]) == static_cast<size_t>(Level::MAX) + 1,
                  "Names array size mismatch");
    return levels[static_cast<int>(level)];
}

void
Log::Set_level(const std::string &level)
{
    Log *log = Get_instance();
    if (level == "debug") {
        log->cur_level = Level::DEBUGGING;
    } else if (level == "info") {
        log->cur_level = Level::INFO;
    } else if (level == "warning") {
        log->cur_level = Level::WARNING;
    } else if (level == "error") {
        log->cur_level = Level::ERROR;
    } else {
        VSM_EXCEPTION(Invalid_param_exception, "Invalid log level name: %s",
                      level.c_str());
    }
}

void
Log::Set_custom_log_inst(const std::string &log_file)
{
    std::unique_lock<std::mutex> lock(mutex);
    if (log_file != custom_log_file_name &&
        custom_log_file_name.size()) {
        Do_cleanup(-1);
    }
    if (!Reopen_custom_log_file(log_file)) {
        lock.unlock();
        VSM_EXCEPTION(Exception, "Could not open log file [%s] for appending.",
                log_file.c_str());
    }
}

void
Log::Set_max_custom_log_size(const std::string& size_str)
{
    static const std::set<std::string> Gb = { "G", "GB", "GBYTE", "GBYTES" };
    static const std::set<std::string> Mb = { "M", "MB", "MBYTE", "MBYTES" };
    static const std::set<std::string> Kb = { "K", "KB", "KBYTE", "KBYTES" };

    std::istringstream input(size_str);
    int val = 0;
    input >> val;
    std::string mult;
    input >> mult;

    std::string mult_upper = mult;
    /* Default locale should be fine for ASCII conversion. */
    std::transform(mult.begin(), mult.end(), mult_upper.begin(),
            [](std::string::value_type chr) { return std::toupper(chr, std::locale()); });

    ssize_t size = val;
    if (Gb.count(mult_upper)) {
        size *= 1024 * 1024 * 1024;
    } else if (Mb.count(mult_upper)) {
        size *= 1024 * 1024;
    } else if (Kb.count(mult_upper)) {
        size *= 1024;
    } else if (mult_upper.size()) {
        VSM_EXCEPTION(Invalid_param_exception, "Unknown bytes size multiplier [%s].",
                mult.c_str());
    }
    Log::Get_instance()->Set_max_custom_log_size_inst(size);
}

void
Log::Set_max_custom_log_size_inst(ssize_t size)
{
    if (size < MIN_CUSTOM_LOG_FILE_SIZE) {
        LOG_ERR("Too small value for maximum log file size (%ld bytes) specified, "
                "using default value of %ld bytes.",
                static_cast<long>(size),
                static_cast<long>(MIN_CUSTOM_LOG_FILE_SIZE));
        max_custom_log = MIN_CUSTOM_LOG_FILE_SIZE;
    } else {
        LOG_DEBUG("Setting max custom log file to %ld bytes", static_cast<long>(size));
        max_custom_log = size;
    }
}

void
Log::Set_max_custom_log_count(size_t count)
{
    Log::Get_instance()->Set_max_custom_log_count_inst(count);
}

void
Log::Set_max_custom_log_count_inst(size_t count)
{
    custom_log_count = count;
    LOG_DEBUG("Setting max old log file count to %zu", count);
}

std::string
Log::Get_basename(const char *path)
{
    std::string s(path);
    size_t sep_pos = s.rfind(PATH_SEPARATOR);
    if (sep_pos != std::string::npos) {
        s = s.substr(sep_pos + 1);
    }
    return s;
}

Log *
Log::Get_instance()
{
    static std::mutex critical_sect_mx;
    std::lock_guard<std::mutex> lock(critical_sect_mx);
    if (!log) {
        log = new Log();
    }
    return const_cast<Log *>(log);
}

bool
Log::Reopen_custom_log_file(const std::string &log_file)
{
    if (custom_log_file) {
        std::fclose(custom_log_file);
    }
    custom_log_file = File_processor::Fopen_utf8(log_file, "a");
    if (!custom_log_file) {
        Write_console_message_inst(
                -1,
                Level::ERROR,
                "Could not open custom log file [%s].",
                log_file.c_str());
        return false;
    }
    custom_log_file_name = log_file;
    /* fseek is necessary for Windows only, because ftell for a file
     * opened with append mode always returns zero and this is documented [bug]
     * on Microsoft site. Anyway, one extra call here does not hurt other OS.
     */
    if (std::fseek(custom_log_file, 0, SEEK_END)) {
        Write_console_message_inst(
                -1,
                Level::ERROR,
                "Could not seek to the end of custom log file [%s].",
                log_file.c_str());
        std::fclose(custom_log_file);
        custom_log_file = nullptr;
        return false;
    }
    custom_log_size = std::ftell(custom_log_file);
    if (custom_log_size < 0) {
        Write_console_message_inst(
                -1,
                Level::ERROR,
                "Could not determine the size of custom log file [%s].",
                log_file.c_str());
        std::fclose(custom_log_file);
        custom_log_file = nullptr;
        return false;
    }
    return true;
}

void
Log::Write_message(Level level, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    Write_message_v(level, msg, args);
    va_end(args);
}

void
Log::Write_message_v(Level level, const char *msg, std::va_list args)
{
    Get_instance()->Write_message_v_inst(level, msg, args);
}

void
Log::Write_message_inst(Level level, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    Write_message_v_inst(level, msg, args);
    va_end(args);
}

void
Log::Write_message_v_inst(Level level, const char *msg, std::va_list args)
{
    static int thread_counter(1);

    if (level < cur_level) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex);
    static thread_local int thread_id(thread_counter++);

    /* Console output is platform-independent so it can be done here. */
    if (use_console) {
        Write_console_message_v_inst(thread_id, level, msg, args);
    }

    Write_custom_message(thread_id, level, msg, args);

    /* Output to platform-dependent system log. */
    plat_logger->Write_message(level, msg, args);
}

void
Log::Write_custom_message(int thread_id, Log::Level level, const char *msg,
        std::va_list args)
{
    if (!custom_log_file) {
        return;
    }

    std::va_list cust_args;
    /* Note va_end! */
    va_copy(cust_args, args);

    char ts_buf[128];
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::strftime(ts_buf, sizeof(ts_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));

    auto duration = now.time_since_epoch();
    uint64_t ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000;

    for (;;) {
        ssize_t res = std::fprintf(custom_log_file, "%s.%03u - <%s> %d ",
                ts_buf,
                static_cast<uint32_t>(ms), /* Remainder of 1000. */
                Log::Get_level_str(level),
                thread_id);
        if (res <= 0) {
            break;
        }
        custom_log_size += res;
        res = std::vfprintf(custom_log_file, msg, cust_args);
        if (res < 0) {
            break;
        }
        custom_log_size += res;
        res = std::fprintf(custom_log_file, "\n");
        if (res <= 0) {
            break;
        }
        custom_log_size += res;
        std::fflush(custom_log_file);
        /* Written OK. */
        if (Is_cleanup_needed()) {
            Do_cleanup(thread_id);
        }
        va_end(cust_args);
        return;
    }

    va_end(cust_args);

    /*
     * Some error happened during custom log file writing. Duplicate the
     * message to the console.
     */
    Write_console_message_inst(
            thread_id,
            Level::ERROR,
            "Custom log file writing error, next message is forced to console.");
    Write_console_message_v_inst(thread_id, level, msg, args);
}

void
Log::Write_console_message_inst(int thread_id, Level level, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    Write_console_message_v_inst(thread_id, level, msg, args);
    va_end(args);
}

void
Log::Write_console_message_v_inst(int thread_id, Level level, const char *msg,
        std::va_list args)
{
    char ts_buf[128];
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::strftime(ts_buf, sizeof(ts_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));

    auto duration = now.time_since_epoch();
    unsigned long ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000;

    std::va_list con_args;
    va_copy(con_args, args);
    Printf_utf8("%s.%03lu - <%s> %d ", ts_buf, ms, Get_level_str(level), thread_id);
    Vprintf_utf8(msg, con_args);
    std::cout<<std::endl;
    fflush(stdout);
    va_end(con_args);
}

int
Log::Printf_utf8(const char *message, ...)
{
    std::va_list args;
    va_start(args, message);
    int ret = Vprintf_utf8(message, args);
    va_end(args);
    return ret;
}

bool
Log::Is_cleanup_needed()
{
    return custom_log_size > max_custom_log;
}

void
Log::Do_cleanup(int thread_id)
{
    fclose(custom_log_file);
    custom_log_file = nullptr;

    char ts_buf[128];
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    // If you change this pattern then you fix the Remove_old_log_files, too.
    std::strftime(ts_buf, sizeof(ts_buf), LOG_FILE_ROTATOR_SUFFIX_FORMAT, std::localtime(&t));

    std::string base_name = custom_log_file_name + ts_buf;

    bool failure = true;
    for (int idx = 0; idx < 100; idx++) {
        std::string check_name = base_name;
        if (idx) {
            check_name += "(" + std::to_string(idx) + ")";
        }
        /* Destination file should not exist. */
        FILE* dst = File_processor::Fopen_utf8(check_name, "r");
        if (dst != nullptr) {
            std::fclose(dst);
            continue;
        }
        /* Try rename. */
        if (!File_processor::Rename_utf8(custom_log_file_name, check_name)) {
            Write_console_message_inst(
                    thread_id,
                    Level::WARNING,
                    "Could not rename custom log file [%s] to [%s].",
                    custom_log_file_name.c_str(),
                    check_name.c_str());
            continue;
        }
        /* Done. */
        failure = false;
        break;
    }
    if (failure) {
        Write_console_message_inst(
                -1,
                Level::ERROR,
                "Cleanup failed for custom log file [%s], custom log disabled.",
                custom_log_file_name.c_str());
        /* Custom log file remains closed, so not used. */
    } else {
        /* Don't throw exception in the middle of work if reopen fails. */
        Reopen_custom_log_file(custom_log_file_name);
    }
    Remove_old_log_files();
}
