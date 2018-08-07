// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * Linux-specific implementation of Utils.
 */

#include <ugcs/vsm/utils.h>

regex::regex_constants::syntax_option_type
ugcs::vsm::platform_independent_filename_regex_matching_flag = regex::regex_constants::none;

#ifdef ANDROID
namespace regex {
// NDK does not have bsearch, so we are providing our own.
const void*
bsearch(
        const void *key,
        const void *base,
        size_t n,
        size_t size,
        int (* cmp)(const void *, const void *))
{
    const void *mp;
    int cr;
    if (n  <= 0) {
        return NULL;
    }
    mp = reinterpret_cast<const void*>(reinterpret_cast<const char*>(base) + n / 2 * size);
    cr = cmp(key, mp);
    if (cr == 0) {
        return mp;
    } else {
        if (cr > 0) {
            if (n <= 2) {
                return NULL;
            }
            return bsearch(key, reinterpret_cast<const char*>(mp) + size, (n - 1 ) / 2, size, cmp);
        } else if (cr < 0) {
            if (n <= 1) {
                return NULL;
            }
            return bsearch(key, base, n / 2, size, cmp);
        }
    }
}
} // namespace regex
#endif
