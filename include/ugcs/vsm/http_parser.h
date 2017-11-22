// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file properties.h
 *
 * Java properties set implementation.
 */
#ifndef _PROPERTIES_H_
#define _PROPERTIES_H_

#include <istream>
#include <map>
#include <string.h>
#include <ugcs/vsm/exception.h>

namespace ugcs {
namespace vsm {

/** This class implements limited HTTP parser according to RFC7230
 * Supports:
 * - HTTP Method name
 * - Header parsing in <name, value> pairs. name is case insensitive, value is trimmed.
 * Does not support:
 * - Header value folding.
 * - URL and protocol string parsing from method/result line.
 */
class Http_parser{
public:
    struct string_compare_ignore_case {
        bool operator() (const std::string& lhs, const std::string& rhs) const {
            return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
        }
    };

    /** Parse http header from text stream.
     * @param stream Input text stream.
     */
    bool
    Parse(std::istream &in);

    /** Check if the header field with the specified name exists. */
    bool
    Header_exists(const std::string &name) const;

    /** Get string value of header
     * @param name Header name, case insensitive.
     * @return String value of header. "" if not present.
     */
    std::string
    Get_header_value(const std::string &name) const;

    /** Get HTTP method name
     */
    std::string
    Get_method() const;

    std::string parser_error;

private:
    std::map<std::string, std::string, string_compare_ignore_case> header_table;
    std::string http_method;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _PROPERTIES_H_ */
