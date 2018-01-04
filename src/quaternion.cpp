// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/quaternion.h>
#include <cmath>
#include <algorithm>

namespace ugcs {
namespace vsm {

void
Quaternion::Set_from_eulers(float pitch, float roll, float yaw)
{
    float cro = cos(0.5 * roll);
    float cpi = cos(0.5 * pitch);
    float cya = cos(0.5 * yaw);
    float sro = sin(0.5 * roll);
    float spi = sin(0.5 * pitch);
    float sya = sin(0.5 * yaw);

    w = cro * cpi * cya + sro * spi * sya;
    x = sro * cpi * cya - cro * spi * sya;
    y = cro * spi * cya + sro * cpi * sya;
    z = cro * cpi * sya - sro * spi * cya;
}

void
Quaternion::Multiply(const Quaternion& q)
{
    float _x = w * q.x + x * q.w + y * q.z - z * q.y;
    float _y = w * q.y + y * q.w + z * q.x - x * q.z;
    float _z = w * q.z + z * q.w + x * q.y - y * q.x;
    float _w = w * q.w - x * q.x - y * q.y - z * q.z;
    x = _x;
    y = _y;
    z = _z;
    w = _w;
}

void
Quaternion::Get_eulers(float& pitch, float& roll, float& yaw)
{
    roll = atan2(2.0f * (w * x + y * z), 1.0f - 2.0f * (x * x + y * y));
    pitch = asin(std::max(-1.0f, std::min(1.0f, 2.0f * (w * y - z * x))));
    yaw = atan2(2.0f * (w * z + x * y), 1.0f - 2.0f * (y * y + z * z));
}

} /* namespace vsm */
} /* namespace ugcs */
