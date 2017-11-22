// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * shared_mutex_file.cpp
 *
 *  Created on: Dec 16, 2013
 *      Author: janis
 */

#include <ugcs/vsm/shared_mutex_file.h>

namespace ugcs {
namespace vsm {

Shared_mutex_file::Shared_mutex_file(const std::string& name, File_processor::Ptr fp)
{
    stream = fp->Open("/tmp/vsm_shared_mutex_" + name, "rx");
}

} /* namespace vsm */
} /* namespace ugcs */
