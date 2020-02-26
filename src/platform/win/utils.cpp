// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * Windows-specific implementation of Utils.
 */

#include <ugcs/vsm/utils.h>

// We want to explicitly say that case insensitive match is required.
std::regex_constants::syntax_option_type
ugcs::vsm::platform_independent_filename_regex_matching_flag =
    std::regex_constants::ECMAScript | std::regex_constants::icase;
