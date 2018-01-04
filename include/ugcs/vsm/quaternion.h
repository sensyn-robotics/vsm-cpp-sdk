// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#ifndef _QUATERNION_H_
#define _QUATERNION_H_

namespace ugcs {
namespace vsm {

class Quaternion {
public:
    void
    Multiply(const Quaternion& q);

    void
    Get_eulers(float& pitch, float& roll, float& yaw);

    void
    Set_from_eulers(float pitch, float roll, float yaw);

    float x = 0;
    float y = 0;
    float z = 0;
    float w = 1;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _QUATERNION_H_ */
