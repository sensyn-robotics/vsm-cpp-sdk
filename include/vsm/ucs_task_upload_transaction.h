// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file ucs_task_upload_transaction.h
 */
#ifndef _UCS_TASK_UPLOAD_TRANSACTION_H_
#define _UCS_TASK_UPLOAD_TRANSACTION_H_

#include <vsm/vehicle_request.h>
#include <vsm/ucs_transaction.h>

namespace vsm {
/** Task upload transaction from UCS server to vehicle. */
class Ucs_task_upload_transaction: public Ucs_transaction {
    DEFINE_COMMON_CLASS(Ucs_task_upload_transaction, Ucs_transaction);
public:
    using Ucs_transaction::Ucs_transaction;

    /** Specific transaction name. */
    virtual std::string
    Get_name() const override;

private:

    virtual void
    On_abort() override;

    /** Enable handler. */
    virtual void
    On_enable() override;

    /** Disable handler. */
    virtual void
    On_disable() override;

    /** Verify received task for validness. Return true on success, false
     * otherwise. */
    bool
    Verify_task();

    /** Component id of the vehicle targeted by this transaction. */
    uint8_t vehicle_component_id = 0;

    /** Task being uploaded to vehicle, or @a nullptr if there is no currently
     * uploading task for this vehicle. */
    Vehicle_task_request::Ptr task;

    /** Number of mission items in the mission provided by UCS. */
    size_t task_item_count = 0;

    /** Current mission item to be received from UCS. */
    size_t current_task_item = 0;

    /** @a true when request is submitted to vehicle and completion is awaited. */
    bool awaiting_vehicle = false;

    virtual void
    Process(mavlink::Message<mavlink::MESSAGE_ID::MISSION_COUNT>::Ptr) override;

    virtual void
    Process(mavlink::Message<mavlink::ugcs::MESSAGE_ID::MISSION_ITEM_EX,
            mavlink::ugcs::Extension>::Ptr) override;

    void
    Get_next_mission_item();

    void
    On_mission_upload_completed(Vehicle_request::Result);

    void
    On_clear_all_missions_completed(Vehicle_request::Result);
};

} /* namespace vsm */

#endif /* _UCS_TASK_UPLOAD_TRANSACTION_H_ */
