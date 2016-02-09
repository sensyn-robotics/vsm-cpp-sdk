// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Description:
 *   Log class partial implementation (platform dependent).
 */

#include <ugcs/vsm/log.h>
#include <ugcs/vsm/file_processor.h>

#include <glob.h>

void
ugcs::vsm::Log::Remove_old_log_files()
{
    // This pattern is derived from the one used in Do_cleanup().
    auto pat = custom_log_file_name + LOG_FILE_ROTATOR_FIND_PATTERN;
    glob_t glob_result;

    // This results in a sorted array of filepaths.
    glob(pat.c_str(), 0, NULL, &glob_result);

    for (auto i = 0; i + custom_log_count < glob_result.gl_pathc; i++) {
        ugcs::vsm::File_processor::Remove_utf8(glob_result.gl_pathv[i]);
    }
    globfree(&glob_result);
}
