// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file file_processor.h
 *
 * Processor for handling file I/O.
 */

#ifndef FILE_PROCESSOR_H_
#define FILE_PROCESSOR_H_

#include <ugcs/vsm/io_request.h>
#include <ugcs/vsm/request_worker.h>

#include <thread>

namespace ugcs {
namespace vsm {

/** Processor for working with filesystem I/O. It is mostly intended for working
 * with special files such as mapped devices in asynchronous mode. It is not so
 * useful for regular files so the user code can use \<cstdio\> functions for them
 * (unless it does not need asynchronous file operations).
 * Keep in mind that this processor provides asynchronous I/O but not concurrent.
 * All operations for each stream are serialized and never executed in parallel.
 * In case the client code issues concurrent requests it should be aware that
 * file position can be unpredictably updated unless it is explicitly specified.
 */
class File_processor: public Request_processor {
    DEFINE_COMMON_CLASS(File_processor, Request_container)
public:
    /** Base class for all File_processor exceptions. */
    VSM_DEFINE_EXCEPTION(Exception);
    /** File not found. Thrown when a specified filesystem path does not exist. */
    VSM_DEFINE_DERIVED_EXCEPTION(Exception, Not_found_exception);
    /** Permission denied. Thrown when there is insufficient permissions to
     * execute the requested action.
     */
    VSM_DEFINE_DERIVED_EXCEPTION(Exception, Permission_denied_exception);
    /** File already exists. Thrown when new file creation requested ('x'
     * specified in opening mode) but the file already exists.
     */
    VSM_DEFINE_DERIVED_EXCEPTION(Exception, Already_exists_exception);

    class Native_controller;

    /** Stream class which represents opened file. */
    class Stream: public Io_stream {
        DEFINE_COMMON_CLASS(Stream, Io_stream)
    public:
        /** Reference type. */
        typedef Reference_guard<Stream::Ptr> Ref;

        /** Lock operation result. */
        enum class Lock_result {
            /** Lock acquired. */
            OK,
            /** Lock owned by other stream. */
            BLOCKED,
            /** Error while locking. */
            ERROR
        };

        /** Mode for file opening. */
        class Mode {
        public:
            /** Read access. */
            bool read:1,

            /** Write access. File is truncated when opened. */
                 write:1,

            /** Extended access (corresponds to "+" in opening mode string.
             * File is not truncated when opened for write and also can be read.
             * Also can be written when opened for read.
             */
                 extended:1,

             /** File should not exist
              * 1) When used with write access:
              * If specified, opening fails if file is already existing.
              * If not specified, new file is created if it does not exist.
              * 2) When used with read access:
              * If specified, file is created if file does not exist.
              * If not specified, opening fails if file does not exist.
              * NOTE 'x' with 'r' combination is not allowed in standard
              * fopen() call. It is our extension to allow creation new
              * file while opening it in read-only mode. (Does not mean
              * that file on filesystem is read only.)
              */
             should_not_exist:1;

            /** Parse and validate mode string into binary structure.
             *
             * @param mode_str Mode string, format is similar to fopen() function
             *      mode argument.
             * @throws Invalid_param_exception if mode string is not valid.
             */
            Mode(const std::string &mode_str);
        };

        /** Interface for platform native file handle. Platform-dependent class
         * should be derived from this one.
         */
        class Native_handle {
        public:

            /** Unique pointer type. */
            typedef std::unique_ptr<Native_handle> Unique_ptr;

            /** Helper class to hold reference to a stream and optionally
             * release a lock before releasing the reference to a stream.
             */
            class Stream_ref_holder {
            public:
                /** Constructor. */
                Stream_ref_holder(Stream::Ptr stream,
                                  std::unique_lock<std::mutex> &&lock):
                    stream(stream), lock(std::move(lock))
                {}

                /** Default move constructor. */
                Stream_ref_holder(Stream_ref_holder &&) = default;

                ~Stream_ref_holder()
                {
                    /* Prevent access to the mutex after stream is released. */
                    if (lock) {
                        lock.unlock();
                        lock.release();
                    }
                    stream = nullptr;
                }
            private:
                Stream::Ptr stream;
                std::unique_lock<std::mutex> lock;
            };

            virtual
            ~Native_handle()
            {}

            /** Set associated stream. */
            void
            Set_stream(Stream::Ptr stream);

            /** Platform-specific handling for write operation.
             * Handle_write_completion() method should be invoked in associated
             * stream upon operation completion.
             * Request should be provided in cur_write_request member.
             */
            virtual void
            Write() = 0;

            /** Platform-specific handling for read operation.
             * Handle_read_completion() method should be invoked in associated
             * stream upon operation completion.
             * Request should be provided in cur_read_request member.
             */
            virtual void
            Read() = 0;

            /** Platform-specific handling for lock operation.
             * Should not block.
             */
            virtual Lock_result
            Try_lock() = 0;

            /** Platform-specific handling for lock operation.
             * Can block if file is locked by another process.
             */
            virtual bool
            Lock() = 0;

            /** Platform-specific handling for unlock operation.
             * Handle_lock_completion() method should be invoked in associated
             * stream upon operation completion.
             * Request should be provided in cur_lock_request member.
             */
            virtual bool
            Unlock() = 0;

            /** Cancel current write operation.
             * @return true if the operation was canceled, false if unable to
             *      cancel it now.
             */
            virtual bool
            Cancel_write() = 0;

            /** Cancel current read operation.
             * @return true if the operation was canceled, false if unable to
             *      cancel it now.
             */
            virtual bool
            Cancel_read() = 0;

            /** Close the handle. Cancel all pending operations. */
            virtual void
            Close() = 0;

            /** Process write operation abortion. */
            void
            Handle_write_abort();

            /** Process write operation abortion. */
            void
            Handle_read_abort();

            /** Current read request. */
            Read_request::Ptr cur_read_request;
            /** Current write request. */
            Write_request::Ptr cur_write_request;
            /** Current lock request. */
            Io_request::Ptr cur_lock_request;
            /** Current unlock request. */
            Io_request::Ptr cur_unlock_request;
            /** Is the handle already closed. */
            bool is_closed = false;
            /** Related stream. */
            Stream::Ptr stream;
            /** Holds reference to a stream while write operation is in progress. */
            Stream::Ptr write_active,
            /** Holds reference to a stream while read operation is in progress. */
                        read_active;
        protected:
            /** Called by derived class to indicate whether platform call is
             * currently active or not.
             *
             * @param is_active Activity flag. Only toggling is allowed.
             * @param lock Lock to pass to returned object.
             * @return Reference holder for the stream.
             */
            Stream_ref_holder
            Set_write_activity(bool is_active,
                               std::unique_lock<std::mutex> &&lock =
                                   std::unique_lock<std::mutex>());

            /** Called by derived class to indicate whether platform call is
             * currently active or not.
             *
             * @param is_active Activity flag. Only toggling is allowed.
             * @param lock Lock to pass to returned object.
             * @return Reference holder for the stream.
             */
            Stream_ref_holder
            Set_read_activity(bool is_active,
                              std::unique_lock<std::mutex> &&lock =
                                  std::unique_lock<std::mutex>());

            /** Get native controlled of the related stream. */
            Native_controller &
            Get_native_controller() const
            {
                return stream->Get_native_controller();
            }
        };

        /** Construct file stream.
         *
         * @param processor Associated processor instance.
         * @param path File path.
         * @param mode Parsed file opening mode.
         * @param maintain_pos Indicates whether file position maintenance is
         *      required. See {@link File_processor::Open}.
         * @param native_handle Native handle to be used for stream operations.
         */
        Stream(File_processor::Ptr processor, const std::string &path, Mode mode,
               bool maintain_pos, Native_handle::Unique_ptr&& native_handle);

        /** Get current position in the stream. This method has meaning only
         * file position is maintained by the stream.
         *
         * @return Current position in the stream.
         */
        Offset
        Get_current_pos() const
        {
            return cur_pos;
        }

        /** Set new current position for the stream. In case the stream does
         * not maintain the current position, this operation does nothing. Be
         * aware that seeking result is undefined if read or write operation is
         * in progress.
         *
         * @param pos New current position value.
         * @param is_relative Indicates that position is set relatively to
         *      the current position value.
         * @return New absolute current position value.
         * @throws Invalid_param_exception if the resulted position is invalid
         *      (e.g. negative).
         */
        Offset
        Seek(Offset pos, bool is_relative = false);

        /** Default prototype for lock operation completion handler. */
        typedef Callback_proxy<void, Io_result> Lock_handler;

        /** Put an exclusive lock on file.
         * On Windows it's mandatory lock on first byte of file implemented
         * via LockFileEx() call.
         * On Linux it's advisory lock implemented via flock() call.
         * File can be unlocked via Unlock() call or via Close().
         *
         * NOTE: No double lock supported:
         * Stream can be locked again only after successful Unlock() operation.
         * I.e. subsequent Lock on the same stream will fail until Unlock()
         * is called and unlock completion is delivered.
         *
         * @param completion_handler User specified completion handler.
         * @param comp_ctx User specified completion context.
         * @param lock true: lock operation, false: unlock operation.
         * @return Operation_waiter which can be used to wait, or
         *         set timeout or cancel for pending lock operation.
         */
        Operation_waiter
        Lock(   Lock_handler completion_handler,
                Request_completion_context::Ptr comp_ctx =
                        Request_temp_completion_context::Create(),
                bool lock = true);

        /** Remove lock from file.
         * On Windows it unlocks on first byte of file implemented
         * via UnlockFile() call.
         * On Linux it is implemented via flock() call.
         *
         * @param completion_handler User specified completion handler.
         * @param comp_ctx User specified completion context.
         * @return Operation_waiter which can be used to wait
         *         for pending unlock operation to complete.
         */
        Operation_waiter
        Unlock( Lock_handler completion_handler =
                        Make_dummy_callback<void, Io_result>(),
                Request_completion_context::Ptr comp_ctx =
                        Request_temp_completion_context::Create())
        {
            return Lock(completion_handler, comp_ctx, false);
        }

    private:
        friend class File_processor;
        friend class Native_handle;

        /** Control block for lock operations. */
        typedef struct {
            /** Set this to false to exit locker thread */
            bool flock_thread_active = false;
            /** Set this to true to tell thread to call Lock() */
            bool flock_acquire_requested = false;
            /** Tells if the file is locked currently. */
            bool flock_acquired = false;
            /** Use to notify thread about state change. */
            std::condition_variable flock_notifier;
        } Lock_cb;

        Lock_cb lock_cb;

        /** Associated processor instance. */
        File_processor::Weak_ptr processor;
        /** Mutex for protecting operation state (queues, current etc.). */
        std::mutex op_mutex;
        /** Queued read requests. */
        std::list<Read_request::Ptr> read_queue;
        /** Queued write requests. */
        std::list<Write_request::Ptr> write_queue;

        /** Opening mode. */
        Mode mode;
        /** Indicates whether file position maintenance is required. */
        bool maintain_pos;
        /** Current position in file when maintained by stream. */
        Offset cur_pos = 0;
        /** Native handle instance. */
        Native_handle::Unique_ptr native_handle;

        /** @see Io_stream::Write_impl
         * @throws Closed_stream_exception if the stream is already closed.
         */
        virtual Operation_waiter
        Write_impl(Io_buffer::Ptr buffer,
                   Offset offset,
                   Write_handler completion_handler,
                   Request_completion_context::Ptr comp_ctx) override;

        /** @see Io_stream::Read_impl
         * @throws Closed_stream_exception if the stream is already closed.
         */
        virtual Operation_waiter
        Read_impl(size_t max_to_read, size_t min_to_read, Offset offset,
                  Read_handler completion_handler,
                  Request_completion_context::Ptr comp_ctx) override;

        /** @see Io_stream::Close_impl
         * @throws Closed_stream_exception if the stream is already closed.
         */
        virtual Operation_waiter
        Close_impl(Close_handler completion_handler,
                   Request_completion_context::Ptr comp_ctx) override;

        /** Either queue or instantly submit write operation. Write requests are
         * serialized so there is no more than one write request submitted
         * simultaneously from one stream.
         * Should be called with op_lock acquired.
         */
        void
        Queue_write(Write_request::Ptr request);

        /** Check if more write operations are pending. Submit next one of they
         * are.
         * Should be called with op_lock acquired.
         */
        void
        Push_write_queue();

        /** Either queue or instantly submit read operation. Read requests are
         * serialized so there is no more than one read request submitted
         * simultaneously from one stream.
         * Should be called with op_lock acquired.
         */
        void
        Queue_read(Read_request::Ptr request);

        /** Check if more read operations are pending. Submit next one of they
         * are.
         * Should be called with op_lock acquired.
         */
        void
        Push_read_queue();

        /** Process write request (set current in native handle). */
        void
        Handle_write();

        /** Process read request (set current in native handle). */
        void
        Handle_read();

        /** Process lock request (set current in native handle). */
        void
        Handle_lock();

        /** Process lock request (set current in native handle). */
        void
        Handle_unlock();

        /** Process close request. */
        void
        Handle_close(Request::Ptr request);

        /** Process write operation cancellation. */
        void
        Handle_write_cancel(Write_request::Ptr request);

        /** Process read operation cancellation. */
        void
        Handle_read_cancel(Read_request::Ptr request);

        /** Process lock operation cancellation. */
        void
        Handle_lock_cancel(Io_request::Ptr request);

        /** Used internally to cancel pending lock operation. Must
         * be called with op_mutex locked.*/
        void
        Cancel_lock_operation(bool stop_locker_thread);

        /** Used internally to handle completed write request. Request is current
         * write request in native_handle.
         *
         * @param completion_handler User provided completion handler.
         */
        void
        Handle_write_completion(Request::Handler completion_handler);

        /** Used internally to handle completed read request. Request is current
         * read request in native_handle.
         *
         * @param completion_handler User provided completion handler.
         */
        void
        Handle_read_completion(Request::Handler completion_handler);

        /** Called by native handle when write operation was aborted. */
        void
        Handle_write_abort();

        /** Called by native handle when read operation was aborted. */
        void
        Handle_read_abort();

        Native_controller &
        Get_native_controller() const;

        /** Helper function to complete cur_lock_request or cur_unlock_request.
         * It checks for timeout, cancellation etc...
         * @param request Request to complete.
         * @param result Result of processing.
         */
        void
        Complete_request(Io_request::Ptr& request, Io_result result = Io_result::OTHER_FAILURE);

        /** Process pending lock request
         * Poll regularly (each 100ms) for lock availability. */
        void
        Locker_thread();

    };

    /** Interface for native I/O controller which manages I/O operations for
     * all native handles.
     */
    class Native_controller {
    public:
        virtual
        ~Native_controller()
        {}

        /** Enable the controller. */
        virtual void
        Enable() = 0;

        /** Disable the controller. */
        virtual void
        Disable() = 0;

        /** Register new opened file handle. */
        virtual void
        Register_handle(Stream::Native_handle &handle) = 0;

        /** Unregister previously registered file handle. */
        virtual void
        Unregister_handle(Stream::Native_handle &handle) = 0;

        /** Create controller instance. */
        static std::unique_ptr<Native_controller>
        Create();
    };

    /** Get global or create new processor instance. */
    template <typename... Args>
    static Ptr
    Get_instance(Args &&... args)
    {
        return singleton.Get_instance(std::forward<Args>(args)...);
    }

    File_processor();

    /** Open file.
     *
     * @param name File path.
     * @param mode Opening mode similar to the corresponding argument for fopen()
     *      function. It supports "x" specifier and do not support "a" and "b"
     *      specifiers. All files are opened in binary mode.
     *      NOTE: In addition "rx" combination is supported and means create
     *      file in read-only mode if it does not exist.
     * @param maintain_pos Indicates whether file position maintenance is
     *      required. It should be "false" for devices which do not support
     *      seeking. In this case file offset for all I/O operations will be set
     *      to zero. In case it is "true" the returned file stream will maintain
     *      current position accordingly to the number of bytes read or written.
     * @return Created file stream.
     *
     * @throws Invalid_param_exception if mode string is not valid.
     * @throws Not_found_exception if the specified path does not exist when it
     *      must exist based on provided opening mode.
     * @throws Permission_denied_exception if there is insufficient permissions
     *      for file creation.
     * @throws Already_exists_exception if new file creation requested ('x'
     *      specified in opening mode) but the file already exists.
     * @throws Exception in case of any other error returned by platform.
     */
    Stream::Ref
    Open(const std::string &name,const std::string &mode, bool maintain_pos = true);

    /**
     * Platform independent method for opening a standard library file handle
     * providing an UTF-8 encoded path and mode.
     * @param name UTF-8 encoded file path to open.
     * @param mode UTF-8 encoded access mode, see standard fopen() method.
     * @return Valid file handle on success, or nullptr on failure.
     */
    static FILE*
    Fopen_utf8(const std::string &name, const std::string & mode);

    /**
     * Platform independent method for renaming a file providing UTF-8
     * encoded paths.
     * @param old_name UTF-8 encoded old file name.
     * @param new_name UTF-8 encoded new file name;
     * @return true on success, false on failure.
     */
    static bool
    Rename_utf8(const std::string &old_name, const std::string &new_name);

    /**
     * Platform independent method for removing a file providing UTF-8
     * encoded path.
     */
    static bool
    Remove_utf8(const std::string &name);

    /**
     * Platform independent method for checking file access permission
     * providing UTF-8 file name.
     * @param name UTF-8 encoded file name.
     * @param mode Mode as in standard access() method.
     * @return As in standard access() method.
     */
    static int
    Access_utf8(const std::string &name, int mode);

private:
    friend class Stream;

    /** Singleton object. */
    static Singleton<File_processor> singleton;
    /** Worker for processing and completion context. */
    Request_worker::Ptr worker;
    /** Own completion context for cases when user does not provided one. */
    Request_completion_context::Ptr comp_ctx;
    /** Native I/O controller instance. */
    std::unique_ptr<Native_controller> native_controller;

    /** Handle processor enabling. */
    virtual void
    On_enable() override;

    /** Handle disabling request. */
    virtual void
    On_disable() override;

    /** Unregister native handle associated with a stream. */
    void
    Unregister_handle(Stream::Native_handle &handle);

    /** Open platform dependent native handle suitable for file processing. */
    Stream::Native_handle::Unique_ptr
    Open_native_handle(const std::string &name, const std::string &mode);

protected:
    /** Register opened stream in a processor. */
    void
    Register_stream(File_processor::Stream::Ptr);
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* FILE_PROCESSOR_H_ */
