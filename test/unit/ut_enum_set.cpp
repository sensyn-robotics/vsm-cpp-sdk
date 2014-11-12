#include <UnitTest++.h>
#include <ugcs/vsm/enum_set.h>

using namespace ugcs::vsm;

enum class Enum1 {
    E1,
    E2,
    E3,
    LAST
};

enum Enum2 {
    ENUM2_E1,
    ENUM2_E2,
    ENUM2_E3,
    LAST
};

TEST(empty)
{
    Enum_set<Enum1> e1;
    Enum_set<Enum2> e2;

    CHECK(!e1.Is_set(Enum1::E1));
    CHECK(!e2.Is_set(Enum2::ENUM2_E1));
}

TEST(construct_one)
{
    Enum_set<Enum1> e1(Enum1::E2);
    Enum_set<Enum2> e2(Enum2::ENUM2_E3);

    CHECK(!e1.Is_set(Enum1::E1));
    CHECK(!e2.Is_set(Enum2::ENUM2_E1));
    CHECK(e1.Is_set(Enum1::E2));
    CHECK(e2.Is_set(Enum2::ENUM2_E3));
}

TEST(construct_two)
{
    Enum_set<Enum1> e1(Enum1::E2, Enum1::E3);
    Enum_set<Enum2> e2(Enum2::ENUM2_E3, Enum2::ENUM2_E1);

    CHECK(!e1.Is_set(Enum1::E1));
    CHECK(!e2.Is_set(Enum2::ENUM2_E2));
    CHECK(e1.Is_set(Enum1::E2));
    CHECK(e1.Is_set(Enum1::E3));
    CHECK(e2.Is_set(Enum2::ENUM2_E3));
    CHECK(e2.Is_set(Enum2::ENUM2_E1));
}

TEST(equality)
{
    {
        Enum_set<Enum1> e1(Enum1::E2, Enum1::E3);
        Enum_set<Enum1> e2(Enum1::E2, Enum1::E3);
        CHECK(e1 == e2);
    }

    {
        Enum_set<Enum1> e1(Enum1::E3, Enum1::E2);
        Enum_set<Enum1> e2(Enum1::E2, Enum1::E3);
        CHECK(e1 == e2);
        e1.Set(Enum1::E3);
        CHECK(e1 == e2);
        e2.Set(Enum1::E1);
        CHECK(!(e1 == e2));
        e1.Set(Enum1::E1);
        CHECK(e1 == e2);
        e1.Set(Enum1::E1, false);
        CHECK(!(e1 == e2));
        e1.Set(Enum1::E1);
        CHECK(e1 == e2);
    }
}
