// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/* HTTP parser implementation.
 * Not a complete implementation, yet. Only the things needed for SSDP for now.
 * See .h for details.
 */

#include <ugcs/vsm/http_parser.h>
#include <ugcs/vsm/platform.h>
#include <ugcs/vsm/debug.h>

#include <climits>
#include <cmath>
#include <functional>
#include <memory>
#include <sstream>
#include <map>
#include <algorithm>

using namespace ugcs::vsm;

std::string
Http_parser::Get_header_value(const std::string &name) const
{
    auto it = header_table.find(name);
    if (it != header_table.end()) {
        return it->second;
    } else {
        return "";
    }
}

std::string
Http_parser::Get_method() const
{
    return http_method;
}

namespace {
// Valid characters of HTTP token. See rfc7230.
bool
Is_token_char(int c)
{
    return      std::isalnum(c)
            ||  c == '!'
            ||  c == '#'
            ||  c == '$'
            ||  c == '%'
            ||  c == '&'
            ||  c == '\''
            ||  c == '*'
            ||  c == '+'
            ||  c == '-'
            ||  c == '.'
            ||  c == '^'
            ||  c == '_'
            ||  c == '`'
            ||  c == '|'
            ||  c == '~'
            ||  c == '$';
}

/* State machine. */

/* Token value available as a result from state handler. */
class Token {
public:
    /* Token type. */
    enum class Type {
        /* Empty token. */
        NONE,
        /* Empty token. */
        EOL,
        /* HTTP method. */
        METHOD,
        /* HTTP result code. */
        RESULT_CODE,
        /* HTTP result code. */
        PROTO_VERSION,
        /* URL. */
        URL,
        /* Characters string. */
        STRING,
        /* Full property. */
        HEADER
    };

    /* Token type. */
    Type type = Type::NONE;
    /* String value. */
    std::string string_1;
    std::string string_2;

    Token() = default;

    Token(Token &&token):
        type(token.type),
        string_1(std::move(token.string_1)),
        string_2(std::move(token.string_2))
    {
        token.type = Type::NONE;
    }

    void
    Set()
    {
        type = Type::EOL;
    }

    void
    Set(const std::string &str, Type t)
    {
        type = t;
        string_1 = str;
    }

    void
    Set(const std::string &key, const std::string &value)
    {
        type = Type::HEADER;
        string_1 = key;
        string_2 = value;
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
    virtual void
    Get_token(Token &token) override
    {
        token.Set();
    }
};

/* Skip space. */
class Skip_space: public State {
    virtual bool
    On_char(int &c, std::unique_ptr<State> &) override
    {
        if (std::isblank(c)) {
            c = CHAR_CONSUMED;
            return false;
        }
        return true;
    }
};

/* Skip spaces and newlines */
class Skip_space_and_eol: public State {
    virtual bool
    On_char(int &c, std::unique_ptr<State> &) override
    {
        if (std::isblank(c) || c == '\n' || c == '\r') {
            c = CHAR_CONSUMED;
            return false;
        }
        return true;
    }
};

/* Read everything until eol.*/
class Read_until_eol: public State {
    bool terminate_at_space;

public:
    Read_until_eol(bool terminate_at_space = false)
    :terminate_at_space(terminate_at_space )
    {
        Set_substate(Ptr(new Skip_space()));
    }
    std::string value;
    virtual bool
    On_char(int &c, std::unique_ptr<State> &) override
    {
        /* New line terminates the value. */
        if (c == '\n' || c == '\r') {
            Set_substate(Ptr(new Line_terminator_state()));
            return false;
        }
        if (terminate_at_space && std::isblank(c)) {
            return true;
        }
        if (std::isprint(c)) {
            value += c;
            c = CHAR_CONSUMED;
            return false;
        }
        c = std::istream::traits_type::eof();
        return true;
    }
    virtual void
    Get_token(Token &token) override
    {
        auto lastchar = value.find_last_not_of(" \t");
        if (lastchar == std::string::npos) {
            token.Set("", Token::Type::STRING);
        } else {
            token.Set(value.erase(lastchar + 1), Token::Type::STRING);
        }
    }
};

/* New line of headers just started.*/
class Read_header: public State {
    std::string name;
    std::string value;
    virtual bool
    On_char(int &c, std::unique_ptr<State> &) override
    {
        /* Must start with token_char */
        if (Is_token_char(c)) {
            name += c;
            c = CHAR_CONSUMED;
            return false;
        }
        if (c == ':' && !name.empty()) {
            c = CHAR_CONSUMED;
            Set_substate(Ptr(new Read_until_eol()));
            return false;
        }
        if (c == '\n' || c == '\r') {
            Set_substate(Ptr(new Line_terminator_state()));
            return false;
        } else {
            c = std::istream::traits_type::eof();
            return true;
        }
    }

    virtual bool
    On_token(Token &&token, std::unique_ptr<State> &next_state)
    {
        if (token.type == Token::Type::EOL) {
            // double CRLF. no more headers. exit.
            return true;
        }
        if (token.type == Token::Type::STRING) {
            value = token.string_1;
            next_state = Ptr(new Read_header());
        }
        return true;
    }
    virtual void
    Get_token(Token &token) override
    {
        if (!name.empty()) {
            token.Set(name, value);
        }
    }
};

// Request. Do not care about url, proto/version for now.
class Request_state: public State {
public:
    Request_state() {
        Set_substate(Ptr(new Read_until_eol()));
    }
    virtual bool
    On_token(Token &&, std::unique_ptr<State> &next_state)
    {
        next_state = Ptr(new Read_header());
        return true;
    }
};

// Response. Ignore proto version and return code for now.
class Response_state: public State {
public:
    Response_state() {
        Set_substate(Ptr(new Read_until_eol()));
    }
    virtual bool
    On_token(Token &&, std::unique_ptr<State> &next_state)
    {
        next_state = Ptr(new Read_header());
        return true;
    }
};

/* First line should contain method.*/
class First_line_state: public State {
    std::string method;
    bool is_request = false;

public:
    First_line_state() {
        Set_substate(Ptr(new Skip_space_and_eol()));
    }
    virtual bool
    On_char(int &c, std::unique_ptr<State> &next_state) override
    {
        if (Is_token_char(c)) {
            method += std::toupper(c);
            c = CHAR_CONSUMED;
            return false;
        }
        if (method == "HTTP" && c == '/') {
            next_state = Ptr(new Response_state());
            c = CHAR_CONSUMED;
        } else if (!method.empty() && std::isblank(c)) {
            is_request = true;
            next_state = Ptr(new Request_state());
            c = CHAR_CONSUMED;
        } else {
            // neither request nor response
            c = std::istream::traits_type::eof();
        }
        return true;
    }
    virtual void
    Get_token(Token &t) override
    {
        if (!method.empty() && is_request) {
            t.Set(method, Token::Type::METHOD);
        }
    }
};

} // namespace

bool
Http_parser::Parse(std::istream &stream)
{
    State::Ptr cur_state = State::Ptr(new First_line_state());
    header_table.clear();
    http_method.clear();
    int ch;
    do {
        ch = stream.get();
        State::Ptr next_state;
        do {
            if (cur_state->Feed(ch, next_state)) {
                Token token;
                cur_state->Get_token(token);
                switch (token.type) {
                case Token::Type::HEADER:
//                    LOG("%s='%s'", token.string_1.c_str(), token.string_2.c_str());
                    header_table.insert({
                            std::move(token.string_1),
                            std::move(token.string_2)});
                    break;
                case Token::Type::METHOD:
//                    LOG("method='%s'", token.string_1.c_str());
                    http_method = token.string_1;
                    break;
                default:
                    break;
                }
                cur_state = std::move(next_state);
            }
        } while (cur_state && ch != CHAR_CONSUMED);
    } while (cur_state);
    return (ch == CHAR_CONSUMED);
}
