// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file ucs_mission_clear_all_transaction.h
 *
 * Mission clear all transaction from UCS server to vehicle.
 */
#ifndef _UCS_MISSION_CLEAR_ALL_TRANSACTION_H_
#define _UCS_MISSION_CLEAR_ALL_TRANSACTION_H_
#include <vsm/ucs_transaction.h>

namespace vsm {

/** Mission clear all transaction. */
class Ucs_mission_clear_all_transaction: public Ucs_transaction {
    DEFINE_COMMON_CLASS(Ucs_mission_clear_all_transaction, Ucs_transaction);
public:
    using Ucs_transaction::Ucs_transaction;

    /** Specific transaction name. */
    virtual std::string
    Get_name() const override;

private:

    virtual void
    On_abort() override;

    virtual void
    On_disable() override;

    virtual void
    Process(mavlink::Message<mavlink::MESSAGE_ID::MISSION_CLEAR_ALL>::Ptr) override;

    void
    On_clear_all_missions_completed(Vehicle_request::Result);

    Vehicle_clear_all_missions_request::Ptr clear_all_missions;

    /** Component id of the vehicle targeted by this transaction. */
    uint8_t vehicle_component_id = 0;
};

} /* namespace vsm */

#endif /* _UCS_MISSION_CLEAR_ALL_TRANSACTION_H_ */
