// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <vsm/adsb_report.h>

using namespace vsm;

Adsb_report::Adsb_report()
{
}

Adsb_report::Adsb_report(const Adsb_frame::ICAO_address& addr,
        Optional<std::string> ident,
                Geodetic_tuple pos, Optional<double> altitude,
                Optional<double> heading,
                Optional<double> horizontal_speed,
                Optional<double> vertical_speed) :
                        address(addr), identification(ident),
                        position(pos), altitude(altitude),
                        heading(heading), horizontal_speed(horizontal_speed),
                        vertical_speed(vertical_speed)
{
}
