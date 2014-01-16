// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <vsm/adsb_frame.h>
#include <vsm/coordinates.h>
#include <cstring>
#include <sstream>

using namespace vsm;

constexpr size_t Adsb_frame::SIZE;

Adsb_frame::ICAO_address::ICAO_address(
        Type type, uint8_t high, uint8_t middle, uint8_t low) :
                type(type), address(low | middle << 8 | high << 16)
{

}

std::string
Adsb_frame::ICAO_address::To_hex_string() const
{
    std::ostringstream value;
    value << std::hex << std::uppercase << address;
    if (type == Type::REAL) {
        value << "(REAL)";
    } else {
        value << "(ANON)";
    }
    return value.str();
}

uint32_t
Adsb_frame::ICAO_address::Get_address() const
{
    return address;
}

Adsb_frame::ICAO_address::Type
Adsb_frame::ICAO_address::Get_type() const
{
    return type;
}

bool
Adsb_frame::ICAO_address::operator ==(const ICAO_address& other) const
{
    return address == other.address && type == other.type;
}

bool
Adsb_frame::ICAO_address::operator !=(const ICAO_address& other) const
{
    return address != other.address || type != other.type;
}

Adsb_frame::Adsb_frame(Io_buffer::Ptr buffer) :
        received(std::chrono::steady_clock::now())
{
    static_assert(sizeof(Adsb_frame::Frame) == 14,
                  "Should be 14 bytes for extended squitter ADSB-B frames");

    if (buffer->Get_length() != SIZE) {
        VSM_EXCEPTION(Invalid_buffer,
                      "wrong ADS-B buffer size (%ld", buffer->Get_length());
    }
    frame = buffer;
    frame_ptr = static_cast<const Frame*>(frame->Get_data());
}

bool
Adsb_frame::Verify_checksum() const
{
    uint32_t calc_value = Calculate_crc(reinterpret_cast<const uint8_t*>(frame_ptr));
    uint8_t PI[4] = {0, 0, 0, 0};
    std::memcpy(&PI[1], frame_ptr->PI, sizeof(Frame::PI));
    Be_uint32* orig_value = reinterpret_cast<Be_uint32*>(PI);
    return calc_value == (*orig_value);
}

std::chrono::steady_clock::time_point
Adsb_frame::Get_received_time() const
{
    return received;
}

uint8_t
Adsb_frame::Get_DF() const
{
    return (frame_ptr->first_byte & 0xF8) >> 3;
}

uint8_t
Adsb_frame::Get_CA() const
{
    ASSERT(Get_DF() == Downlink_format::DF_17);
    return Get_CA_CF_AF();
}

uint8_t
Adsb_frame::Get_CF() const
{
    ASSERT(Get_DF() == Downlink_format::DF_18);
    return Get_CA_CF_AF();
}

uint8_t
Adsb_frame::Get_AF() const
{
    ASSERT(Get_DF() == Downlink_format::DF_19);
    return Get_CA_CF_AF();
}

uint8_t
Adsb_frame::Get_ME_type() const
{
    return (frame_ptr->ME[0] & 0xF8) >> 3;
}

uint8_t
Adsb_frame::Get_ME_subtype() const
{
    return (frame_ptr->ME[0] & 0x07);
}

bool
Adsb_frame::Is_rebroadcast() const
{
    return Get_DF() == Downlink_format::DF_18 && Get_CF() == CF_values::CF_6;
}

uint32_t
Adsb_frame::Calculate_crc(const uint8_t* in)
{
    uint32_t poly = 0xFFFA0480;

    uint32_t data = in[0] << 24 | in[1] << 16 | in[2] << 8 | in[3];
    uint32_t data1 = in[4] << 24 | in[5] << 16 | in[6] << 8 | in[7];
    uint32_t data2 = in[8] << 24 | in[9] << 16 | in[10] << 8;

    for (int i = 1; i <= 88; i++) {
        if (data & 0x80000000) {
            data ^= poly;
        }
        data <<= 1;
        if (data1 & 0x80000000) {
            data |= 1;
        }
        data1 <<= 1;
        if (data2 & 0x80000000) {
            data1 |= 1;
        }
        data2 <<= 1;
    }
    return data >> 8;
}

uint8_t
Adsb_frame::Get_CA_CF_AF() const
{
    return frame_ptr->first_byte & 0x07;
}

uint16_t
Adsb_frame::Gray_to_value(uint16_t gray)
{
    gray ^= (gray >> 8);
    gray ^= (gray >> 4);
    gray ^= (gray >> 2);
    gray ^= (gray >> 1);
    return gray;
}

Adsb_frame::ICAO_address
Adsb_frame::Get_ICAO_address(ICAO_address::Type type) const
{
    return ICAO_address(type,
                        frame_ptr->address[0],
                        frame_ptr->address[1],
                        frame_ptr->address[2]);
}

Adsb_frame::ME_message::ME_message(
        Adsb_frame::Ptr frame) : frame(frame)
{
}

Adsb_frame::ME_message::~ME_message()
{
}

bool
Adsb_frame::ME_message::Is_empty() const
{
    return frame == nullptr;
}

uint8_t
Adsb_frame::ME_message::Get_ME_type() const
{
    return frame->Get_ME_type();
}

Adsb_frame::ICAO_address
Adsb_frame::ME_message::Get_ICAO_address() const
{
    ICAO_address::Type type;

    if (frame->Is_rebroadcast()) {
        /* IMF is applicable to rebroadcasted messages. */
        type = imf ? ICAO_address::Type::ANONYMOUS : ICAO_address::Type::REAL;
    } else {
        type = ICAO_address::Type::REAL;
    }

    return frame->Get_ICAO_address(type);
}

std::chrono::steady_clock::time_point
Adsb_frame::ME_message::Get_received_time() const
{
    return frame->Get_received_time();
}

Adsb_frame::Position_message::Position_message(Adsb_frame::Ptr frame) :
        ME_message(frame)
{
}

bool
Adsb_frame::Position_message::Get_time_IMF() const
{
    return (frame->frame_ptr->ME[2] & 0x08);
}

bool
Adsb_frame::Position_message::Get_CPR_format() const
{
    return (frame->frame_ptr->ME[2] & 0x04);
}

uint32_t
Adsb_frame::Position_message::Get_CPR_latitude() const
{
    uint32_t lat = frame->frame_ptr->ME[2] & 0x03; /* Highest 2 bits. */
    lat <<= 8;
    lat |= frame->frame_ptr->ME[3]; /* Next 8 bits. */
    lat <<= 7;
    lat |= frame->frame_ptr->ME[4] >> 1; /* Lowest 7 bits. */

    return lat;
}

uint32_t
Adsb_frame::Position_message::Get_CPR_longitude() const
{
    uint32_t lon = frame->frame_ptr->ME[4] & 0x01; /* Highest bit. */
    lon <<= 8;
    lon |= frame->frame_ptr->ME[5]; /* Next 8 bits. */
    lon <<= 8;
    lon |= frame->frame_ptr->ME[6]; /* Lowest 8 bits. */

    return lon;
}

Adsb_frame::Airborne_position_message::Airborne_position_message(
        Adsb_frame::Ptr frame) :
                Position_message(frame)
{
    if (frame->Is_rebroadcast()) {
        imf = Get_NIC_supplement_IMF();
    }
}

uint8_t
Adsb_frame::Airborne_position_message::Get_surveillance_status() const
{
    return (frame->frame_ptr->ME[0] & 0x06) >> 1;
}

bool
Adsb_frame::Airborne_position_message::Get_NIC_supplement_IMF() const
{
    return (frame->frame_ptr->ME[0] & 0x1);
}

bool
Adsb_frame::Airborne_position_message::Is_altitude_available() const
{
    return Get_altitude_raw() != 0;
}

int32_t
Adsb_frame::Airborne_position_message::Get_altitude(bool in_feet) const
{
    uint16_t value = Get_altitude_raw();
    if (value & 0x10) {
        uint16_t low = value & 0xf;
        value &= 0xFFE0;
        value = (value >> 1) | low;
        return (in_feet ? 1 : FEET_TO_M) * (25 * value - 1000);
    } else {
        /* Decode pulse position Gillham altitude code, which is basically
         * a combination of pure Gray encoding for 500ft increments and
         * modified Gray encoding for 100ft increments. */

        /* First, decode Gray code part for 500ft increment,
         * bits D2 D4 A1 A2 A4 B1 B2 B4
         */
        uint16_t gray500 = 0;
        gray500 |= (value & 0x002) >> 1; /* B4 */
        gray500 |= (value & 0x008) >> 2; /* B2 */
        gray500 |= (value & 0x020) >> 3; /* B1 */
        gray500 |= (value & 0x040) >> 3; /* A4 */
        gray500 |= (value & 0x100) >> 4; /* A2 */
        gray500 |= (value & 0x400) >> 5; /* A1 */
        gray500 |= (value & 0x001) << 6; /* D4 */
        gray500 |= (value & 0x004) << 5; /* D2 */

        gray500 = frame->Gray_to_value(gray500);

        /* Decode reflected Gray code for 100ft increment,
         * bits C1 C2 C4
         */
        uint16_t gray100 = 0;
        gray100 |= (value & 0x080) >> 7;  /* C4 */
        gray100 |= (value & 0x200) >> 8;  /* C2 */
        gray100 |= (value & 0x800) >> 9;  /* C1 */

        gray100 = frame->Gray_to_value(gray100);

        /* 7 switched to 5 as per specification to have a continuous
         * sequence of 1 2 3 4 5 instead of 1 2 3 4 7.
         */
        if (gray100 == 7) {
            gray100 = 5;
        }

        if (gray500 & 0x1) {
            /* Uneven 500ft increment values indicate reflected Gray code. */
            gray100 = 6 - gray100;
        }

        /* 100ft increments are corrected by 1 as per specification. */
        gray100 -= 1;

        /* Finally, basis is -1200ft. */
        return (in_feet ? 1 : FEET_TO_M) * (gray500 * 500 + gray100 * 100 - 1200);
    }

    return value;
}

uint16_t
Adsb_frame::Airborne_position_message::Get_altitude_raw() const
{
    return frame->frame_ptr->ME[1] * 16 + (frame->frame_ptr->ME[2] >> 4);
}

Adsb_frame::Surface_position_message::Surface_position_message(
        Adsb_frame::Ptr frame) :
                Position_message(frame)
{
    if (frame->Is_rebroadcast()) {
        imf = Get_time_IMF();
    }
}

bool
Adsb_frame::Surface_position_message::Is_speed_available() const
{
    return Get_movement_raw() && Get_movement_raw() < 125;
}

double
Adsb_frame::Surface_position_message::Get_speed() const
{
    ASSERT(Is_speed_available());
    uint8_t raw = Get_movement_raw();
    double speed = 0; /* Initially in Km/h */
    if (raw <= 1) {
        speed = 0;
    } else if (raw == 2) {
        speed = 0.2315;
    } else if (raw >= 3 && raw <= 8) {
        speed = (raw - 2) * 0.2700833 + 0.2315;
    } else if (raw >= 9 && raw <= 12) {
        speed = (raw - 8) * 0.463 + 1.852;
    } else if (raw >= 13 && raw <= 38) {
        speed = (raw - 12) * 0.926 + 3.704;
    } else if (raw >= 39 && raw <= 93) {
        speed = (raw - 38) * 1.852 + 27.78;
    } else if (raw >= 94 && raw <= 108) {
        speed = (raw - 93) * 3.704 + 129.64;
    } else if (raw >= 109 && raw <= 123) {
        speed = (raw - 108) * 9.26 + 185.2;
    } else if (raw == 124) {
        speed = 324.1;
    }

    /* Convert to m/s. */
    return (speed * 1000) / 3600;
}

bool
Adsb_frame::Surface_position_message::Is_heading_available() const
{
    return (frame->frame_ptr->ME[1] & 0x8);
}

double
Adsb_frame::Surface_position_message::Get_heading() const
{
    ASSERT(Is_heading_available());
    return Get_heading_raw() * (2 * M_PI / 128);
}

uint8_t
Adsb_frame::Surface_position_message::Get_heading_raw() const
{
    return ((frame->frame_ptr->ME[1] & 0x7) << 4) |
            (frame->frame_ptr->ME[2] >> 4);
}

uint8_t
Adsb_frame::Surface_position_message::Get_movement_raw() const
{
    return ((frame->frame_ptr->ME[0] & 0x7) << 4) |
            (frame->frame_ptr->ME[1] >> 4);
}

std::string
Adsb_frame::Aircraft_id_and_cat_message::Get_id() const
{
    std::string ident = "";
    static_assert(sizeof(uint64_t) >= ID_FIELD_LENGTH, "Fool check");
    uint8_t data[sizeof(uint64_t)];
    uint64_t* value = reinterpret_cast<uint64_t*>(data);
    *value = 0;
    /* Copy frame data to native data type starting from less significant side
     * so that last character is in the less significant side.
     */
    if (Is_system_le()) {
        /* On little endian system swap bytes to be in the right order. */
        for (int i = 0; i < ID_FIELD_LENGTH; i++) {
            data[i] = frame->frame_ptr->ME[ID_FIELD_LENGTH - i];
        }
    } else {
        /* On big endian system byte order is preserved, but 2 most significant
         * bytes are skipped, because frame has only 6 bytes with characters
         * data.
         */
        for (int i = 0; i < ID_FIELD_LENGTH; i++) {
            data[sizeof(uint64_t) - ID_FIELD_LENGTH + i] = frame->frame_ptr->ME[i + 1];
        }
    }
    for (int i = 0; i < NUM_OF_CHARS; i++) {
        ident = Id_to_char(*value & 0x3F) + ident;
        *value = *value >> 6;
    }
    return ident;
}

Adsb_frame::Aircraft_id_and_cat_message::Emitter_category
Adsb_frame::Aircraft_id_and_cat_message::Get_emitter_category() const
{
    switch (frame->Get_ME_type()) {
    case AIRCRAFT_ID_AND_CAT_A_4:
        switch (frame->Get_ME_subtype()) {
        case 0: return Emitter_category::NO_INFO_A;
        case 1: return Emitter_category::LIGHT;
        case 2: return Emitter_category::SMALL;
        case 3: return Emitter_category::LARGE;
        case 4: return Emitter_category::HIGH_VORTEX_LARGE;
        case 5: return Emitter_category::HEAVY;
        case 6: return Emitter_category::HIGH_PERFORMANCE;
        case 7: return Emitter_category::ROTORCRAFT;
        default: VSM_EXCEPTION(Internal_error_exception,
                "Value out of range (%d)",
                frame->Get_ME_subtype());
        }
    case AIRCRAFT_ID_AND_CAT_B_3:
        switch (frame->Get_ME_subtype()) {
        case 0: return Emitter_category::NO_INFO_B;
        case 1: return Emitter_category::GLIDER_SAILPLANE;
        case 2: return Emitter_category::LIGHTER_THAN_AIR;
        case 3: return Emitter_category::PARACHUTIST_SKYDIVER;
        case 4: return Emitter_category::ULTRALIGHT_PARAGLIDER;
        case 5: return Emitter_category::RESERVED;
        case 6: return Emitter_category::UAV;
        case 7: return Emitter_category::SPACE_TRANS_ATMOSPHERIC;
        default: VSM_EXCEPTION(Internal_error_exception,
                "Value out of range (%d)",
                frame->Get_ME_subtype());
        }
    case AIRCRAFT_ID_AND_CAT_C_2:
        switch (frame->Get_ME_subtype()) {
        case 0: return Emitter_category::NO_INFO_C;
        case 1: return Emitter_category::SURFACE_EMERGENCY;
        case 2: return Emitter_category::SURFACE_SERVICE;
        case 3: return Emitter_category::POINT_OBSTACLE;
        case 4: return Emitter_category::CLUSTER_OBSTACLE;
        case 5: return Emitter_category::LINE_OBSTACLE;
        case 6: return Emitter_category::RESERVED;
        case 7: return Emitter_category::RESERVED;
        default: VSM_EXCEPTION(Internal_error_exception,
                "Value out of range (%d)",
                frame->Get_ME_subtype());
        }
    case AIRCRAFT_ID_AND_CAT_D_1:
        switch (frame->Get_ME_subtype()) {
        case 0: return Emitter_category::NO_INFO_D;
        case 1: return Emitter_category::RESERVED;
        case 2: return Emitter_category::RESERVED;
        case 3: return Emitter_category::RESERVED;
        case 4: return Emitter_category::RESERVED;
        case 5: return Emitter_category::RESERVED;
        case 6: return Emitter_category::RESERVED;
        case 7: return Emitter_category::RESERVED;
        default: VSM_EXCEPTION(Internal_error_exception,
                "Value out of range (%d)",
                frame->Get_ME_subtype());
        }
    default: VSM_EXCEPTION(Invalid_format, "Unsupported ME type for aircraft id"
            " and category message (%d)", frame->Get_ME_type());
    }
}

char
Adsb_frame::Aircraft_id_and_cat_message::Id_to_char(uint8_t id)
{
    if ((id & 0x30) == 0x30) {
        /* Number. */
        id &= 0xf;
        if (id <= 9) {
            return '0' + id;
        } else {
            return '?';
        }
    } else if (id == 0x20) {
        /* Space. */
        return ' ';
    } else if ((id & 0x30) == 0x10) {
        /* P to Z */
        id &= 0xf;
        if (id <= 0xA) {
            return 'P' + id;
        } else {
            return '?';
        }
    } else if ((id & 0x30) == 0x00) {
        /* A to O */
        id &= 0xf;
        if (id >= 0x01) {
            return 'A' + id - 1;
        } else {
            return '?';
        }
    }
    return '?';
}

Adsb_frame::Airborne_velocity_message::Airborne_velocity_message(
        Adsb_frame::Ptr frame) :
                ME_message(frame)
{
    if (frame->Is_rebroadcast()) {
        imf = Get_intent_IMF();
    }
}

bool
Adsb_frame::Airborne_velocity_message::Get_intent_IMF() const
{
    return (frame->frame_ptr->ME[1] & 0x80);
}

bool
Adsb_frame::Airborne_velocity_message::Is_horizontal_speed_available() const
{
    if (Is_ground_speed_type()) {
        return Get_ew_velocity_heading() && Get_ns_velocity_airspeed();
    } else {
        return Get_ns_velocity_airspeed();
    }
}

double
Adsb_frame::Airborne_velocity_message::Get_horizontal_speed() const
{
    ASSERT(Is_horizontal_speed_available());

    if (Is_ground_speed_type()) {
        double ew = Get_ew_velocity_heading() - 1; /* In knots. */
        ew *= KNOTS_TO_MS;
        double ns = Get_ns_velocity_airspeed() - 1; /* In knots. */
        ns *= KNOTS_TO_MS;
        if (Is_supersonic()) {
            ew *= 4;
            ns *= 4;
        }
        return std::hypot(ew, ns);
    } else {
        double speed = Get_ns_velocity_airspeed() - 1; /* In knots. */
        speed *= KNOTS_TO_MS;
        if (Is_supersonic()) {
            speed *= 4;
        }
        return speed;
    }
}

bool
Adsb_frame::Airborne_velocity_message::Is_heading_available() const
{
    if (Is_ground_speed_type()) {
        return Is_horizontal_speed_available();
    } else {
        return Get_ew_direction_heading_status();
    }
}

double
Adsb_frame::Airborne_velocity_message::Get_heading() const
{
    ASSERT(Is_heading_available());

    if (Is_ground_speed_type()) {
        double ew = Get_ew_velocity_heading();
        double ns = Get_ns_velocity_airspeed();
        /* Calculate heading in range [0, Pi/2]. */
        double angle = M_PI / 2;
        if (ns) {
            angle = std::atan(ew / ns);
        }
        if (Get_ns_direction_airspeed_type()) {
            /* To South. */
            return Get_ew_direction_heading_status() ? angle + M_PI : angle + M_PI / 2;
        } else {
            /* To North. */
            return Get_ew_direction_heading_status() ? angle + M_PI * 3 / 2 : angle;
        }
    } else {
        /* Heading is known directly. */
        return Get_ew_velocity_heading() * (2 * M_PI / 1024);
    }
}

bool
Adsb_frame::Airborne_velocity_message::Is_vertical_speed_available() const
{
    return Get_vertical_rate();
}

double
Adsb_frame::Airborne_velocity_message::Get_vertical_speed() const
{
    ASSERT(Is_vertical_speed_available());
    double speed = (Get_vertical_rate() - 1) * 64; /* In feet/minute */
    return (Get_vertical_rate_sign() ? -1 : 1) * (speed * FEET_TO_M) / 60;
}

bool
Adsb_frame::Airborne_velocity_message::Is_ground_speed_type() const
{
    auto subtype = frame->Get_ME_subtype();
    if (subtype == Sub_type::TYPE_1 || subtype == Sub_type::TYPE_2) {
        return true;
    } else if (subtype == Sub_type::TYPE_3 || subtype == Sub_type::TYPE_4) {
        return false;
    } else {
        ASSERT(false);
        return false;
    }
}

bool
Adsb_frame::Airborne_velocity_message::Is_supersonic() const
{
    return frame->Get_ME_subtype() == Sub_type::TYPE_2 ||
           frame->Get_ME_subtype() == Sub_type::TYPE_4;
}

bool
Adsb_frame::Airborne_velocity_message::Get_ew_direction_heading_status() const
{
    return (frame->frame_ptr->ME[1] & 0x4);
}

uint16_t
Adsb_frame::Airborne_velocity_message::Get_ew_velocity_heading() const
{
    return ((frame->frame_ptr->ME[1] & 0x3) << 8) | frame->frame_ptr->ME[2];
}

bool
Adsb_frame::Airborne_velocity_message::Get_ns_direction_airspeed_type() const
{
    return (frame->frame_ptr->ME[3] & 0x80);
}

uint16_t
Adsb_frame::Airborne_velocity_message::Get_ns_velocity_airspeed() const
{
    return ((frame->frame_ptr->ME[3] & 0x7f) << 3) |
            (frame->frame_ptr->ME[4] >> 5);
}

bool
Adsb_frame::Airborne_velocity_message::Get_vertical_rate_sign() const
{
    return (frame->frame_ptr->ME[4] & 0x8);
}

uint16_t
Adsb_frame::Airborne_velocity_message::Get_vertical_rate() const
{
    return ((frame->frame_ptr->ME[4] & 0x7) << 6) |
            (frame->frame_ptr->ME[5] >> 2);
}
