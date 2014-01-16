// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <vsm/log.h>
#include <vsm/debug.h>
#include <vsm/cucs_processor.h>
#include <vsm/timer_processor.h>
#include <vsm/properties.h>

using namespace vsm;

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
        VSM_EXCEPTION(Invalid_param_exception, "Vehicle with ID %d already unregistered",
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
        LOG_WARNING("CUCS processor overload suspected, %ld pending ADS-B "
                "reports are present. Waiting for all reports to be completed...",
                pending_adsb_reports.load());
        Operation_waiter waiter(request);
        waiter.Wait(false);
        LOG_DEBUG("%ld pending ADS-B reports after waiting.",
                pending_adsb_reports.load());
    }
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
        LOG_ERR("%ld vehicles are still present in Cucs processor while disabling.",
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
        Mavlink_stream::Ptr mav_stream = Mavlink_stream::Create(
                stream, mavlink::ugcs::Extension::Get());

        mav_stream->Bind_decoder_demuxer();

        Register_mavlink_handlers(mav_stream);

        Operation_waiter& op_waiter = ucs_connections.emplace(
                mav_stream,
                Operation_waiter()).first->second;
        Schedule_next_read(mav_stream, op_waiter);
    } else {
        LOG_WARN("Incoming connection accept failed %d", result);
    }
    Accept_next_connection();
}

void
Cucs_processor::Schedule_next_read(
        Mavlink_stream::Ptr& mav_stream,
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
        Mavlink_stream::Ptr mav_stream)
{
    auto iter = ucs_connections.find(mav_stream);
    ASSERT(iter != ucs_connections.end());
    if (result == Io_result::OK) {
        mav_stream->Get_decoder().Decode(buffer);
        Schedule_next_read(mav_stream, iter->second);
    } else {
        LOG_WARN("UCS connection closed [%s], %lu vehicles are still registered.",
                mav_stream->Get_stream()->Get_name().c_str(),
                vehicles.size());
        mav_stream->Get_stream()->Close();
        /* Propagate stream disconnect event for all vehicle contexts. */
        for (auto& iter: vehicles) {
            iter.second->Stream_disconnected(mav_stream);
        }
        mav_stream->Disable();
        ucs_connections.erase(iter);
    }
}

void
Cucs_processor::Register_mavlink_handlers(Mavlink_stream::Ptr mav_stream)
{
    auto& demuxer = mav_stream->Get_demuxer();

    demuxer.Register_default_handler(
            Make_mavlink_demuxer_default_handler(
                    &Cucs_processor::On_default_mavlink_message_handler,
                    Shared_from_this(),
                    mav_stream));

    /* Here are all Mavlink messages supported by UCS processor and UCS
     * transactions. Other messages will end up in default handler.
     */
    Register_mavlink_message<mavlink::MESSAGE_ID::MISSION_CLEAR_ALL>(mav_stream);
    Register_mavlink_message<mavlink::MESSAGE_ID::MISSION_COUNT>(mav_stream);
    Register_mavlink_message<mavlink::MESSAGE_ID::COMMAND_LONG, mavlink::Extension>(mav_stream);
    Register_mavlink_message<mavlink::ugcs::MESSAGE_ID::MISSION_ITEM_EX,
        mavlink::ugcs::Extension>(mav_stream);

}

void
Cucs_processor::Start_listening()
{
    cucs_listener_op.Abort();
    auto props = Properties::Get_instance();
    cucs_listener_op = Socket_processor::Get_instance()->Listen(
            Socket_address::Create(
                    props->Get("cucs_processor.server_address"),
                    props->Get("cucs_processor.server_port")),
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

bool
Cucs_processor::On_default_mavlink_message_handler(
        mavlink::MESSAGE_ID_TYPE message_id,
        mavlink::System_id,
        uint8_t,
        Mavlink_stream::Ptr mav_stream)
{
    LOG_WARN("Unsupported Mavlink message %d, closing connection.", message_id);
    mav_stream->Get_stream()->Close();
    return false;
}

bool
Cucs_processor::On_heartbeat_timer()
{
    for (auto& vehicle_iter: vehicles) {
        auto vehicle = vehicle_iter.second->Get_vehicle();
        for (auto& iter: ucs_connections) {
            mavlink::Pld_heartbeat hb;
            hb->autopilot = vehicle->autopilot;
            hb->base_mode = vehicle->system_mode;
            int custom_mode = vehicle->custom_mode.uplink_connected ?
                    mavlink::ugcs::MAV_CUSTOM_MODE_FLAG::MAV_CUSTOM_MODE_FLAG_UPLINK_CONNECTED :
                    0;
            custom_mode |= vehicle->custom_mode.downlink_connected ?
                    mavlink::ugcs::MAV_CUSTOM_MODE_FLAG::MAV_CUSTOM_MODE_FLAG_DOWNLINK_CONNECTED :
                    0;
            hb->custom_mode = static_cast<mavlink::ugcs::MAV_CUSTOM_MODE_FLAG>(custom_mode);
            hb->system_status = vehicle->system_state;
            hb->type = vehicle->type;
            Send_message(iter.first, hb, vehicle->system_id, DEFAULT_COMPONENT_ID);
        }
    }
    return true;
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

#undef __REGISTER_TELEMETRY_LOCAL

    ctx->Get_vehicle()->Register_telemetry_interface(telemetry);
}

void
Cucs_processor::Unregister_vehicle_telemetry(Ucs_vehicle_ctx::Ptr ctx)
{
    ctx->Get_vehicle()->Unregister_telemetry_interface();
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
Cucs_processor::Send_message(
        const Mavlink_stream::Ptr mav_stream,
        const mavlink::Payload_base& payload,
        mavlink::System_id system_id,
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
        Mavlink_stream::Weak_ptr mav_stream)
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
