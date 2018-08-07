// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Tests for Properties class.
 */

#include <ugcs/vsm/properties.h>
#include <ugcs/vsm/platform.h>

#include <UnitTest++.h>

#include <list>

using namespace ugcs::vsm;

TEST(basic_functionality)
{
    const char *initial_data =
        "   \n"
        "\n"
        "\r\n"
        "# comment\n"
        "! comment\r"
        "# comment\r\n"
        "  \t\f  # comment\r\n"
        "key1 = value1\n"
        "key2   =   value2\n"
        "    \t\fkey3    \t\f value3\n"
        "    key4\n"
        "key5   =  \n"
        "key6  :   \t\f\n"
        "key7\\=\n"
        "key8\\:\r"
        "key9\\ \\n\\r\\\t\\\\   value9 \n"
        "key10 = \\u0030\r\n"
        "key11==value11\n"
        "key12::value12\n"
        "key13 value 1 2 3\n"
        "key14 value 1 2 3 \\\n"
        "      4 5 \\\n"
        "      6 7\n"
        "key15 value 1 2 \\\r\n"
        "   \t 3 4\n"

        "last_key = last_value";

    Properties props;
    std::istringstream buf(initial_data);
    props.Load(buf);

    CHECK_EQUAL("value1", props.Get("key1"));
    CHECK_EQUAL("value2", props.Get("key2"));
    CHECK_EQUAL("value3", props.Get("key3"));
    CHECK_EQUAL("", props.Get("key4"));
    CHECK_EQUAL("", props.Get("key5"));
    CHECK_EQUAL("", props.Get("key6"));
    CHECK_EQUAL("", props.Get("key7="));
    CHECK_EQUAL("", props.Get("key8:"));
    CHECK_EQUAL("value9 ", props.Get("key9 \n\r\t\\"));
    CHECK_EQUAL("\x30", props.Get("key10"));
    CHECK_EQUAL("=value11", props.Get("key11"));
    CHECK_EQUAL(":value12", props.Get("key12"));
    CHECK_EQUAL("value 1 2 3", props.Get("key13"));
    CHECK_EQUAL("value 1 2 3 4 5 6 7", props.Get("key14"));
    CHECK_EQUAL("value 1 2 3 4", props.Get("key15"));

    CHECK_EQUAL("last_value", props.Get("last_key"));

    CHECK_THROW(props.Get("non existing"), Properties::Not_found_exception);
    CHECK_THROW(props.Delete("non existing"), Properties::Not_found_exception);
    props.Delete("key1");
    CHECK_THROW(props.Get("key1"), Properties::Not_found_exception);

    props.Set("key1", "2.25");
    CHECK_EQUAL("2.25", props.Get("key1"));
    CHECK_EQUAL(2, props.Get_int("key1"));
    CHECK_EQUAL(2.25, props.Get_float("key1"));

    props.Set("key1", "5.25");
    CHECK_EQUAL("5.25", props.Get("key1"));
    CHECK_EQUAL(5, props.Get_int("key1"));
    CHECK_EQUAL(5.25, props.Get_float("key1"));

    props.Set("key1", 4);
    CHECK_EQUAL("4", props.Get("key1"));
    CHECK_EQUAL(4, props.Get_int("key1"));
    CHECK_EQUAL(4.0, props.Get_float("key1"));

    props.Set("key1", 3.25);
    CHECK_EQUAL("3.250000", props.Get("key1"));
    CHECK_EQUAL(3, props.Get_int("key1"));
    CHECK_EQUAL(3.25, props.Get_float("key1"));
}

void
Check_invalid(const char *input, const char *exception_text)
{
    Properties props;
    std::istringstream buf(input);
    bool caught = false;
    try {
        props.Load(buf);
    } catch (Properties::Parse_exception &e) {
        caught = true;
        CHECK(std::string(e.what()).find(exception_text) != std::string::npos);
    }
    CHECK(caught);
}

TEST(parser_exceptions)
{
    Check_invalid("   = value", "Empty key name");
    Check_invalid("key = \\=", "Invalid escape character");
    Check_invalid("\\g = value", "Invalid escape character");
    Check_invalid("key = \\u000z", "Invalid digit in unicode escape");
    Check_invalid("key = \\u00", "Unexpected EOF - unclosed escape");
    Check_invalid("key\\", "Unexpected EOF - unclosed escape");
}

TEST(conversions)
{
    const char *initial_data =
        "i = 10\n"
        "f = 2.25\n"
        "s = some string";

    Properties props;
    std::istringstream buf(initial_data);
    props.Load(buf);

    CHECK_EQUAL(10, props.Get_int("i"));
    CHECK_EQUAL(2, props.Get_int("f"));
    CHECK_EQUAL(2.25, props.Get_float("f"));
    CHECK_THROW(props.Get_int("s"), Properties::Not_convertible_exception);
    CHECK_THROW(props.Get_float("s"), Properties::Not_convertible_exception);
}

TEST(storing)
{
    const char *initial_data =
        "a = 10" LINE_TERMINATOR
        "b = 2.25" LINE_TERMINATOR
        "c = some string" LINE_TERMINATOR
        "d\\=\\:\\r\\n\\t\\f\\\\ = a\t\fb\\\\" LINE_TERMINATOR;
    std::istringstream buf(initial_data);
    Properties props;
    props.Load(buf);
    std::ostringstream out;
    props.Store(out);
    CHECK_EQUAL(initial_data, out.str().c_str());
}

TEST(iterations)
{
    const char *initial_data =
        "a = 1" LINE_TERMINATOR
        "a.1 = 1.1" LINE_TERMINATOR
        "a.2 = 1.2" LINE_TERMINATOR
        "a.2.1 = 1.2.1" LINE_TERMINATOR
        "a.2.2 = 1.2.2" LINE_TERMINATOR
        "b = 2" LINE_TERMINATOR
        "b.1 = 2.1" LINE_TERMINATOR
        "c = 3" LINE_TERMINATOR
        "d = 4" LINE_TERMINATOR;

    std::istringstream buf(initial_data);
    Properties props;
    props.Load(buf);

    {
        std::list<const char *> names =
            {"a", "a.1", "a.2", "a.2.1", "a.2.2", "b", "b.1", "c", "d"};

        auto expected = names.begin();
        for (auto prop: props) {
            CHECK_EQUAL(*expected, prop.c_str());
            expected++;
        }
        CHECK(expected == names.end());
    }

    {
        std::list<const char *> names =
            {"a.1", "a.2", "a.2.1", "a.2.2"};
        auto expected = names.begin();
        for (auto it = props.begin("a."); it != props.end(); it++) {
            CHECK_EQUAL(*expected, (*it).c_str());
            expected++;
        }
        CHECK(expected == names.end());
    }

    {
        std::list<const char *> names =
            {"a.2.1", "a.2.2"};
        auto expected = names.begin();
        for (auto it = props.begin("a.2."); it != props.end(); it++) {
            CHECK_EQUAL(*expected, (*it).c_str());
            if (!(*it).compare("a.2.1")) {
                CHECK_EQUAL("a", it[0]);
                CHECK_EQUAL("2", it[1]);
                CHECK_EQUAL("1", it[2]);
                CHECK_THROW(it[3], Invalid_param_exception);
            }
            expected++;
        }
        CHECK(expected == names.end());
    }
}
