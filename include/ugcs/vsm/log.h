// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file log.h
 *
 * Logging functionality for VSM.
 */

#ifndef LOG_H_
#define LOG_H_

#include <ugcs/vsm/exception.h>

#include <stdlib.h>

#include <cstdio>
#include <cstdarg>
#include <string>
#include <memory>
#include <mutex>
#include <cinttypes>

namespace ugcs {
namespace vsm {

/** Class for handling log output. A singleton object of this class is created
 * when the first log output occurs. Log output is written to a custom log file
 * (if set by the Set_custom_log method) and to the platform specific logging
 * facilities (if present and implemented).
 */
class Log {

public:

    /** Logging related exception. */
    VSM_DEFINE_EXCEPTION(Exception);

    /** Available log levels. */
    enum class Level {
        /** Most verbose debug level. Should be used only for debug information
         * output.
         */
        DEBUGGING,
        /** Informational messages. */
        INFO,
        /** Warning messages which should be noticed by an operator. The error
         * condition should be recoverable.
         */
        WARNING,
        /** Non-recoverable errors which should be noticed by an operator. */
        ERROR,

        /** Maximal level code. */
        MAX = ERROR
    };

    /** Platform-specific logging handler. */
    class Platform_logger {
    public:
        /** Create unique instance. */
        static std::unique_ptr<Platform_logger>
        Create(Log &log);

        /** Construct platform logger to work with abstract logger. */
        Platform_logger(Log &log):
            log(log)
        {}

        virtual
        ~Platform_logger()
        {}

        /** Write formatted message. */
        virtual void
        Write_message(Level level, const char *msg, std::va_list args)
            __FORMAT(printf, 3, 0) = 0;

    protected:

        /** Reference to abstract logger. */
        Log &log;
    };

    /** Constructor. */
    Log();

    /** Destructor. */
    ~Log();

    /** Convert level value to readable string. */
    static const char *
    Get_level_str(Level level);

    /** Get base name (without leading directories) of file path. */
    static std::string
    Get_basename(const char *path);

    /** Platform-dependent convenience method to get descriptive string for
     * last system error.
     */
    static std::string
    Get_system_error();

    /** Write formatted message to the log.
     *
     * @param level Log level.
     * @param msg Message format similar to format used in printf() function.
     */
    static void
    Write_message(Level level, const char *msg, ...) __FORMAT(printf, 2, 3);

    /** Write formatted message to the log.
     *
     * @param level Log level.
     * @param msg Message format similar to format used in printf() function.
     * @param args Variable arguments list for format string.
     */
    static void
    Write_message_v(Level level, const char *msg, std::va_list args) __FORMAT(printf, 2, 0);

    /** Write formatted message to the log.
     *
     * @param level Log level.
     * @param msg Message format similar to format used in printf() function.
     */
    void
    Write_message_inst(Level level, const char *msg, ...) __FORMAT(printf, 3, 4);

    /** Write formatted message to the log.
     *
     * @param level Log level.
     * @param msg Message format similar to format used in printf() function.
     * @param args Variable arguments list for format string.
     */
    void
    Write_message_v_inst(Level level, const char *msg, std::va_list args) __FORMAT(printf, 3, 0);

    /** Set current log level for the application.
     * @param level New global log level.
     */
    static void
    Set_level(Level level)
    {
        /* No need for separate lock because concurrent modification in any case
         * will give unpredictable result.
         */
        Get_instance()->cur_level = level;
    }

    /** Set current log level.
     *
     * @param level Level symbolic name. Valid names: error, warning, info, debug.
     * @throws Invalid_param_exception if invalid name specified.
     */
    static void
    Set_level(const std::string &level);

    /** Set log file path for a custom (i.e. non system standard) logging.
     * Custom logging is disabled until file name is set.
     *
     * @param log_file Full log file path.
     * @throw Log::Exception if log file could not be opened/created for appending.
     */
    static void
    Set_custom_log(const std::string &log_file)
    {
        Get_instance()->Set_custom_log_inst(log_file);
    }

    /** Set custom file path for the instance itself, behaves like Set_custom_log. */
    void
    Set_custom_log_inst(const std::string &log_file);

    /** Set maximum size of a single custom log file. When this size is reached,
     * current log file is closed, re-named by appending a time stamp and a new
     * custom log file is opened.
     *
     * @param size_str Human readable size, which is a number postfixed with
     * a case insensitive multiplier:
     *  - Gb, G, Gbyte, Gbytes - for Giga-bytes;
     *  - Mb, M, Mbyte, Mbytes - for Mega-bytes;
     *  - Kb, K, Kbyte, Kbytes - for Kilo-bytes;
     *  - no postfix - for bytes;
     *  @throw Invalid_param_exception exception if multiplier is not recognized.
     */
    static void
    Set_max_custom_log_size(const std::string& size_str);

    /** Set maximum number of log files to keep.
     * Older log files will be deleted.
     *
     * @param count Number of old log files to keep. Default is 1.
     */
    static void
    Set_max_custom_log_count(size_t count);

    /** The same as Set_max_custom_log_size, but for the instance.
     *
     * @param size Max size in bytes.
     */
    void
    Set_max_custom_log_size_inst(ssize_t size);

    /** @param size Max number of log files to keep.
    */
    void
    Set_max_custom_log_count_inst(size_t count);

private:
    friend class Platform_logger;

    /** Minimum size of custom log file. */
    constexpr static ssize_t MIN_CUSTOM_LOG_FILE_SIZE = 16384;

    /** Default maximum size of custom log file. */
    constexpr static ssize_t DEFAULT_MAX_CUSTOM_LOG_FILE_SIZE = 100 * 1024 * 1024;

    /** Format to rename old log files when rotating. */
    constexpr static const char* const LOG_FILE_ROTATOR_SUFFIX_FORMAT = "_%Y%m%d-%H%M%S";

    /** Pattern used to fing old log files for deletion. This must match the suffix format above.
     * It is a fixed format date plus optional " (idx)"
     * Escaped ? because of trigraphs.*/
    constexpr static const char* const LOG_FILE_ROTATOR_FIND_PATTERN  = "_\?\?\?\?\?\?\?\?-\?\?\?\?\?\?*";

    /** Current log level, messages with lower level will be suppressed. */
    Level cur_level =
#   ifdef DEBUG
        Level::DEBUGGING;
#   else /* DEBUG */
        Level::INFO;
#   endif /* DEBUG */

    /** Protection for log writing operations. */
    std::mutex mutex;
    /** Global singleton, created on the first access. */
    static volatile Log *log;
    /** Duplicate output also to console. */
    bool use_console = true;
    /** Platform-specific logger. */
    std::unique_ptr<Platform_logger> plat_logger;
    /** Custom log file name (as passed to Set_custom_log()). */
    std::string custom_log_file_name;
    /** Log file for custom logging. */
    FILE* custom_log_file = nullptr;
    /** Max single custom log file size. */
    ssize_t max_custom_log = DEFAULT_MAX_CUSTOM_LOG_FILE_SIZE;
    /** Current size of custom log file. */
    ssize_t custom_log_size;

    /** How many old log files to keep. */
    size_t custom_log_count = 1;

    /** Get singleton instance of the log object. */
    static Log *
    Get_instance();

    /** Reopen custom log file. */
    bool
    Reopen_custom_log_file(const std::string &log_file);

    /** Write formatted message to the custom log file. */
    void
    Write_custom_message(int thread_id, Log::Level level, const char *msg,
            std::va_list args) __FORMAT(printf, 4, 0);

    /** Write formatted message to the console.
     *
     * @param thread_id Thread id which produces the message.
     * @param level Log level.
     * @param msg Message format similar to format used in printf() function.
     */
    void
    Write_console_message_inst(int thread_id, Level level, const char *msg, ...) __FORMAT(printf, 4, 5);

    /** Write formatted message to the console.
     *
     * @param thread_id Thread id which produces the message.
     * @param level Log level.
     * @param msg Message format similar to format used in printf() function.
     * @param args Variable arguments list for format string.
     */
    void
    Write_console_message_v_inst(int thread_id, Level level, const char *msg,
            std::va_list args) __FORMAT(printf, 4, 0);

    /**
     * Platform independent vprintf() for UTF-8 strings.
     * @param format printf() style format string in UTF-8.
     * @param args Arguments for format string.
     * @return The same as vprintf().
     */
    static int
    Vprintf_utf8(const char *format, std::va_list args) __FORMAT(printf, 1, 0);

    /**
     * Platform independent printf() for UTF-8 strings.
     * @param format printf() style format string in UTF-8.
     * @return The same as printf().
     */
    static int
    Printf_utf8(const char *format, ...) __FORMAT(printf, 1, 2);

    /** Checks, whether current custom log file cleanup procedure is needed. */
    bool
    Is_cleanup_needed();

    /** Do custom log file cleanup. */
    void
    Do_cleanup(int thread_id);

    /** Log file rotator. Removes old logs. */
    void
    Remove_old_log_files();
};

/** Write message to log. For internal usage only. */
#define _LOG_WRITE_MSG(level, msg, ...) \
    ::ugcs::vsm::Log::Write_message(level, "[%s:%d] " msg, \
                            ::ugcs::vsm::Log::Get_basename(__FILE__).c_str(), \
                            __LINE__, ## __VA_ARGS__)

/** Write debug message to the application log. */
#define LOG_DEBUG(msg, ...) \
    _LOG_WRITE_MSG(::ugcs::vsm::Log::Level::DEBUGGING, msg, ## __VA_ARGS__)

/** Short name. */
#define LOG_DBG     LOG_DEBUG
/** Short name. */
#define LOG         LOG_DEBUG

/** Write informational message to the application log. */
#define LOG_INFO(msg, ...) \
    _LOG_WRITE_MSG(::ugcs::vsm::Log::Level::INFO, msg, ## __VA_ARGS__)

/** Write warning message to the application log. */
#define LOG_WARNING(msg, ...) \
    _LOG_WRITE_MSG(::ugcs::vsm::Log::Level::WARNING, msg, ## __VA_ARGS__)

/** Short name. */
#define LOG_WARN    LOG_WARNING

/** Write error message to the application log. */
#define LOG_ERROR(msg, ...) \
    _LOG_WRITE_MSG(::ugcs::vsm::Log::Level::ERROR, msg, ## __VA_ARGS__)

/** Short name. */
#define LOG_ERR     LOG_ERROR

} /* namespace vsm */
} /* namespace ugcs */

#endif /* LOG_H_ */
