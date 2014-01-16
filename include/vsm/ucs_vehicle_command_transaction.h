// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file ucs_vehicle_command_transaction.h
 */
#ifndef _UCS_VEHICLE_COMMAND_TRANSACTION_H
#define _UCS_VEHICLE_COMMAND_TRANSACTION_H
#include <vsm/ucs_transaction.h>

namespace vsm {

/**  Vehicle command transaction from UCS server to vehicle. */
class Ucs_vehicle_command_transaction: public Ucs_transaction {
    DEFINE_COMMON_CLASS(Ucs_vehicle_command_transaction, Ucs_transaction);
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
    Process(mavlink::Message<mavlink::MESSAGE_ID::COMMAND_LONG>::Ptr) override;

    void
    On_vehicle_command_completed(Vehicle_request::Result);

    Vehicle_command_request::Ptr vehicle_command;

    /** Original command. */
    mavlink::Pld_command_long command;
};

} /* namespace vsm */

#endif /* _UCS_VEHICLE_COMMAND_TRANSACTION_H */
