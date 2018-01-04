// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file regex.h
 *
 * This file is replacement for std::regex which is not yet implemented in
 * the standard library. Once it is done, this file should be removed.
 */

#ifndef _REGEX_H_
#define _REGEX_H_

#include <string.h>
#include <list>

namespace regex {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

#ifdef __APPLE__
/*
 * Some OSX portability stuff...
 * On OSX "int isalpha(int)" and friends are defined as inline in ctype.h
 * but this regex implementation requires them to be real functions
 * as it uses function pointers. So, we use Apple specific #define to revert
 * to standards compliant (with external linkage) compilation of ctype.h
 *
 * WARNING: If ctype.h is already included before preprocessor reaches here,
 *  _DONT_USE_CTYPE_INLINE_ will be ignored due to include guards.
 *  Make sure regex.h is included before ctype.h
 */
#define _DONT_USE_CTYPE_INLINE_
#endif

#ifdef ANDROID
// Android NDK does not have bsearch, so we are providing our own. Defined in utils.cpp
void
*bsearch(
        const void *key,
        const void *base,
        size_t n,
        size_t size,
        int (* cmp)(const void *, const void *));
#endif

#include <ugcs/vsm/deelx.h>
#pragma GCC diagnostic pop

namespace regex_constants {
    typedef int syntax_option_type;
    static constexpr syntax_option_type none = REGEX_FLAGS::NO_FLAG;
    static constexpr syntax_option_type icase = REGEX_FLAGS::IGNORECASE;
    static constexpr syntax_option_type ECMAScript = REGEX_FLAGS::NO_FLAG;  // not supported by deelx
    static constexpr syntax_option_type syntax_option_default = ECMAScript;
}

class smatch;

/** Regular expression object. Corresponds to std::regex. */
class regex {
public:
    regex(
        const std::string &pattern,
        regex_constants::syntax_option_type flags = regex_constants::syntax_option_default):
        pattern(pattern), re(pattern.c_str(), flags)
    {
    }

    /** deelx regexp is not copy-constructible. */
    regex(const regex &re):
        pattern(re.pattern), re(re.pattern.c_str(), re.re.m_builder.m_nFlags)
    {}

private:
    friend bool regex_match(const std::string &s, smatch &m, regex &re);
    friend bool regex_search(const std::string &s, smatch &m, regex &re);

    std::string pattern;
    CRegexpT<char> re;
};

/** String match object. Corresponds to std::smatch. */
class smatch {
public:
    smatch() = default;

    bool
    ready() const
    {
        return is_ready;
    }

    bool
    empty() const
    {
        return result.size() == 0;
    }

    size_t
    size() const
    {
        return result.size();
    }

    size_t
    position(size_t n = 0) const
    {
        if (!n) {
            return result.front().start;
        }
        return (*this)[n].position();
    }

    size_t
    length(size_t n = 0) const
    {
        if (!n) {
            const Match_entry &e = result.front();
            return e.end - e.start;
        }
        return (*this)[n].length();
    }

    std::string
    str(size_t n = 0) const
    {
        if (!n) {
            const Match_entry &e = result.front();
            return target.substr(e.start, e.end - e.start);
        }
        return (*this)[n].str();
    }

    bool
    matched(size_t n = 0) const
    {
        if (!n) {
            const Match_entry &e = result.front();
            return e.matched;
        }
        return (*this)[n].matched();
    }

    smatch
    operator[](size_t idx) const
    {
        for (const Match_entry &group : result) {
            if (!idx) {
                return smatch(target, group);
            }
            idx--;
        }
        VSM_EXCEPTION(ugcs::vsm::Exception, "Group index out of range");
    }

private:
    friend bool regex_match(const std::string &s, smatch &m, regex &re);
    friend bool regex_search(const std::string &s, smatch &m, regex &re);

    bool is_ready = false;

    class Match_entry {
    public:
        bool matched;
        /** Match start and end position in target sequence. */
        size_t start, end;

        Match_entry():
            matched(false), start(0), end(0)
        {}

        Match_entry(size_t start, size_t end):
            matched(true), start(start), end(end)
        {}
    };
    /** Target sequence. */
    std::string target;

    std::list<Match_entry> result;

    smatch(std::string target, const Match_entry &e):
        is_ready(true), target(target)
    {
        result.push_back(e);
    }

    void
    Set_result(const std::string &target, std::list<Match_entry> &&result)
    {
        this->target = target;
        this->result = std::move(result);
        is_ready = true;
    }
};

/** See std::regex_match. */
inline bool
regex_match(const std::string &s, smatch &m, regex &re)
{
    MatchResult result = re.re.MatchExact(s.c_str());
    if (!result.IsMatched()) {
        m.Set_result(s, std::list<smatch::Match_entry>());
        return false;
    }
    std::list<smatch::Match_entry> groups;
    groups.push_back(smatch::Match_entry(result.GetStart(), result.GetEnd()));
    for (int i = 1; i <= result.MaxGroupNumber(); i++) {
        int start = result.GetGroupStart(i);
        if (start < 0) {
            groups.push_back(smatch::Match_entry());
        } else {
            groups.push_back(smatch::Match_entry(start, result.GetGroupEnd(i)));
        }
    }
    m.Set_result(s, std::move(groups));
    return true;
}

/** See std::regex_search. */
inline bool
regex_search(const std::string &s, smatch &m, regex &re)
{
    MatchResult result = re.re.Match(s.c_str());
    if (!result.IsMatched()) {
        m.Set_result(s, std::list<smatch::Match_entry>());
        return false;
    }
    std::list<smatch::Match_entry> groups;
    groups.push_back(smatch::Match_entry(result.GetStart(), result.GetEnd()));
    for (int i = 1; i <= result.MaxGroupNumber(); i++) {
        int start = result.GetGroupStart(i);
        if (start < 0) {
            groups.push_back(smatch::Match_entry());
        } else {
            groups.push_back(smatch::Match_entry(start, result.GetGroupEnd(i)));
        }
    }
    m.Set_result(s, std::move(groups));
    return true;
}

} /* namespace regex */

#endif /* _REGEX_H_ */
