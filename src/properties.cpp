// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Properties class implementation.
 */

#include <ugcs/vsm/properties.h>
#include <ugcs/vsm/platform.h>
#include <ugcs/vsm/debug.h>
#include <ugcs/vsm/utils.h>

#include <climits>
#include <cmath>
#include <functional>
#include <memory>
#include <sstream>
#include <map>
#include <algorithm>

using namespace ugcs::vsm;

/* Properties::Property class. */

Properties::Property::Property(std::string &&value):
    str_repr(std::move(value)),
    description(LINE_TERMINATOR)
{
    std::string trimmed(str_repr);
    Trim(trimmed);
    try {
        size_t pos;
        int_repr = std::stol(trimmed, &pos, 0);
        if (pos == trimmed.size()) {
            int_valid = true;
        } else {
            int_valid = false;
        }
    } catch(...) {
        int_valid = false;
    }

    try {
        size_t pos;
        float_repr = std::stod(trimmed, &pos);
        if (pos == trimmed.size()) {
            float_valid = true;
        } else {
            float_valid = false;
        }
    } catch (...) {
        float_valid = false;
    }

    /* Create integer representation from float if possible. */
    if (!int_valid && float_valid &&
        float_repr >= LLONG_MIN && float_repr <= LLONG_MAX) {
        int_repr = std::lrint(float_repr);
        int_valid = true;
    }
}

Properties::Property::Property(int32_t value):
    str_repr(std::to_string(value)), int_repr(value), float_repr(value),
    int_valid(true), float_valid(true),
    description(LINE_TERMINATOR)
{
}

Properties::Property::Property(double value):
    str_repr(std::to_string(value)), float_repr(value), float_valid(true),
    description(LINE_TERMINATOR)
{
    if (value >= LLONG_MIN && value <= LLONG_MAX) {
        int_repr = std::lrint(value);
        int_valid = true;
    } else {
        int_valid = false;
    }
}

/* Properties class. */

Singleton<Properties> Properties::singleton;

Properties::Properties()
{}

void
Properties::Load(std::istream &stream)
{
    table.clear();

    /* State machine. */

    /* Token value available as a result from state handler. */
    class Token {
    public:
        /* Token type. */
        enum class Type {
            /* Empty token. */
            NONE,
            /* One character. */
            CHAR,
            /* Characters string. */
            STRING,
            /* Full property. */
            PROPERTY
        };

        /* Token type. */
        Type type = Type::NONE;
        /* Character value. */
        int v_char;
        /* String value. */
        std::string v_string;
        /* Property value. */
        struct {
            std::string key, value;
        } v_property;

        Token() = default;

        Token(Token &&token):
            type(token.type), v_char(token.v_char),
            v_string(std::move(token.v_string)),
            v_property(std::move(token.v_property))
        {
            token.type = Type::NONE;
        }

        void
        Set(int c)
        {
            type = Type::CHAR;
            v_char = c;
        }

        void
        Set(const std::string &str)
        {
            type = Type::STRING;
            v_string = str;
        }

        void
        Set(const std::string &key, const std::string &value)
        {
            type = Type::PROPERTY;
            v_property.key = key;
            v_property.value = value;
        }

        explicit operator bool() const
        {
            return type != Type::NONE;
        }
    };

    /* Indicates that character was consumed by a state. */
    static constexpr int CHAR_CONSUMED = 0;

    /* Represents parser FA state. */
    class State {
    public:
        typedef std::unique_ptr<State> Ptr;

        virtual
        ~State()
        {}

        /* Feed token for the state.
         * @param token Token to feed.
         * @param next_state Receives next state if a next one is created.
         * @return true if state is processed and a token can be obtained.
         */
        bool
        Feed(Token &&token, std::unique_ptr<State> &next_state)
        {
            std::unique_ptr<State> next_substate;
            while (substate && token && substate->Feed(std::move(token), next_substate)) {
                substate->Get_token(token);
                substate = std::move(next_substate);
            }
            if (substate) {
                return false;
            }
            if (token) {
                return On_token(std::move(token), next_state);
            }
            return false;
        }

        /* Feed next character for the state.
         * @param c Next character. Set to CHAR_CONSUMED if consumed.
         * @param next_state Receives next state if a next one is created.
         * @return true if state is processed and a token can be obtained.
         */
        bool
        Feed(int &c, std::unique_ptr<State> &next_state)
        {
            if (substate) {
                std::unique_ptr<State> next_substate;
                if (substate->Feed(c, next_substate)) {
                    Token token;
                    substate->Get_token(token);
                    substate = std::move(next_substate);
                    return Feed(std::move(token), next_state);
                }
            } else {
                return On_char(c, next_state);
            }
            return false;
        }

        /* Get token after state is processed. */
        virtual void
        Get_token(Token &token __UNUSED)
        {}

    protected:
        /* Called on each character fed.
         * @param c Next character, should be set to CHAR_CONSUMED if consumed.
         * @param next_state Receives next state if a next one is created.
         * @return true if state is processed and a token can be obtained.
         */
        virtual bool
        On_char(int &c __UNUSED, std::unique_ptr<State> &next_state __UNUSED)
        {
            return true;
        }

        /* Token retrieved from substate or previous state.
         * @param token Retrieved token.
         * @param next_state Receives next state if a next one is created.
         * @return true if state is processed and a token can be obtained.
         */
        virtual bool
        On_token(Token &&token __UNUSED, std::unique_ptr<State> &next_state __UNUSED)
        {
            return true;
        }

        void
        Set_substate(std::unique_ptr<State> &&state)
        {
            substate = std::move(state);
        }

    private:
        /* Current sub-state. */
        std::unique_ptr<State> substate;
    };

    /* Allowed whitespaces. */
    static auto Is_whitespace = [](int c)
        {
            return c == ' ' || c == '\t' || c == '\f';
        };

    /* Line terminator started and should be skipped. */
    class Line_terminator_state: public State {
        bool cr_seen = false;

        virtual bool
        On_char(int &c, std::unique_ptr<State> &next_state __UNUSED) override
        {
            if (c == std::istream::traits_type::eof()) {
                return true;
            }
            /* Skip either CR, CR+LF or LF. */
            if (c == '\r') {
                if (cr_seen) {
                    return true;
                }
                cr_seen = true;
                c = CHAR_CONSUMED;
                return false;
            }
            if (c == '\n') {
                c = CHAR_CONSUMED;
                return true;
            }
            return true;
        }
    };

    /* Comment encountered and should be skipped. */
    class Comment_state: public State {
        virtual bool
        On_char(int &c, std::unique_ptr<State> &next_state) override
        {
            if (c == std::istream::traits_type::eof()) {
                return true;
            }
            /* Comment terminated by new line. */
            if (c == '\n' || c == '\r') {
                next_state = Ptr(new Line_terminator_state);
                return true;
            }
            /* All the rest considered as comment and consumed. */
            c = CHAR_CONSUMED;
            return false;
        }
    };

    /* Line is wrapped by '\' in the end. Skip new line and leading whitespaces. */
    class Line_break_state: public State {
    public:
        Line_break_state()
        {
            /* Skip new line first. */
            Set_substate(Ptr(new Line_terminator_state));
        }

    private:
        virtual bool
        On_char(int &c, std::unique_ptr<State> &next_state __UNUSED) override
        {
            if (c == std::istream::traits_type::eof()) {
                return true;
            }
            /* Skip all leading whitespaces. */
            if (Is_whitespace(c)) {
                c = CHAR_CONSUMED;
                return false;
            }
            return true;
        }
    };

    /* Read and parse escaped character. */
    class Escape_state: public State {
    public:
        /* @param in_key Indicates whether escape code was encountered in key
         *      identifier and thus '\=' and '\:' are allowed.
         */
        Escape_state(bool in_key = false): in_key(in_key)
        {}

    private:
        bool in_key;
        bool backslash_seen = false;
        int escaped_char = 0;
        /* Currently reading 4-digits character code. */
        bool reading_code = false;
        int num_digits_read = 0;

        virtual bool
        On_char(int &c, std::unique_ptr<State> &next_state) override
        {
            if (c == std::istream::traits_type::eof()) {
                VSM_EXCEPTION(Parse_exception, "Unexpected EOF - unclosed escape");
            }

            if (!backslash_seen) {
                ASSERT(c == '\\');
                backslash_seen = true;
                c = CHAR_CONSUMED;
                return false;
            }

            /* Parse unicode escape. */
            if (reading_code) {
                ASSERT(num_digits_read < 4);
                int digit;
                if (c >= '0' && c <= '9') {
                    digit = c - '0';
                } else if (c >= 'a' && c <= 'f') {
                    digit = c - 'a' + 10;
                } else if (c >= 'A' && c <= 'F') {
                    digit = c - 'A' + 10;
                } else {
                    VSM_EXCEPTION(Parse_exception, "Invalid digit in unicode escape");
                }
                escaped_char = (escaped_char << 4) | digit;
                num_digits_read++;
                c = CHAR_CONSUMED;
                return num_digits_read == 4;
            }

            switch (c) {
            case ' ':
                escaped_char = ' ';
                break;
            case '\t':
            case 't':
                escaped_char = '\t';
                break;
            case '\f':
            case 'f':
                escaped_char = '\f';
                break;
            case 'r':
                escaped_char = '\r';
                break;
            case 'n':
                escaped_char = '\n';
                break;
            case '\\':
                escaped_char = '\\';
                break;
            case '\r':
            case '\n':
                next_state = Ptr(new Line_break_state());
                break;
            case 'u':
                reading_code = true;
                c = CHAR_CONSUMED;
                return false;
            default:
                if (in_key) {
                    switch (c) {
                    case '=':
                    case ':':
                        escaped_char = c;
                        break;
                    default:
                        VSM_EXCEPTION(Parse_exception,
                                      "Invalid escape character: %c", c);
                    }
                } else {
                    VSM_EXCEPTION(Parse_exception,
                                  "Invalid escape character: %c", c);
                }
            }
            c = CHAR_CONSUMED;
            return true;
        }

        virtual void
        Get_token(Token &token) override
        {
            if (escaped_char) {
                token.Set(escaped_char);
            }
        }
    };

    /* Read string (either key name or value string). */
    class Read_string_state: public State {
    public:
        Read_string_state(bool is_key): is_key(is_key)
        {}

    private:
        bool is_key;
        std::string str;

        virtual bool
        On_char(int &c, std::unique_ptr<State> &next_state __UNUSED) override
        {
            if (c == std::istream::traits_type::eof()) {
                return true;
            }
            if (c == '\\') {
                Set_substate(Ptr(new Escape_state(is_key)));
                return false;
            }
            /* New line terminates the token. */
            if (c == '\r' || c == '\n') {
                return true;
            }
            /* Whitespace, '=' and ':' terminates key token. */
            if (is_key && (Is_whitespace(c) || c == '=' || c == ':')) {
                return true;
            }
            str += c;
            c = CHAR_CONSUMED;
            return false;
        }

        virtual bool
        On_token(Token &&token, std::unique_ptr<State> &next_state __UNUSED) override
        {
            if (token.type == Token::Type::CHAR) {
                str += token.v_char;
            } else {
                ASSERT(false);
            }
            return false;
        }

        virtual void
        Get_token(Token &token) override
        {
            token.Set(str);
        }
    };

    /* Reading key-value string. */
    class Key_value_state: public State {
        std::string key, value;
        bool assignment_seen = false;

        virtual bool
        On_char(int &c, std::unique_ptr<State> &next_state __UNUSED) override
        {
            if (c == std::istream::traits_type::eof()) {
                return true;
            }
            /* Skip whitespaces. */
            if (Is_whitespace(c)) {
                c = CHAR_CONSUMED;
                return false;
            }
            /* Skip assignment token. */
            if (!key.empty() && !assignment_seen &&
                (c == '=' || c == ':')) {
                assignment_seen = true;
                c = CHAR_CONSUMED;
                return false;
            }
            Set_substate(Ptr(new Read_string_state(key.empty())));
            return false;
        }

        virtual bool
        On_token(Token &&token, std::unique_ptr<State> &next_state __UNUSED) override
        {
            if (token.type == Token::Type::STRING) {
                if (key.empty()) {
                    if (token.v_string.empty()) {
                        VSM_EXCEPTION(Parse_exception, "Empty key name");
                    }
                    key = std::move(token.v_string);
                } else {
                    /* Value parsed, state is terminated. */
                    value = std::move(token.v_string);
                    return true;
                }
            } else {
                ASSERT(false);
            }
            return false;
        }

        virtual void
        Get_token(Token &token) override
        {
            token.Set(key, value);
        }
    };

    /* New line of new property just started. Skipping whitespaces. */
    class Initial_state: public State {
        virtual bool
        On_char(int &c, std::unique_ptr<State> &next_state) override
        {
            /* Can safely terminate on EOF. */
            if (c == std::istream::traits_type::eof()) {
                return true;
            }
            /* Skip whitespaces. */
            if (Is_whitespace(c)) {
                c = CHAR_CONSUMED;
                return false;
            }
            /* Detect comments. */
            if (c == '!' || c == '#') {
                next_state = Ptr(new Comment_state());
                return true;
            }
            /* New line does nothing. */
            if (c == '\n' || c == '\r') {
                Set_substate(Ptr(new Line_terminator_state()));
                return false;
            }
            /* Key-value string started. */
            next_state = Ptr(new Key_value_state());
            return true;
        }
    };

    class Position_tracker {
    public:
        /* Feed character to the tracker.
         * @param trailing Indicates whether it is next parsed character or one
         *      from trailing line after exception.
         * @return true if need more characters to display position information.
         */
        bool
        Feed(int c, bool trailing = false)
        {
            if (c == '\t' || c == '\f') {
                c = ' ';
            }
            if (trailing) {
                if (c == '\r' || c == '\n' || c == std::istream::traits_type::eof()) {
                    return false;
                }
                cur_line += c;
            } else if (c != std::istream::traits_type::eof()) {
                if (c == '\r' || c == '\n') {
                    cur_line.clear();
                    col_idx = 0;
                    line_idx++;
                } else {
                    cur_line += c;
                    col_idx++;
                }
            }
            return true;
        }

        std::string
        Get_position() const
        {
            std::stringstream result;
            result << "Line " << line_idx << " column " << col_idx << ":\n";
            result << cur_line << '\n';
            for (size_t idx = 0; idx + 1 < col_idx; idx++) {
                result << '-';
            }
            result << '^';
            return result.str();
        }

    private:
        std::string cur_line;
        size_t line_idx = 1, col_idx = 0;
    };

    State::Ptr cur_state;
    int original_c, pocessed_c;
    Position_tracker pos_tracker;
    std::string cur_description;

    while (true) {
        original_c = stream.get();
        pocessed_c = original_c;
        pos_tracker.Feed(original_c);
        State::Ptr next_state;
        try {
            do {
                if (!cur_state) {
                    cur_state = State::Ptr(new Initial_state());
                }
                if (cur_state->Feed(pocessed_c, next_state)) {
                    Token token;
                    cur_state->Get_token(token);
                    if (token.type == Token::Type::PROPERTY) {
                        /* New property parsed, check for duplicate and add. */
                        if (table.find(token.v_property.key) != table.end()) {
                            VSM_EXCEPTION(Parse_exception, "Duplicated entry: %s",
                                          token.v_property.key.c_str());
                        }
                        auto result = table.insert(std::pair<std::string, Property>(
                                std::move(token.v_property.key),
                                Property(std::move(token.v_property.value))));
                        result.first->second.seq_number = last_sequence_number++;
                        result.first->second.description = cur_description;
                        cur_description.clear();
                    }
                    cur_state = std::move(next_state);
                }
                /* End-of-file encountered. */
                if (pocessed_c == std::istream::traits_type::eof()) {
                    if (cur_state) {
                        VSM_EXCEPTION(Parse_exception, "Unexpected end of stream");
                    } else {
                        break;
                    }
                }
            } while (pocessed_c != CHAR_CONSUMED);
        } catch(Parse_exception &e) {
            while (pos_tracker.Feed(stream.get(), true)) {}
            LOG_WARNING("Exception thrown during properties parsing:\n%s\n%s",
                        e.what(), pos_tracker.Get_position().c_str());
            throw;
        }
        if (original_c == std::istream::traits_type::eof()) {
            break;
        } else {
            if (cur_state) {
                Token token;
                cur_state->Get_token(token);
                if (token.type != Token::Type::PROPERTY) {
                    cur_description += original_c;
                }
            } else {
                cur_description += original_c;
            }
        }
    }
    trailer = cur_description;
}

const Properties::Property &
Properties::Find_property(const std::string &key) const
{
    auto it = table.find(key);
    if (it == table.end()) {
        VSM_EXCEPTION(Not_found_exception, "Specified key not found: %s",
                      key.c_str());
    }
    return it->second;
}

Properties::Property *
Properties::Find_property(const std::string &key)
{
    auto it = table.find(key);
    if (it == table.end()) {
        return nullptr;
    }
    return &it->second;
}

std::string
Properties::Get(const std::string &key) const
{
    const Property &prop = Find_property(key);
    return prop.str_repr;
}

/** Check if the property with the specified key exists. */
bool
Properties::Exists(const std::string &key) const
{
    return const_cast<Properties *>(this)->Find_property(key) != nullptr;
}

int32_t
Properties::Get_int(const std::string &key) const
{
    const Property &prop = Find_property(key);
    if (!prop.int_valid) {
        VSM_EXCEPTION(Not_convertible_exception,
                      "Property value '%s' cannot be represented as integer value",
                      prop.str_repr.c_str());
    }
    return prop.int_repr;
}

double
Properties::Get_float(const std::string &key) const
{
    const Property &prop = Find_property(key);
    if (!prop.float_valid) {
        VSM_EXCEPTION(Not_convertible_exception,
                      "Property value '%s' cannot be represented as floating point number value",
                      prop.str_repr.c_str());
    }
    return prop.float_repr;
}

void
Properties::Delete(const std::string &key)
{
    auto it = table.find(key);
    if (it == table.end()) {
        VSM_EXCEPTION(Not_found_exception, "Specified key not found: %s",
                      key.c_str());
    }
    table.erase(it);
}

namespace {
// do not make this public.
void
String_replace(std::string& str, const std::string& search, const std::string& replace)
{
    for (size_t pos = 0; ; pos += replace.length()) {
        pos = str.find(search, pos);
        if (pos == std::string::npos) {
            break;
        }
        str.erase(pos, search.length());
        str.insert(pos, replace);
    }
}
} // namespace

void
Properties::Set_description(const std::string &key, const std::string &desc)
{
    Property *prop = Find_property(key);
    if (prop == nullptr) {
        prop = &table.insert(std::pair<std::string, Property>(key, std::string(""))).first->second;
        prop->seq_number = last_sequence_number++;
    }
    prop->description = LINE_TERMINATOR + desc;
    String_replace(prop->description, LINE_TERMINATOR, LINE_TERMINATOR "# ");
    prop->description += LINE_TERMINATOR;
}

void
Properties::Set(const std::string &key, const std::string &value)
{
    Property *prop = Find_property(key);
    if (prop) {
        *prop = Property(std::string(value));
    } else {
        auto result = table.insert(std::pair<std::string, Property>(key, std::string(value)));
        result.first->second.seq_number = last_sequence_number++;
    }
}

void
Properties::Set(const std::string &key, int32_t value)
{
    Property *prop = Find_property(key);
    if (prop) {
        *prop = Property(value);
    } else {
        auto result = table.insert(std::pair<std::string, Property>(key, value));
        result.first->second.seq_number = last_sequence_number++;
    }
}

void
Properties::Set(const std::string &key, double value)
{
    Property *prop = Find_property(key);
    if (prop) {
        *prop = Property(value);
    } else {
        auto result = table.insert(std::pair<std::string, Property>(key, value));
        result.first->second.seq_number = last_sequence_number++;
    }
}

std::string
Properties::Escape(const std::string &str, bool is_key)
{
    std::string result;
    for (int c : str) {
        switch (c) {
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\\':
            result += "\\\\";
            break;
        default:
            if (is_key) {
                switch (c) {
                case ' ':
                    result += "\\ ";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                case '\f':
                    result += "\\f";
                    break;
                case '=':
                case ':':
                    result += '\\';
                    result += c;
                    break;
                default:
                    result += c;
                }
            } else {
                result += c;
            }
        }
    }
    return result;
}

void
Properties::Store(std::ostream &stream)
{
    /* Want sorted by seq_ids. */
    std::map<int, std::string> map;
    for (auto pair : table) {
        map.insert({pair.second.seq_number, pair.first});
    }
    for (auto pair : map) {
        auto item = table.find(pair.second);
        stream << item->second.description;
        stream << Escape(item->first, true);
        if (item->second.str_repr.size()) {
            stream << " = " << Escape(item->second.str_repr);
        }
    }
    stream << trailer;
}

void
Properties::Iterator::_NextProp()
{
    if (prefix.empty()) {
        return;
    }
    size_t prefix_len = prefix.size();
    while (table_iterator != table_end) {
        if (table_iterator->first.size() >= prefix_len &&
            !table_iterator->first.compare(0, prefix_len, prefix, 0, prefix_len)) {
            /* Match found. */
            break;
        }
        table_iterator++;
    }
}

void
Properties::Iterator::operator ++()
{
    if (table_iterator == table_end) {
        VSM_EXCEPTION(Internal_error_exception, "Iterated past the end");
    }
    table_iterator++;
    _NextProp();
}

int
Properties::Iterator::Get_count()
{
    int ret = 0;
    if (table_iterator != table_end) {
        const std::string &s = table_iterator->first;
        for (size_t cur_idx = 0; cur_idx != std::string::npos; cur_idx = s.find(separator, cur_idx)) {
            cur_idx++;
            ret++;
        }
    }
    return ret;
}

std::string
Properties::Iterator::operator[](size_t comp_idx)
{
    if (table_iterator == table_end) {
        VSM_EXCEPTION(Internal_error_exception, "Accessing end iterator");
    }
    const std::string &s = table_iterator->first;
    size_t cur_idx = 0;
    while (comp_idx) {
        cur_idx = s.find(separator, cur_idx);
        if (cur_idx == std::string::npos) {
            VSM_EXCEPTION(Invalid_param_exception, "Component index out of range");
        }
        cur_idx++;
        comp_idx--;
    }
    /* Until next separator. */
    size_t end_idx = s.find(separator, cur_idx);
    if (end_idx == std::string::npos) {
        return s.substr(cur_idx);
    }
    return s.substr(cur_idx, end_idx - cur_idx);
}
