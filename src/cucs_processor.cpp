// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/log.h>
#include <ugcs/vsm/debug.h>
#include <ugcs/vsm/cucs_processor.h>
#include <ugcs/vsm/request_context.h>
#include <ugcs/vsm/timer_processor.h>
#include <ugcs/vsm/transport_detector.h>
#include <ugcs/vsm/properties.h>

using namespace ugcs::vsm;

constexpr std::chrono::seconds Cucs_processor::WRITE_TIMEOUT;

Singleton<Cucs_processor> Cucs_processor::singleton;

Cucs_processor::Cucs_processor():
        Request_processor("Cucs processor")
{
}

void
Cucs_processor::Register_vehicle(Vehicle::Ptr vehicle)
{
    bool success = false;
    auto request = Request::Create();
    auto proc_handler = Make_callback(
            &Cucs_processor::On_register_vehicle,
            Shared_from_this(),
            request,
            vehicle,
            &success);
    request->Set_processing_handler(proc_handler);
    Operation_waiter waiter(request);
    Submit_request(request);
    /* Register vehicle is never called from UCS processor context, so no
     * need to process requests. */
    waiter.Wait(false);
    if (!success) {
        VSM_EXCEPTION(Invalid_param_exception,
                "Can not register vehicle [%s:%s].",
                vehicle->Get_model_name().c_str(),
                vehicle->Get_serial_number().c_str());
    }
}

void
Cucs_processor::Unregister_vehicle(Vehicle::Ptr vehicle)
{
    bool success = false;
    auto request = Request::Create();
    auto proc_handler = Make_callback(
            &Cucs_processor::On_unregister_vehicle,
            Shared_from_this(),
            request,
            vehicle,
            &success);
    request->Set_processing_handler(proc_handler);
    Operation_waiter waiter(request);
    Submit_request(request);
    /* Unregister vehicle is never called from UCS processor context, so no
     * need to process request. */
    waiter.Wait(false);
    if (!success) {
        VSM_EXCEPTION(Invalid_param_exception, "Vehicle with ID %u already unregistered",
                vehicle->system_id);
    }
}

void
Cucs_processor::Send_adsb_report(const Adsb_report& report)
{
    auto request = Request::Create();
    request->Set_processing_handler(
            Make_callback(
                    &Cucs_processor::On_send_adsb_report,
                    Shared_from_this(),
                    report, request));
    pending_adsb_reports.fetch_add(1);
    Submit_request(request);
    if (pending_adsb_reports.load() >= MAX_ADSB_REPORTS_PENDING) {
        LOG_WARNING("CUCS processor overload suspected, %zu pending ADS-B "
                "reports are present. Waiting for all reports to be completed...",
                pending_adsb_reports.load());
        Operation_waiter waiter(request);
        waiter.Wait(false);
        LOG_DEBUG("%zu pending ADS-B reports after waiting.",
                pending_adsb_reports.load());
    }
}

void
Cucs_processor::Send_peripheral_register(const Peripheral_message::Peripheral_register& report) {
    auto request = Request::Create();
    request->Set_processing_handler(
            Make_callback(
                    &Cucs_processor::On_send_peripheral_register,
                    Shared_from_this(),
                    report, request));
    Operation_waiter waiter(request);
    Submit_request(request);
    waiter.Wait(false);
}


void
Cucs_processor::Send_peripheral_update(const Peripheral_message::Peripheral_update& update) {
    auto request = Request::Create();
    request->Set_processing_handler(
            Make_callback(
                    &Cucs_processor::On_send_peripheral_update,
                    Shared_from_this(),
                    update, request));
    Operation_waiter waiter(request);
    Submit_request(request);
    waiter.Wait(false);
}

void
Cucs_processor::On_enable()
{
    completion_ctx = Request_completion_context::Create("Cucs processor completion");
    worker = Request_worker::Create(
            "Cucs processor worker",
            std::initializer_list<Request_container::Ptr>{completion_ctx, Shared_from_this()});
    completion_ctx->Enable();
    Request_processor::On_enable();
    worker->Enable();
    heartbeat_timer = Timer_processor::Get_instance()->Create_timer(
            std::chrono::seconds(3),
            Make_callback(&Cucs_processor::On_heartbeat_timer, Shared_from_this()),
                    completion_ctx);
    Start_listening();
}

void
Cucs_processor::On_disable()
{
    auto req = Request::Create();
    req->Set_processing_handler(
            Make_callback(
                    &Cucs_processor::Process_on_disable,
                    Shared_from_this(),
                    req));
    Submit_request(req);
    req->Wait_done(false);
    Set_disabled();
    worker->Disable();
}

void
Cucs_processor::Process_on_disable(Request::Ptr request)
{
    accept_op.Abort();
    cucs_listener_op.Abort();
    heartbeat_timer->Cancel();
    heartbeat_timer = nullptr;
    if (cucs_listener) {
        cucs_listener->Close();
    }
    if (vehicles.size()) {
        LOG_ERR("%zu vehicles are still present in Cucs processor while disabling.",
                vehicles.size());
        ASSERT(false);
    }
    auto vehicles_copy = vehicles;
    for (auto& iter : vehicles_copy) {
        /* Disable context. */
        iter.second->Disable();
    }
    vehicles.clear();
    vehicles_copy.clear();
    for (auto& iter : ucs_connections) {
        iter.second.Abort();
        iter.first->Get_stream()->Close();
        iter.first->Disable();
    }
    ucs_connections.clear();

    completion_ctx->Disable();
    completion_ctx = nullptr;

    request->Complete();
}

void
Cucs_processor::On_listening_started(Socket_processor::Socket_listener::Ref listener,
        Io_result result)
{
    if (result == Io_result::OK) {
        cucs_listener = listener;
        LOG_DEBUG("Cucs listening started.");
        Accept_next_connection();

    } else {
        LOG_ERR("Cucs listening failed. %d", result);
        //XXX Try again
        std::this_thread::sleep_for(std::chrono::seconds(1));
        Start_listening();
    }
}

void
Cucs_processor::On_incoming_connection(Socket_processor::Stream::Ref stream, Io_result result)
{
    if (result == Io_result::OK) {
        LOG_INFO("UCS connection accepted [%s].",
                stream->Get_name().c_str());
        Ucs_vehicle_ctx::Ugcs_mavlink_stream::Ptr mav_stream = Ucs_vehicle_ctx::Ugcs_mavlink_stream::Create(
                stream, mavlink::ugcs::Extension::Get());

        mav_stream->Bind_decoder_demuxer();

        Register_mavlink_handlers(mav_stream);

        if (ucs_connections.size() == 0) {
            Transport_detector::Get_instance()->Activate(true);
        }

        Operation_waiter& op_waiter = ucs_connections.emplace(
                mav_stream,
                Operation_waiter()).first->second;
        Schedule_next_read(mav_stream, op_waiter);

        if(on_new_connection_callback_proxy) {
        	on_new_connection_callback_proxy();
        }

    } else {
        LOG_WARN("Incoming connection accept failed %d", result);
    }
    Accept_next_connection();
}

void
Cucs_processor::Schedule_next_read(
        Ucs_vehicle_ctx::Ugcs_mavlink_stream::Ptr& mav_stream,
        Operation_waiter& waiter)
{
    waiter.Abort();
    auto cb = Make_read_callback(
            &Cucs_processor::Read_completed,
            Shared_from_this(),
            mav_stream);
    size_t to_read = mav_stream->Get_decoder().Get_next_read_size();
    waiter = mav_stream->Get_stream()->Read(to_read, to_read, std::move(cb), completion_ctx);
}

void
Cucs_processor::Read_completed(Io_buffer::Ptr buffer, Io_result result,
        Ucs_vehicle_ctx::Ugcs_mavlink_stream::Ptr mav_stream)
{
    auto iter = ucs_connections.find(mav_stream);
    ASSERT(iter != ucs_connections.end());
    if (result == Io_result::OK) {
        mav_stream->Get_decoder().Decode(buffer);
        Schedule_next_read(mav_stream, iter->second);
    } else {
        LOG_WARN("UCS connection closed [%s], %zu vehicles are still registered.",
                mav_stream->Get_stream()->Get_name().c_str(),
                vehicles.size());
        mav_stream->Get_stream()->Close();
        /* Propagate stream disconnect event for all vehicle contexts. */
        for (auto& iter: vehicles) {
            iter.second->Stream_disconnected(mav_stream);
        }
        mav_stream->Disable();
        ucs_connections.erase(iter);
        if (ucs_connections.size() == 0) {
            if (!transport_detector_on_when_diconnected) {
                Transport_detector::Get_instance()->Activate(false);
            }
        }
    }
}

void
Cucs_processor::Register_mavlink_handlers(Ucs_vehicle_ctx::Ugcs_mavlink_stream::Ptr mav_stream)
{
    auto& demuxer = mav_stream->Get_demuxer();

    demuxer.Register_default_handler(
            Mavlink_demuxer::Make_default_handler(
                    &Cucs_processor::On_default_mavlink_message_handler,
                    Shared_from_this(),
                    mav_stream));

    /* Here are all Mavlink messages supported by UCS processor and UCS
     * transactions with their negative ack builders. Other messages will
     * end up in default handler.
     */
    Register_mavlink_message<mavlink::ugcs::MESSAGE_ID::MISSION_CLEAR_ALL_EX,
        Mission_nack_builder, mavlink::ugcs::Extension>(mav_stream);
    Register_mavlink_message<mavlink::ugcs::MESSAGE_ID::MISSION_COUNT_EX,
        Mission_nack_builder, mavlink::ugcs::Extension>(mav_stream);
    Register_mavlink_message<mavlink::ugcs::MESSAGE_ID::COMMAND_LONG_EX,
        Command_nack_builder, mavlink::ugcs::Extension>(mav_stream);
    Register_mavlink_message<mavlink::ugcs::MESSAGE_ID::MISSION_ITEM_EX,
        Mission_nack_builder, mavlink::ugcs::Extension>(mav_stream);
    Register_mavlink_message<mavlink::ugcs::MESSAGE_ID::TAIL_NUMBER_REQUEST,
        Tail_number_response_nack_builder, mavlink::ugcs::Extension>(mav_stream);

}

void
Cucs_processor::Start_listening()
{
    cucs_listener_op.Abort();
    auto props = Properties::Get_instance();
    if (props->Exists("ucs.disable")) {
        return;
    }

    if (Properties::Get_instance()->Exists("ucs.transport_detector_on_when_diconnected")) {
        transport_detector_on_when_diconnected = true;
        Transport_detector::Get_instance()->Activate(true);
    } else {
        transport_detector_on_when_diconnected = false;
        Transport_detector::Get_instance()->Activate(false);
    }

    cucs_listener_op = Socket_processor::Get_instance()->Listen(
            Socket_address::Create(
                    props->Get("ucs.local_listening_address"),
                    props->Get("ucs.local_listening_port")),
            Make_socket_listen_callback(
                    &Cucs_processor::On_listening_started,
                    Shared_from_this()),
            completion_ctx);
}

void
Cucs_processor::Accept_next_connection()
{
    accept_op = Socket_processor::Get_instance()->Accept(cucs_listener,
            Make_socket_accept_callback(
                    &Cucs_processor::On_incoming_connection,
                    Shared_from_this()),
            completion_ctx);
}

void
Cucs_processor::On_register_vehicle(Request::Ptr request, Vehicle::Ptr vehicle,
        bool* success)
{
    if (vehicles.find(vehicle->system_id) != vehicles.end()) {
        *success = false;
        request->Complete();
        return;
    }

    Ucs_vehicle_ctx::Ptr ctx =
            Ucs_vehicle_ctx::Create(vehicle, Shared_from_this(), completion_ctx);
    ctx->Enable();

    vehicles.emplace(vehicle->system_id, ctx);
    Register_vehicle_telemetry(ctx);
    *success = true;
    request->Complete();
    if (!ucs_connections.size()) {
        LOG_WARN("Vehicle [%s:%s] registered, but no any UCS servers connected.",
          vehicle->Get_model_name().c_str(), vehicle->Get_serial_number().c_str());
    } else {
        /* Immediately notify all servers about new vehicle. */
        Send_heartbeat(vehicle.get());
    }
}

void
Cucs_processor::On_unregister_vehicle(Request::Ptr request, Vehicle::Ptr vehicle,
        bool* success)
{
    auto iter = vehicles.find(vehicle->system_id);
    if (iter == vehicles.end()) {
        *success = false;
        request->Complete();
        return;
    }
    auto ctx = iter->second;
    vehicles.erase(iter);
    ctx->Get_vehicle()->Unregister_telemetry_interface();
    ctx->Disable();
    *success = true;
    request->Complete();
}

void
Cucs_processor::On_send_adsb_report(
        Adsb_report report,
        Request::Ptr request)
{
    mavlink::ugcs::Pld_adsb_aircraft_report msg;

    uint32_t icao = report.address.Get_address();
    switch (report.address.Get_type()) {
    case Adsb_frame::ICAO_address::Type::REAL:
        icao |= mavlink::ugcs::MAV_ICAO_ADDRESS_TYPE::MAV_ICAO_ADDRESS_TYPE_REAL << 24;
        break;
    case Adsb_frame::ICAO_address::Type::ANONYMOUS:
        icao |= mavlink::ugcs::MAV_ICAO_ADDRESS_TYPE::MAV_ICAO_ADDRESS_TYPE_ANONYMOUS << 24;
        break;
    }
    auto nan = std::numeric_limits<float>::quiet_NaN();
    msg->icao_id = icao;
    msg->latitude = (report.position.latitude * 180) / M_PI;
    msg->longitude = (report.position.longitude * 180) / M_PI;
    msg->msl_altitude = report.altitude ? *report.altitude : nan;
    msg->heading = report.heading ? (*report.heading * 180) / M_PI : -1;
    msg->ground_speed = report.horizontal_speed ? *report.horizontal_speed : nan;
    msg->vertical_velocity = report.vertical_speed ? *report.vertical_speed : nan;
    msg->identification = report.identification ? *report.identification : "";
#if 0
    LOG_DEBUG("Sending ADS-B report about ICAO:%s",
            report.address.To_hex_string().c_str());
#endif
    Broadcast_mavlink_message(msg, DEFAULT_SYSTEM_ID, DEFAULT_COMPONENT_ID);
    request->Complete();
    ASSERT(pending_adsb_reports.load() > 0);
    pending_adsb_reports.fetch_sub(1);
}

void
Cucs_processor::On_send_peripheral_register(
		Peripheral_message::Peripheral_register report,
        Request::Ptr request)
{
    mavlink::ugcs::Pld_peripheral_device_register msg;

    msg->device_id = report.Get_id();
    msg->device_type = static_cast<uint8_t>(report.Get_dev_type());
    msg->device_name = report.Get_name().c_str();
    msg->port_name = report.Get_port().c_str();
#if 0
    LOG_DEBUG("Sending device report for [%s]",
            report.Get_name().c_str());
#endif
    Broadcast_mavlink_message(msg, DEFAULT_SYSTEM_ID, DEFAULT_COMPONENT_ID);
    request->Complete();
}

void
Cucs_processor::On_send_peripheral_update(
		Peripheral_message::Peripheral_update update,
        Request::Ptr request)
{
    mavlink::ugcs::Pld_peripheral_device_update msg;

    msg->device_id = update.Get_id();
    msg->device_state = static_cast<uint8_t>(update.Get_state());;
#if 0
    LOG_DEBUG("Sending device update for #[%d]",
            update.Get_id());
#endif
    Broadcast_mavlink_message(msg, DEFAULT_SYSTEM_ID, DEFAULT_COMPONENT_ID);
    request->Complete();
}

bool
Cucs_processor::On_default_mavlink_message_handler(
        mavlink::MESSAGE_ID_TYPE message_id,
        typename mavlink::Mavlink_kind_ugcs::System_id,
        uint8_t,
        Ucs_vehicle_ctx::Ugcs_mavlink_stream::Ptr mav_stream)
{
    LOG_WARN("Unsupported Mavlink message %d, closing connection.", message_id);
    mav_stream->Get_stream()->Close();
    return false;
}

bool
Cucs_processor::On_heartbeat_timer()
{
    for (auto& vehicle_iter: vehicles) {
        Send_heartbeat(vehicle_iter.second->Get_vehicle().get());
    }
    return true;
}

void
Cucs_processor::Send_heartbeat(Vehicle* vehicle)
{
    for (auto& iter: ucs_connections) {
        mavlink::Pld_heartbeat hb;
        hb->autopilot = vehicle->autopilot;

        auto status = vehicle->Get_system_status();
        switch (status.control_mode) {
        case Vehicle::Sys_status::Control_mode::MANUAL:
            hb->base_mode = mavlink::MAV_MODE_FLAG::MAV_MODE_FLAG_MANUAL_INPUT_ENABLED;
            break;
        case Vehicle::Sys_status::Control_mode::AUTO:
            hb->base_mode = mavlink::MAV_MODE_FLAG::MAV_MODE_FLAG_AUTO_ENABLED;
            break;
        case Vehicle::Sys_status::Control_mode::UNKNOWN:
            break;
        }

        switch (status.state) {
        case Vehicle::Sys_status::State::DISARMED:
            hb->system_status = mavlink::MAV_STATE::MAV_STATE_POWEROFF;
            break;
        case Vehicle::Sys_status::State::ARMED:
            hb->system_status = mavlink::MAV_STATE::MAV_STATE_ACTIVE;
            break;
        case Vehicle::Sys_status::State::UNKNOWN:
            hb->system_status = mavlink::MAV_STATE::MAV_STATE_UNINIT;
            break;

        }
        int custom_mode = status.uplink_connected ?
                mavlink::ugcs::MAV_CUSTOM_MODE_FLAG::MAV_CUSTOM_MODE_FLAG_UPLINK_CONNECTED :
                0;
        custom_mode |= status.downlink_connected ?
                mavlink::ugcs::MAV_CUSTOM_MODE_FLAG::MAV_CUSTOM_MODE_FLAG_DOWNLINK_CONNECTED :
                0;

        if (vehicle->Read_reset_altitude_origin()) {
            custom_mode |= mavlink::ugcs::MAV_CUSTOM_MODE_FLAG::MAV_CUSTOM_MODE_FLAG_RESET_ALTITUDE_ORIGIN;
        }

        auto capabilities = vehicle->Get_capabilities();
        auto capability_states = vehicle->Get_capability_states();

#define __TRANSLATE_CAPABILITY(__capability, __mav_capability) \
        capabilities.Is_set(__capability) ? \
                mavlink::ugcs::MAV_CUSTOM_MODE_FLAG::__mav_capability : \
                0;

#define __TRANSLATE_CAPABILITY_STATE(__capability, __mav_capability) \
        capability_states.Is_set(__capability) ? \
                mavlink::ugcs::MAV_CUSTOM_MODE_FLAG::__mav_capability : \
                0;

        /* Capabilities. */
        custom_mode |= __TRANSLATE_CAPABILITY(Vehicle::Capability::ARM_AVAILABLE,
                MAV_CUSTOM_MODE_FLAG_ARM_AVAILABLE);

        custom_mode |= __TRANSLATE_CAPABILITY(Vehicle::Capability::DISARM_AVAILABLE,
                MAV_CUSTOM_MODE_FLAG_DISARM_AVAILABLE);

        custom_mode |= __TRANSLATE_CAPABILITY(Vehicle::Capability::AUTO_MODE_AVAILABLE,
                MAV_CUSTOM_MODE_FLAG_AUTO_MODE_AVAILABLE);

        custom_mode |= __TRANSLATE_CAPABILITY(Vehicle::Capability::MANUAL_MODE_AVAILABLE,
                MAV_CUSTOM_MODE_FLAG_MANUAL_MODE_AVAILABLE);

        custom_mode |= __TRANSLATE_CAPABILITY(Vehicle::Capability::RETURN_HOME_AVAILABLE,
                MAV_CUSTOM_MODE_FLAG_RETURN_HOME_AVAILABLE);

        custom_mode |= __TRANSLATE_CAPABILITY(Vehicle::Capability::TAKEOFF_AVAILABLE,
                MAV_CUSTOM_MODE_FLAG_TAKEOFF_AVAILABLE);

        custom_mode |= __TRANSLATE_CAPABILITY(Vehicle::Capability::LAND_AVAILABLE,
                MAV_CUSTOM_MODE_FLAG_LAND_AVAILABLE);

        custom_mode |= __TRANSLATE_CAPABILITY(Vehicle::Capability::EMERGENCY_LAND_AVAILABLE,
                MAV_CUSTOM_MODE_FLAG_EMERGENCY_LAND_AVAILABLE);

        custom_mode |= __TRANSLATE_CAPABILITY(Vehicle::Capability::CAMERA_TRIGGER_AVAILABLE,
                MAV_CUSTOM_MODE_FLAG_CAMERA_TRIGGER_AVAILABLE);

        custom_mode |= __TRANSLATE_CAPABILITY(Vehicle::Capability::WAYPOINT_AVAILABLE,
                MAV_CUSTOM_MODE_FLAG_WAYPOINT_AVAILABLE);

        custom_mode |= __TRANSLATE_CAPABILITY(Vehicle::Capability::PAUSE_MISSION_AVAILABLE,
                MAV_CUSTOM_MODE_FLAG_HOLD_AVAILABLE);

        custom_mode |= __TRANSLATE_CAPABILITY(Vehicle::Capability::RESUME_MISSION_AVAILABLE,
                MAV_CUSTOM_MODE_FLAG_CONTINUE_AVAILABLE);


        /* Capability states. */
        custom_mode |= __TRANSLATE_CAPABILITY_STATE(Vehicle::Capability_state::ARM_ENABLED,
                MAV_CUSTOM_MODE_FLAG_ARM_ENABLED);

        custom_mode |= __TRANSLATE_CAPABILITY_STATE(Vehicle::Capability_state::DISARM_ENABLED,
                MAV_CUSTOM_MODE_FLAG_DISARM_ENABLED);

        custom_mode |= __TRANSLATE_CAPABILITY_STATE(Vehicle::Capability_state::AUTO_MODE_ENABLED,
                MAV_CUSTOM_MODE_FLAG_AUTO_MODE_ENABLED);

        custom_mode |= __TRANSLATE_CAPABILITY_STATE(Vehicle::Capability_state::MANUAL_MODE_ENABLED,
                MAV_CUSTOM_MODE_FLAG_MANUAL_MODE_ENABLED);

        custom_mode |= __TRANSLATE_CAPABILITY_STATE(Vehicle::Capability_state::RETURN_HOME_ENABLED,
                MAV_CUSTOM_MODE_FLAG_RETURN_HOME_ENABLED);

        custom_mode |= __TRANSLATE_CAPABILITY_STATE(Vehicle::Capability_state::TAKEOFF_ENABLED,
                MAV_CUSTOM_MODE_FLAG_TAKEOFF_ENABLED);

        custom_mode |= __TRANSLATE_CAPABILITY_STATE(Vehicle::Capability_state::LAND_ENABLED,
                MAV_CUSTOM_MODE_FLAG_LAND_ENABLED);

        custom_mode |= __TRANSLATE_CAPABILITY_STATE(Vehicle::Capability_state::EMERGENCY_LAND_ENABLED,
                MAV_CUSTOM_MODE_FLAG_EMERGENCY_LAND_ENABLED);

        custom_mode |= __TRANSLATE_CAPABILITY_STATE(Vehicle::Capability_state::CAMERA_TRIGGER_ENABLED,
                MAV_CUSTOM_MODE_FLAG_CAMERA_TRIGGER_ENABLED);

        custom_mode |= __TRANSLATE_CAPABILITY_STATE(Vehicle::Capability_state::WAYPOINT_ENABLED,
                MAV_CUSTOM_MODE_FLAG_WAYPOINT_ENABLED);

        custom_mode |= __TRANSLATE_CAPABILITY_STATE(Vehicle::Capability_state::PAUSE_MISSION_ENABLED,
                MAV_CUSTOM_MODE_FLAG_HOLD_ENABLED);

        custom_mode |= __TRANSLATE_CAPABILITY_STATE(Vehicle::Capability_state::RESUME_MISSION_ENABLED,
                MAV_CUSTOM_MODE_FLAG_CONTINUE_ENABLED);

#undef __TRANSLATE_CAPABILITY
#undef __TRANSLATE_CAPABILITY_STATE

        hb->custom_mode = static_cast<mavlink::ugcs::MAV_CUSTOM_MODE_FLAG>(custom_mode);
        hb->type = vehicle->type;
        Send_message(iter.first, hb, vehicle->system_id, DEFAULT_COMPONENT_ID);
    }
}

void
Cucs_processor::Register_vehicle_telemetry(Ucs_vehicle_ctx::Ptr ctx)
{
    Telemetry_interface telemetry;
    /* Weak pointer to make sure telemetry registration does not prevent the
     * context to be fully deleted after it is disabled from the processor
     * context.
     */
    Ucs_vehicle_ctx::Weak_ptr weak_ctx = ctx;

#define __REGISTER_TELEMETRY_LOCAL(PAYLOAD_TYPE, field_name) \
    telemetry.field_name = Make_callback( \
                &Cucs_processor::On_telemetry<mavlink::PAYLOAD_TYPE>, \
                Shared_from_this(), \
                static_cast<const mavlink::PAYLOAD_TYPE*>(nullptr), \
                weak_ctx);

    __REGISTER_TELEMETRY_LOCAL(Pld_global_position_int, global_position);
    __REGISTER_TELEMETRY_LOCAL(Pld_attitude, attitude);
    __REGISTER_TELEMETRY_LOCAL(Pld_gps_raw_int, gps_raw);
    __REGISTER_TELEMETRY_LOCAL(Pld_raw_imu, raw_imu);
    __REGISTER_TELEMETRY_LOCAL(Pld_scaled_pressure, scaled_pressure);
    __REGISTER_TELEMETRY_LOCAL(Pld_vfr_hud, vfr_hud);
    __REGISTER_TELEMETRY_LOCAL(Pld_sys_status, sys_status);
    __REGISTER_TELEMETRY_LOCAL(ugcs::Pld_camera_attitude, camera_attitude);

#undef __REGISTER_TELEMETRY_LOCAL

    telemetry.sys_status_update = Make_callback(
            &Cucs_processor::On_sys_status_update,
            Shared_from_this(),
            weak_ctx);

    ctx->Get_vehicle()->Register_telemetry_interface(telemetry);
}

void
Cucs_processor::Unregister_vehicle_telemetry(Ucs_vehicle_ctx::Ptr ctx)
{
    ctx->Get_vehicle()->Unregister_telemetry_interface();
}

void
Cucs_processor::On_sys_status_update(Ucs_vehicle_ctx::Weak_ptr ctx)
{
    auto request = Request::Create();
    auto proc_handler = Make_callback(
            &Cucs_processor::On_sys_status_update_handle,
            Shared_from_this(),
            ctx,
            request);
    request->Set_processing_handler(proc_handler);
    Submit_request(request);
}

void
Cucs_processor::Broadcast_mavlink_message(
        const mavlink::Payload_base& message,
        Mavlink_demuxer::System_id system_id,
        Mavlink_demuxer::Component_id component_id)
{
    for (auto& iter: ucs_connections) {
        Send_message(iter.first, message, system_id, component_id);
    }
}

void
Cucs_processor::On_sys_status_update_handle(
        Ucs_vehicle_ctx::Weak_ptr ctx,
        Request::Ptr request)
{
    Ucs_vehicle_ctx::Ptr strong_ctx = ctx.lock();
    if (strong_ctx) {
        ASSERT(strong_ctx->Get_vehicle());
        Send_heartbeat(strong_ctx->Get_vehicle().get());
    }
    request->Complete();
}

void
Cucs_processor::Send_message(
        const Ucs_vehicle_ctx::Ugcs_mavlink_stream::Ptr mav_stream,
        const mavlink::Payload_base& payload,
        typename mavlink::Mavlink_kind_ugcs::System_id system_id,
        uint8_t component_id)
{
    mav_stream->Send_message(
            payload,
            system_id,
            component_id,
            WRITE_TIMEOUT,
            Make_timeout_callback(
                    &Cucs_processor::Write_to_ucs_timed_out,
                    Shared_from_this(),
                    mav_stream),
            completion_ctx);
}

void
Cucs_processor::Write_to_ucs_timed_out(
        const Operation_waiter::Ptr& waiter,
        Ucs_vehicle_ctx::Ugcs_mavlink_stream::Weak_ptr mav_stream)
{
    auto locked = mav_stream.lock();
    Io_stream::Ref stream = locked ? locked->Get_stream() : nullptr;
    std::string server_info =
            stream ? stream->Get_name() : "already disconnected";
    LOG_DEBUG("Write timeout towards UCS server at [%s] detected from UCS processor.",
            server_info.c_str());
    waiter->Abort();
    if (stream) {
        stream->Close();
    }
}

void
Cucs_processor::Register_on_new_ucs_connection(Callback_proxy<void> On_new_connection_handler) {
	LOG_DEBUG("Registering new callback proxy for 'On new CUCS connection' event.");
	on_new_connection_callback_proxy = On_new_connection_handler;
}
