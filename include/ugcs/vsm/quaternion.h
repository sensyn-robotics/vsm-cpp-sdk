/*
 * quaternion.h
 *
 *  Created on: Sep 29, 2016
 *      Author: j
 */

#ifndef SRC_QUATERNION_H_
#define SRC_QUATERNION_H_

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

#endif /* SRC_QUATERNION_H_ */
