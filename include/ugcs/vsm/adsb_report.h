// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file adsb_report.h
 */
#ifndef _ADSB_REPORT_H_
#define _ADSB_REPORT_H_

#include <ugcs/vsm/adsb_frame.h>
#include <ugcs/vsm/optional.h>
#include <ugcs/vsm/coordinates.h>

namespace ugcs {
namespace vsm {

/** ADS-B report information. */
class Adsb_report {
public:

    /** Construct a report with default values. */
    Adsb_report();

    /** Construct a specific report. */
    Adsb_report(const Adsb_frame::ICAO_address&, Optional<std::string>,
            Geodetic_tuple, Optional<double>, Optional<double>,
            Optional<double>, Optional<double>);

    // @{
    /** Reported values. */
    Adsb_frame::ICAO_address address =
            Adsb_frame::ICAO_address(
                    Adsb_frame::ICAO_address::Type::ANONYMOUS,
                    0, 0, 0);

    Optional<std::string> identification;

    Geodetic_tuple position = Geodetic_tuple(0, 0, 0);

    Optional<double> altitude;

    Optional<double> heading;

    Optional<double> horizontal_speed;

    Optional<double> vertical_speed;
    // @}
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _ADSB_REPORT_H_ */
