// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file windows_file_handle.h
 */

#ifndef _WINDOWS_FILE_HANDLE_H_
#define _WINDOWS_FILE_HANDLE_H_

#include <ugcs/vsm/file_processor.h>
#include <cstring>

#include <windows.h>

/* Some weird code can define this macro in Windows headers. */
#ifdef ERROR
#undef ERROR
#endif

namespace ugcs {
namespace vsm {
namespace internal {

/** Windows-specific implementation of system native file handle. */
class Windows_file_handle: public File_processor::Stream::Native_handle {
public:

    /** Construct an instance based on already opened handles. */
    Windows_file_handle(
            HANDLE handle,
            HANDLE write_handle = INVALID_HANDLE_VALUE);

    /** Construct an instance by opening a file specified by path. */
    Windows_file_handle(
    		const std::string &path,
    		File_processor::Stream::Mode mode);

    /** Closes handles on destruction. */
    virtual
    ~Windows_file_handle();

    /** Schedule write operation based on current write request. */
    virtual void
    Write() override;

    /** Schedule read operation based on current read request. */
    virtual void
    Read() override;

    /** Schedule lock operation based on current lock request. */
    virtual File_processor::Stream::Lock_result
    Try_lock() override;

    /** Schedule lock operation based on current lock request. */
    virtual bool
    Lock() override;

    /** Schedule unlock operation based on current lock request. */
    virtual bool
    Unlock() override;

    /** Cancel current write operation. */
    virtual bool
    Cancel_write() override;

    /** Cancel current read operation. */
    virtual bool
    Cancel_read() override;

    /** Cancel all pending I/O operations. */
    void
    Cancel_io(bool write);

    /** Completion handler for platform write call.
     * @param transfer_size Number of bytes transferred during I/O call.
     * @param error Error code if the operation has failed.
     */
    void
    Write_complete_cbk(size_t transfer_size, DWORD error);

    /** Completion handler for platform read call.
     * @param transfer_size Number of bytes transferred during I/O call.
     * @param error Error code if the operation has failed.
     */
    void
    Read_complete_cbk(size_t transfer_size, DWORD error);

    /** Completion handler for platform lock call.
     * @param error Error code if the operation has failed.
     */
    void
    Lock_complete_cbk(DWORD error);

    /** Completion handler for platform I/O call.
     *
     * @param io_cb Pointer to I/O operation control block.
     * @param transfer_size Number of bytes transferred during I/O call.
     * @param error Error code if the operation has failed.
     */
    void
    Io_complete_cbk(OVERLAPPED *io_cb, size_t transfer_size, DWORD error);

    /** Close the handle. */
    virtual void
    Close() override;

    /** Map error value to Io_result. */
    static Io_result
    Map_error(DWORD error);

protected:
    friend class Overlapped_io_controller;

    /** Opened file handle for reading/writing. */
    HANDLE handle;
    /** Use this handle for writing, if specified. */
    HANDLE write_handle = INVALID_HANDLE_VALUE;
    /** Control block for current read operation. */
    OVERLAPPED read_cb;
    /** Control block for current write operation. */
    OVERLAPPED write_cb;
    /** Control block for current lock operation. */
    OVERLAPPED lock_cb;
    /** Event to signal about lock completion. */
    HANDLE lock_complete_event = INVALID_HANDLE_VALUE;
    /** Result of lock operation. (initialize with some nonsense value)*/
    DWORD lock_complete_result = ERROR_ARENA_TRASHED;
    /** Read buffer. */
    std::shared_ptr<std::vector<uint8_t>> read_buf;
    /** Number of bytes pending for read. */
    size_t read_size,
    /** Number of bytes pending for write. */
           write_size,
   /** Minimal number of bytes to read in current read operation. */
           min_read_size;
    /** File offset for read operation. */
    Io_stream::Offset read_offset,
    /** File offset for write operation. */
                      write_offset;
    /** Mutex for protecting write control block. */
    std::mutex write_mutex,
    /** Mutex for protecting read control block. */
               read_mutex;
};

} /* namespace internal */
} /* namespace vsm */
} /* namespace ugcs */

#endif /* _WINDOWS_FILE_HANDLE_H_ */
