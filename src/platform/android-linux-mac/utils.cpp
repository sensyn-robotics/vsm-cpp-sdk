// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * Linux-specific implementation of Utils.
 */

#include <ugcs/vsm/utils.h>

// We want to explicitly say that case sensitive match is required.
// There is no std::regex::none enum defined, so we use ECMAScript as it is the default if no grammar specified,
// which is the default in c++11.
std::regex_constants::syntax_option_type
ugcs::vsm::platform_independent_filename_regex_matching_flag = std::regex_constants::ECMAScript;
