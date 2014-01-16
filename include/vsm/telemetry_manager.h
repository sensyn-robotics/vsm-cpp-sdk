// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file telemetry_manager.h
 *
 * Telemetry manager class declaration.
 */
#ifndef TELEMETRY_MANAGER_H_
#define TELEMETRY_MANAGER_H_

#include <vsm/telemetry_interface.h>
#include <vsm/telemetry_params.h>
#include <mutex>

namespace vsm {

/** This class implements an abstraction layer between telemetry provider and
 * internal telemetry data transport towards UCS. It also responsible for last
 * valid data replication in messages where they are required but were not
 * provided by last provider transaction.
 */
class Telemetry_manager {
public:

    /** Telemetry data report object. As many as possible parameters should be
     * encapsulated in one report in order to optimize telemetry traffic between
     * VSM and UCS.
     */
    class Report {
    public:
        /** Pointer type. */
        typedef std::shared_ptr<Report> Ptr;

        /** Create a report associated with manager. */
        Report(Telemetry_manager &manager, std::chrono::milliseconds timestamp);

        ~Report();

        /** Set telemetry parameter in the report. See vsm::telemetry namespace
         * for all supported telemetry parameter types.
         * @param param New parameter value. Default value sets the parameter to
         *      undefined state (might not be supported by individual parameter,
         *      in such case the last value will remain active).
         */
        template <class Param_type>
        void
        Set(const Param_type &param = Param_type())
        {
            params.emplace_back(std::unique_ptr<Param_type>(new Param_type(param)));
        }

        /** Commit all accumulated parameters. This method call can be avoided
         * because it is implicitly called when the report object is destructed.
         */
        void
        Commit();
    private:
        /** Associated manager. */
        Telemetry_manager &manager;
        /** Report timestamp (on-board time). */
        std::chrono::milliseconds timestamp;
        /** All parameters reported in scope of this report. */
        std::vector<std::unique_ptr<tm::internal::Param>> params;
    };

    /** Register telemetry interface towards UCS. */
    void
    Register_transport_interface(Telemetry_interface &iface);

    /** Unregister current telemetry interface towards UCS. */
    void
    Unregister_transport_interface();

    /** Open new report object which can be used to report necessary telemetry
     * parameters.
     *
     * @param timestamp Report timestamp (on-board time).
     * @return Report object.
     */
    Report::Ptr
    Open_report(std::chrono::milliseconds timestamp);

private:
    std::mutex mutex;
    /** Telemetry data transport interface towards UCS. */
    Telemetry_interface iface;

    /** Raw data representation as it is sent to UCS. */
    struct Raw_data {
        enum Payload_type {
            SYS_STATUS,
            POSITION,
            ATTITUDE,
            GPS_RAW,
            RAW_IMU,
            SCALED_PRESSURE,
            VFR_HUD,

            NUM_PAYLOAD_TYPES
        };

        mavlink::Pld_sys_status sys_status;
        mavlink::Pld_global_position_int position;
        mavlink::Pld_attitude attitude;
        mavlink::Pld_gps_raw_int gps_raw;
        mavlink::Pld_raw_imu raw_imu;
        mavlink::Pld_scaled_pressure scaled_pressure;
        mavlink::Pld_vfr_hud vfr_hud;

        int payload_generation[NUM_PAYLOAD_TYPES];

        Raw_data()
        {
            Reset();
        }

        /** Reset all parameters to their undefined values. */
        void
        Reset();

        /** Mark payload of the given type as modified. */
        void
        Hit_payload(Payload_type type);


        /** Check if payload of the given type was modified. */
        bool
        Is_dirty(Payload_type type, const Raw_data &prev_data);

        /** Commit new data. */
        void
        Commit(const Telemetry_interface &iface,
               std::chrono::milliseconds timestamp,
               const Raw_data &prev_data);
    };
    /** Last known data values. */
    Raw_data last_data;

    /** Commit whole the accumulated report.
     * @param timestamp Report timestamp (on-board time).
     * @param prev_data Previous raw data state.
     */
    void
    Commit(std::chrono::milliseconds timestamp, const Raw_data &prev_data);

    /* Committers */
    template<typename> friend class tm::internal::Param_base;

    void
    Commit(const tm::Battery_voltage::Base &value);

    void
    Commit(const tm::Battery_current::Base &value);

    void
    Commit(const tm::Position::Base &value);

    void
    Commit(const tm::Abs_altitude::Base &value);

    void
    Commit(const tm::Rel_altitude::Base &value);

    void
    Commit(const tm::Gps_satellites_count::Base &value);

    void
    Commit(const tm::Gps_fix_type::Base &value);

    void
    Commit(const tm::Heading::Base &value);

    void
    Commit(const tm::Pitch::Base &value);

    void
    Commit(const tm::Roll::Base &value);

    void
    Commit(const tm::Yaw::Base &value);

    void
    Commit(const tm::Ground_speed::Base &value);

    void
    Commit(const tm::Climb_rate::Base &value);

    void
    Commit(const tm::Link_quality::Base &value);
};

} /* namespace vsm */

#endif /* TELEMETRY_MANAGER_H_ */
