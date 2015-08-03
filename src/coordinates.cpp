// Copyright (c) 2015, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/coordinates.h>

float
ugcs::vsm::Normalize_angle_0_2pi(float a)
{
    float ret = fmod(a, 2 * M_PI);
    if (ret < 0) {
        return ret + 2 * M_PI;
    } else {
        return ret;
    }
}
