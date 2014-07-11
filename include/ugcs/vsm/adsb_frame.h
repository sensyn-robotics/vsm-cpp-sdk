// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file adsb_frame.h
 *
 * Basic functionality for decoding ADS-B frame format.
 */
#ifndef _ADSB_FRAME_H_
#define _ADSB_FRAME_H_

#include <ugcs/vsm/endian.h>
#include <ugcs/vsm/debug.h>
#include <ugcs/vsm/utils.h>
#include <ugcs/vsm/io_buffer.h>
#include <chrono>

namespace ugcs {
namespace vsm {

/** ADS-B frame class. */
class Adsb_frame: public std::enable_shared_from_this<Adsb_frame> {
    DEFINE_COMMON_CLASS(Adsb_frame, Adsb_frame)
private:

    enum {
        /** The length of ME field in bytes. Do not change it! */
        ME_FIELD_LENGTH = 7,
    };

public:

    /** POD class for raw frame access. */
    struct Frame {
        /** DF, (CA|CF|AF) */
        uint8_t first_byte;
        /** AA ICAO or non-ICAO address. */
        uint8_t address[3];
        /** ME field. */
        uint8_t ME[ME_FIELD_LENGTH];
        /** Checksum field. */
        uint8_t PI[3];
    } __PACKED;

    // @{
    /** Downlink format constants. */
    enum Downlink_format {
        DF_17 = 17,
        DF_18 = 18,
        DF_19 = 19,
    };
    // @}

    // @{
    /** CF field values. Currently contains only values which denote
     * ADS-B ME and ICAO address fields presence. */
    enum CF_values {
        CF_0 = 0,
        CF_1 = 1,
        CF_6 = 6,
    };
    // @}

    /** AF field values. Currently contain only values which denote ADS-B ME
     * and ICAO address fields presence. */
    enum AF_values {
        AF_0 = 0
    };

    /** ICAO globally unique aircraft identification. */
    class ICAO_address {
    public:

        /** The type of the ICAO address. */
        enum Type {
            /** Real ICAO address. */
            REAL,
            /** Anonymous address or ground vehicle address or fixed obstacle
             * address of the transmitting ADS-B Participant. */
            ANONYMOUS
        };

        /** Construct ICAO address from 3 bytes. */
        ICAO_address(Type type, uint8_t high, uint8_t middle, uint8_t low);

        /** Default copy constructor. */
        ICAO_address(const ICAO_address&) = default;

        /** Equality operator. */
        bool
        operator ==(const ICAO_address&) const;

        /** Non-equality operator. */
        bool
        operator !=(const ICAO_address&) const;

        /** Get HEX string representation of the address. */
        std::string
        To_hex_string() const;

        /** Get address value in 3 less significant bytes. */
        uint32_t
        Get_address() const;

        /** Get the type of the address. */
        Type
        Get_type() const;

        /** Address hasher. */
        class Hasher
        {
        public:
            /** Hash value operator. */
            size_t
            operator ()(const ICAO_address& addr) const
            {
                return addr.address ^ addr.type;
            }
        };

    private:

        /** The type. */
        Type type;

        /** Address in native byte order. */
        uint32_t address;
    };

    /** Thrown when raw data buffer is of incorrect length. */
    VSM_DEFINE_EXCEPTION(Invalid_buffer);

    /** Thrown when unrecoverable format error is found. Usually means
     * user error. */
    VSM_DEFINE_EXCEPTION(Invalid_format);

    /** Supported size of ADS-B frame. */
    constexpr static size_t SIZE = sizeof(Frame);

    /** Construct frame based on buffer of size SIZE. Throws Invalid_buffer
     * exception if buffer size is not equal to SIZE. */
    Adsb_frame(Io_buffer::Ptr);

    /** Verify frame checksum.
     * @return true if checksum is valid, otherwise false.
     */
    bool
    Verify_checksum() const;

    /** Get the time when the frame was received. */
    std::chrono::steady_clock::time_point
    Get_received_time() const;

    /** Get DF field value. */
    uint8_t
    Get_DF() const;

    /** Get CA field value. */
    uint8_t
    Get_CA() const;

    /** Get CF field value. */
    uint8_t
    Get_CF() const;

    /** Get AF field value. */
    uint8_t
    Get_AF() const;

    /** Get ME message type. */
    uint8_t
    Get_ME_type() const;

    /** Get ME message sub-type. */
    uint8_t
    Get_ME_subtype() const;

    /** Checks if the message is ADS-R or not. */
    bool
    Is_rebroadcast() const;

    /** Base class for ME message. */
    class ME_message {
    public:

        // @{
        /** Type of the ME frame. Last number is an actual value, for convenience. */
        enum Type {
            NO_POSITION_0 = 0,
            AIRCRAFT_ID_AND_CAT_D_1 = 1,
            AIRCRAFT_ID_AND_CAT_C_2 = 2,
            AIRCRAFT_ID_AND_CAT_B_3 = 3,
            AIRCRAFT_ID_AND_CAT_A_4 = 4,
            SURFACE_POSITION_5 = 5,
            SURFACE_POSITION_6 = 6,
            SURFACE_POSITION_7 = 7,
            SURFACE_POSITION_8 = 8,
            AIRBORNE_POSITION_9 = 9,
            AIRBORNE_POSITION_10 = 10,
            AIRBORNE_POSITION_11 = 11,
            AIRBORNE_POSITION_12 = 12,
            AIRBORNE_POSITION_13 = 13,
            AIRBORNE_POSITION_14 = 14,
            AIRBORNE_POSITION_15 = 15,
            AIRBORNE_POSITION_16 = 16,
            AIRBORNE_POSITION_17 = 17,
            AIRBORNE_POSITION_18 = 18,
            AIRBORNE_VELOCITY_19 = 19,
            AIRBORNE_POSITION_20 = 20,
            AIRBORNE_POSITION_21 = 21,
            AIRBORNE_POSITION_22 = 22,
            TEST_23 = 23,
            SURFACE_SYSTEM_STATUS_24 = 24,
            RESERVED_25 = 25,
            RESERVED_26 = 26,
            TRAJECTORY_CHANGE_RESERVED_27 = 27,
            STATUS_MESSAGE_28 = 28,
            TARGET_STATE_AND_STATUS_29 = 29,
            RESERVED_30 = 30,
            AIRCRAFT_OPERATIONAL_STATUS_31 = 31,
        };
        // @}

        // @{
        /** Sub-type of the ME frame. */
        enum Sub_type {
            TYPE_1 = 1,
            TYPE_2 = 2,
            TYPE_3 = 3,
            TYPE_4 = 4,
        };
        // @}

        /** Construct message from an ADS-B frame. */
        ME_message(Adsb_frame::Ptr);

        /** Construct empty message. */
        ME_message() = default;

        /** Virtual destructor. */
        virtual
        ~ME_message();

        /** Check, if message is empty or not. */
        bool
        Is_empty() const;

        /** Get the type of ME message. Convenience. */
        uint8_t
        Get_ME_type() const;

        /** Get the ICAO address of the ME message. */
        ICAO_address
        Get_ICAO_address() const;

        /** Get the time when the message was received. */
        std::chrono::steady_clock::time_point
        Get_received_time() const;

    protected:

        /** ADS-B frame with ME message.*/
        Adsb_frame::Ptr frame;

        /** The value of IMF field (if applicable) of the message.
         * false by default which denotes a real ICAO address. */
        bool imf = false;

        /** Feet to meters conversion constant. */
        static constexpr double FEET_TO_M = 0.3048;
    };

    /** Position message, common for airborne and surface position messages. */
    class Position_message: public ME_message {
    public:

        /** Construct empty. */
        Position_message() = default;

        /** Get time of IMF bit value. */
        bool
        Get_time_IMF() const;

        /** Get CPR format bit.
         * @return true if CPR frame format is odd, otherwise false. */
        bool
        Get_CPR_format() const;

        /** Get latitude field. */
        uint32_t
        Get_CPR_latitude() const;

        /** Get longitude field. */
        uint32_t
        Get_CPR_longitude() const;

    protected:

        /** Do not allow users to create ambiguous position messages. */
        Position_message(Adsb_frame::Ptr);

    };

    /** Airborne position message. */
    class Airborne_position_message: public Position_message {
    public:

        /** Construct empty position message. */
        Airborne_position_message() = default;

        /** Construct from non-empty frame. */
        Airborne_position_message(Adsb_frame::Ptr);

        /** Get surveillance status. */
        uint8_t
        Get_surveillance_status() const;

        /** Get NIC supplement or IMF. */
        bool
        Get_NIC_supplement_IMF() const;

        /** Return true is altitude is available, otherwise false. */
        bool
        Is_altitude_available() const;

        /** Return altitude in feet or meters - either barometric or GNSS. */
        int32_t
        Get_altitude(bool in_feet = false) const;

    private:

        /** Get the raw value of altitude field. */
        uint16_t
        Get_altitude_raw() const;
    };

    /** Surface position message. */
    class Surface_position_message: public Position_message {
    public:

        /** Construct empty position message. */
        Surface_position_message() = default;

        /** Construct from non-empty frame. */
        Surface_position_message(Adsb_frame::Ptr);

        /** Checks speed data availability. */
        bool
        Is_speed_available() const;

        /** Get the ground speed of the aircraft, in m/s/ */
        double
        Get_speed() const;

        /** Checks heading availability. */
        bool
        Is_heading_available() const;

        /** Ground heading in radians clockwise from North. */
        double
        Get_heading() const;

    private:

        /** Get the raw value of the movement field. */
        uint8_t
        Get_movement_raw() const;

        /** Get the raw value of the heading field. */
        uint8_t
        Get_heading_raw() const;
    };

    /** Aircraft identification and category message. */
    class Aircraft_id_and_cat_message: public ME_message {
    public:
        using ME_message::ME_message;

        // @{
        /** Category of the vehicle emitting an identification message. */
        enum Emitter_category {
            NO_INFO_A,
            NO_INFO_B,
            NO_INFO_C,
            NO_INFO_D,
            LIGHT,
            SMALL,
            LARGE,
            HIGH_VORTEX_LARGE,
            HEAVY,
            HIGH_PERFORMANCE,
            ROTORCRAFT,
            GLIDER_SAILPLANE,
            LIGHTER_THAN_AIR,
            PARACHUTIST_SKYDIVER,
            ULTRALIGHT_PARAGLIDER,
            RESERVED,
            UAV,
            SPACE_TRANS_ATMOSPHERIC,
            SURFACE_EMERGENCY,
            SURFACE_SERVICE,
            POINT_OBSTACLE,
            CLUSTER_OBSTACLE,
            LINE_OBSTACLE
        };
        // @{

        /** Get aircraft identification string. */
        std::string
        Get_id() const;

        /** Get aircraft category. */
        Emitter_category
        Get_emitter_category() const;

    private:
        enum {
            /** The length of identification field. */
            ID_FIELD_LENGTH = ME_FIELD_LENGTH - 1,

            /** Number of 6-bit characters in identification field. */
            NUM_OF_CHARS = 8
        };

        /** Convert encoded char to ASCII char. */
        static char
        Id_to_char(uint8_t);
    };

    /** Airborne velocity message. */
    class Airborne_velocity_message: public ME_message {
    public:

        /** Construct from non-empty frame. */
        Airborne_velocity_message(Adsb_frame::Ptr);

        /** Construct empty message. */
        Airborne_velocity_message() = default;

        /** Get intent or IMF field. */
        bool
        Get_intent_IMF() const;

        /** Checks, if horizontal speed is available. */
        bool
        Is_horizontal_speed_available() const;

        /** Get horizontal speed, in m/s. */
        double
        Get_horizontal_speed() const;

        /** Checks, if heading is available. */
        bool
        Is_heading_available() const;

        /** Get heading, in radians clockwise from North. */
        double
        Get_heading() const;

        /** Checks, if vertical speed is available. */
        bool
        Is_vertical_speed_available() const;

        /** Get vertical speed, in m/s. */
        double
        Get_vertical_speed() const;

    private:

        /** Knots to m/s conversion constant. */
        static constexpr double KNOTS_TO_MS = 0.514444;

        /** Checks, if this message denotes ground or airspeed type. */
        bool
        Is_ground_speed_type() const;

        /** Checks, if reported speed is under supersonic conditions. */
        bool
        Is_supersonic() const;

        /** Get E/W direction or heading status bit. */
        bool
        Get_ew_direction_heading_status() const;

        /** Get E/W velocity or heading. */
        uint16_t
        Get_ew_velocity_heading() const;

        /** Get N/S direction or airspeed type bit. */
        bool
        Get_ns_direction_airspeed_type() const;

        /** Get N/S velocity or airspeed. */
        uint16_t
        Get_ns_velocity_airspeed() const;

        /** Get the sign of vertical rate. */
        bool
        Get_vertical_rate_sign() const;

        /** Get vertical rate. */
        uint16_t
        Get_vertical_rate() const;
    };

private:

    /** ADS-B frame buffer. */
    Io_buffer::Ptr frame;

    /** Pointer to POD class representing a frame. */
    const Frame* frame_ptr;

    /** Time point when the frame was received. Equals to the instance
     * creation time.
     */
    std::chrono::steady_clock::time_point received;

    /** Calculate ADS-B frame CRC-24 checksum based on exactly 11-byte buffer. */
    static uint32_t
    Calculate_crc(const uint8_t*);

    /** Get the value of the field for CA/CF/AF. */
    uint8_t
    Get_CA_CF_AF() const;

    /** Convert value in Gray code to native value. */
    uint16_t
    Gray_to_value(uint16_t);

    /** Get ICAO address interpreted as per given type. */
    ICAO_address
    Get_ICAO_address(ICAO_address::Type type) const;

};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _ADSB_FRAME_H_ */
