#include <UnitTest++.h>
#include <ugcs/vsm/endian.h>

using namespace ugcs::vsm;

TEST(basic)
{
    uint8_t le_value[] = {0, 0};
    uint8_t be_value[] = {0, 0};
    Be_uint16::Set(be_value, 0x0201);
    Le_uint16::Set(le_value, 0x0201);

    CHECK_EQUAL(0x01, le_value[0]);
    CHECK_EQUAL(0x02, le_value[1]);
    CHECK_EQUAL(0x02, be_value[0]);
    CHECK_EQUAL(0x01, be_value[1]);

    CHECK_EQUAL(0x0201, Be_uint16::Get(be_value));
    CHECK_EQUAL(0x0201, Le_uint16::Get(le_value));
}
