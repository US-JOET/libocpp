#include "database_handler_mock.hpp"
#include "evse_security_mock.hpp"
#include "ocpp/v201/charge_point.hpp"
#include "ocpp/v201/device_model_storage_sqlite.hpp"
#include "gmock/gmock.h"
#include <gmock/gmock.h>

namespace ocpp::v201 {


//TODO: Stolen from test_component_state_manager.cpp. Common mock header?
class DatabaseHandlerMock : public DatabaseHandler {
private:
    std::map<std::pair<int32_t, int32_t>, OperationalStatusEnum> data;

    void insert(int32_t evse_id, int32_t connector_id, OperationalStatusEnum status, bool replace) {
        if (replace || this->data.count(std::make_pair(evse_id, connector_id)) == 0) {
            this->data.insert_or_assign(std::make_pair(evse_id, connector_id), status);
        }
    }

    OperationalStatusEnum get(int32_t evse_id, int32_t connector_id) {
        if (this->data.count(std::make_pair(evse_id, connector_id)) == 0) {
            throw std::logic_error("Get: no data available");
        } else {
            return this->data.at(std::make_pair(evse_id, connector_id));
        }
    }

public:
    DatabaseHandlerMock() : DatabaseHandler(std::unique_ptr<common::DatabaseConnectionInterface>(), "/dev/null") {
    }

    virtual void insert_cs_availability(OperationalStatusEnum operational_status, bool replace) override {
        this->insert(0, 0, operational_status, replace);
    }
    virtual OperationalStatusEnum get_cs_availability() override {
        return this->get(0, 0);
    }

    virtual void insert_evse_availability(int32_t evse_id, OperationalStatusEnum operational_status,
                                          bool replace) override {

        this->insert(evse_id, 0, operational_status, replace);
    }
    virtual OperationalStatusEnum get_evse_availability(int32_t evse_id) override {
        return this->get(evse_id, 0);
    }

    virtual void insert_connector_availability(int32_t evse_id, int32_t connector_id,
                                               OperationalStatusEnum operational_status, bool replace) override {
        this->insert(evse_id, connector_id, operational_status, replace);
    }
    virtual OperationalStatusEnum get_connector_availability(int32_t evse_id, int32_t connector_id) override {
        return this->get(evse_id, connector_id);
    }
};


class ChargePointFixture : public testing::Test {
public:
    ChargePointFixture() {
    }
    ~ChargePointFixture() {
    }

    //TODO: Device model helpers tolen from test_smart_charging_handler. Put into common helper file?
    void create_device_model_db(const std::string& path) {
        sqlite3* source_handle;
        sqlite3_open(DEVICE_MODEL_DB_LOCATION_V201, &source_handle);
        sqlite3_open(path.c_str(), &db_handle);

        auto* backup = sqlite3_backup_init(db_handle, "main", source_handle, "main");
        sqlite3_backup_step(backup, -1);
        sqlite3_backup_finish(backup);

        sqlite3_close(source_handle);
    }

    std::shared_ptr<DeviceModel>
    create_device_model(const std::string& path = "file:device_model?mode=memory&cache=shared",
                        const std::optional<std::string> ac_phase_switching_supported = "true") {
        create_device_model_db(path);
        auto device_model_storage = std::make_unique<DeviceModelStorageSqlite>(path);
        auto device_model = std::make_shared<DeviceModel>(std::move(device_model_storage));

        // Defaults
        const auto& charging_rate_unit_cv = ControllerComponentVariables::ChargingScheduleChargingRateUnit;
        device_model->set_value(charging_rate_unit_cv.component, charging_rate_unit_cv.variable.value(),
                                AttributeEnum::Actual, "A,W", true);

        const auto& ac_phase_switching_cv = ControllerComponentVariables::ACPhaseSwitchingSupported;
        device_model->set_value(ac_phase_switching_cv.component, ac_phase_switching_cv.variable.value(),
                                AttributeEnum::Actual, ac_phase_switching_supported.value_or(""), true);

        return device_model;
    }

    void configure_callbacks_with_mocks() {
        callbacks.is_reset_allowed_callback = is_reset_allowed_callback_mock.AsStdFunction();
        callbacks.reset_callback = reset_callback_mock.AsStdFunction();
        callbacks.stop_transaction_callback = stop_transaction_callback_mock.AsStdFunction();
        callbacks.pause_charging_callback = pause_charging_callback_mock.AsStdFunction();
        callbacks.connector_effective_operative_status_changed_callback =
            connector_effective_operative_status_changed_callback_mock.AsStdFunction();
        callbacks.get_log_request_callback = get_log_request_callback_mock.AsStdFunction();
        callbacks.unlock_connector_callback = unlock_connector_callback_mock.AsStdFunction();
        callbacks.remote_start_transaction_callback = remote_start_transaction_callback_mock.AsStdFunction();
        callbacks.is_reservation_for_token_callback = is_reservation_for_token_callback_mock.AsStdFunction();
        callbacks.update_firmware_request_callback = update_firmware_request_callback_mock.AsStdFunction();
        callbacks.security_event_callback = security_event_callback_mock.AsStdFunction();
        callbacks.set_charging_profiles_callback = set_charging_profiles_callback_mock.AsStdFunction();
    }

    testing::MockFunction<bool(const std::optional<const int32_t> evse_id, const ResetEnum& reset_type)>
        is_reset_allowed_callback_mock;
    testing::MockFunction<void(const std::optional<const int32_t> evse_id, const ResetEnum& reset_type)>
        reset_callback_mock;
    testing::MockFunction<void(const int32_t evse_id, const ReasonEnum& stop_reason)> stop_transaction_callback_mock;
    testing::MockFunction<void(const int32_t evse_id)> pause_charging_callback_mock;
    testing::MockFunction<void(const int32_t evse_id, const int32_t connector_id,
                               const OperationalStatusEnum new_status)>
        connector_effective_operative_status_changed_callback_mock;
    testing::MockFunction<GetLogResponse(const GetLogRequest& request)> get_log_request_callback_mock;
    testing::MockFunction<UnlockConnectorResponse(const int32_t evse_id, const int32_t connecor_id)>
        unlock_connector_callback_mock;
    testing::MockFunction<void(const RequestStartTransactionRequest& request, const bool authorize_remote_start)>
        remote_start_transaction_callback_mock;
    testing::MockFunction<bool(const int32_t evse_id, const CiString<36> idToken,
                               const std::optional<CiString<36>> groupIdToken)>
        is_reservation_for_token_callback_mock;
    testing::MockFunction<UpdateFirmwareResponse(const UpdateFirmwareRequest& request)>
        update_firmware_request_callback_mock;
    testing::MockFunction<void(const CiString<50>& event_type, const std::optional<CiString<255>>& tech_info)>
        security_event_callback_mock;
    testing::MockFunction<void()> set_charging_profiles_callback_mock;
    ocpp::v201::Callbacks callbacks;
    sqlite3* db_handle;
    std::shared_ptr<DeviceModel> device_model;
};

/*
 * K01.FR.02 states
 *
 *     "The CSMS MAY send a new charging profile for the EVSE that SHALL be used
 *      as a limit schedule for the EV."
 *
 * When using libocpp, a charging station is notified of a new charging profile
 * by means of the set_charging_profiles_callback. In order to ensure that a new
 * profile can be immediately "used as a limit schedule for the EV", a
 * valid set_charging_profiles_callback must be provided.
 *
 * As part of testing that K01.FR.02 is met, we provide the following tests that
 * confirm an OCPP 2.0.1 ChargePoint with smart charging enabled will only
 * consider its collection of callbacks valid if set_charging_profiles_callback
 * is provided.
 */

TEST_F(ChargePointFixture, CreateChargePoint) {
    device_model = create_device_model();
    auto database_handler = 
        std::make_shared<DatabaseHandlerMock>();
    auto evse_security = std::make_shared<EvseSecurityMock>();
    configure_callbacks_with_mocks();

    const auto DEFAULT_MESSAGE_QUEUE_SIZE_THRESHOLD = 2E5;

    //TODO: Look at init_message_queue and SetUp in test_message_queue.
    // Currently something is wrong as it either hangs if I do nothing or crashes if I try to use auth_cache_cleanup_thread.
    auto message_queue = std::make_shared<ocpp::MessageQueue<v201::MessageType>>(
        [this](json message) -> bool { return false; },
        MessageQueueConfig{
            this->device_model->get_value<int>(ControllerComponentVariables::MessageAttempts),
            this->device_model->get_value<int>(ControllerComponentVariables::MessageAttemptInterval),
            this->device_model->get_optional_value<int>(ControllerComponentVariables::MessageQueueSizeThreshold)
                .value_or(DEFAULT_MESSAGE_QUEUE_SIZE_THRESHOLD),
            this->device_model->get_optional_value<bool>(ControllerComponentVariables::QueueAllMessages)
                .value_or(false),
            this->device_model->get_value<int>(ControllerComponentVariables::MessageTimeout)},
        database_handler);

    ocpp::v201::ChargePoint chargePoint(device_model, database_handler, message_queue, evse_security, callbacks);
    EXPECT_TRUE(true);
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfSetChargingProfilesCallbackExists) {
    configure_callbacks_with_mocks();
    callbacks.set_charging_profiles_callback = nullptr;

    EXPECT_FALSE(callbacks.all_callbacks_valid());
}

/*
 * For completeness, we also test that all other callbacks are checked by
 * all_callbacks_valid.
 */

TEST_F(ChargePointFixture, K01FR02_CallbacksAreInvalidWhenNotProvided) {
    EXPECT_FALSE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksAreValidWhenAllRequiredCallbacksProvided) {
    configure_callbacks_with_mocks();
    EXPECT_TRUE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfResetIsAllowedCallbackExists) {
    configure_callbacks_with_mocks();
    callbacks.is_reset_allowed_callback = nullptr;

    EXPECT_FALSE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfResetCallbackExists) {
    configure_callbacks_with_mocks();
    callbacks.reset_callback = nullptr;

    EXPECT_FALSE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfStopTransactionCallbackExists) {
    configure_callbacks_with_mocks();
    callbacks.stop_transaction_callback = nullptr;

    EXPECT_FALSE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfPauseChargingCallbackExists) {
    configure_callbacks_with_mocks();
    callbacks.pause_charging_callback = nullptr;

    EXPECT_FALSE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfConnectorEffectiveOperativeStatusChangedCallbackExists) {
    configure_callbacks_with_mocks();
    callbacks.connector_effective_operative_status_changed_callback = nullptr;

    EXPECT_FALSE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfGetLogRequestCallbackExists) {
    configure_callbacks_with_mocks();
    callbacks.get_log_request_callback = nullptr;

    EXPECT_FALSE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfUnlockConnectorCallbackExists) {
    configure_callbacks_with_mocks();
    callbacks.unlock_connector_callback = nullptr;

    EXPECT_FALSE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfRemoteStartTransactionCallbackExists) {
    configure_callbacks_with_mocks();
    callbacks.remote_start_transaction_callback = nullptr;

    EXPECT_FALSE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfIsReservationForTokenCallbackExists) {
    configure_callbacks_with_mocks();
    callbacks.is_reservation_for_token_callback = nullptr;

    EXPECT_FALSE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfUpdateFirmwareRequestCallbackExists) {
    configure_callbacks_with_mocks();
    callbacks.update_firmware_request_callback = nullptr;

    EXPECT_FALSE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfSecurityEventCallbackExists) {
    configure_callbacks_with_mocks();
    callbacks.security_event_callback = nullptr;

    EXPECT_FALSE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfOptionalVariableChangedCallbackIsNotSetOrNotNull) {
    configure_callbacks_with_mocks();

    callbacks.variable_changed_callback = nullptr;
    EXPECT_FALSE(callbacks.all_callbacks_valid());

    testing::MockFunction<void(const SetVariableData& set_variable_data)> variable_changed_callback_mock;
    callbacks.variable_changed_callback = variable_changed_callback_mock.AsStdFunction();
    EXPECT_TRUE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfOptionalVariableNetworkProfileCallbackIsNotSetOrNotNull) {
    configure_callbacks_with_mocks();

    callbacks.validate_network_profile_callback = nullptr;
    EXPECT_FALSE(callbacks.all_callbacks_valid());

    testing::MockFunction<SetNetworkProfileStatusEnum(const int32_t configuration_slot,
                                                      const NetworkConnectionProfile& network_connection_profile)>
        validate_network_profile_callback_mock;
    callbacks.validate_network_profile_callback = validate_network_profile_callback_mock.AsStdFunction();
    EXPECT_TRUE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture,
       K01FR02_CallbacksValidityChecksIfOptionalConfigureNetworkConnectionProfileCallbackIsNotSetOrNotNull) {
    configure_callbacks_with_mocks();

    callbacks.configure_network_connection_profile_callback = nullptr;
    EXPECT_FALSE(callbacks.all_callbacks_valid());

    testing::MockFunction<bool(const NetworkConnectionProfile& network_connection_profile)>
        configure_network_connection_profile_callback_mock;
    callbacks.configure_network_connection_profile_callback =
        configure_network_connection_profile_callback_mock.AsStdFunction();
    EXPECT_TRUE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfOptionalTimeSyncCallbackIsNotSetOrNotNull) {
    configure_callbacks_with_mocks();

    callbacks.time_sync_callback = nullptr;
    EXPECT_FALSE(callbacks.all_callbacks_valid());

    testing::MockFunction<void(const ocpp::DateTime& currentTime)> time_sync_callback_mock;
    callbacks.time_sync_callback = time_sync_callback_mock.AsStdFunction();
    EXPECT_TRUE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfOptionalBootNotificationCallbackIsNotSetOrNotNull) {
    configure_callbacks_with_mocks();

    callbacks.boot_notification_callback = nullptr;
    EXPECT_FALSE(callbacks.all_callbacks_valid());

    testing::MockFunction<void(const ocpp::v201::BootNotificationResponse& response)> boot_notification_callback_mock;
    callbacks.boot_notification_callback = boot_notification_callback_mock.AsStdFunction();
    EXPECT_TRUE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfOptionalOCPPMessagesCallbackIsNotSetOrNotNull) {
    configure_callbacks_with_mocks();

    callbacks.ocpp_messages_callback = nullptr;
    EXPECT_FALSE(callbacks.all_callbacks_valid());

    testing::MockFunction<void(const std::string& message, MessageDirection direction)> ocpp_messages_callback_mock;
    callbacks.ocpp_messages_callback = ocpp_messages_callback_mock.AsStdFunction();
    EXPECT_TRUE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture,
       K01FR02_CallbacksValidityChecksIfOptionalCSEffectiveOperativeStatusChangedCallbackIsNotSetOrNotNull) {
    configure_callbacks_with_mocks();

    callbacks.cs_effective_operative_status_changed_callback = nullptr;
    EXPECT_FALSE(callbacks.all_callbacks_valid());

    testing::MockFunction<void(const OperationalStatusEnum new_status)>
        cs_effective_operative_status_changed_callback_mock;
    callbacks.cs_effective_operative_status_changed_callback =
        cs_effective_operative_status_changed_callback_mock.AsStdFunction();
    EXPECT_TRUE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture,
       K01FR02_CallbacksValidityChecksIfOptionalEvseEffectiveOperativeStatusChangedCallbackIsNotSetOrNotNull) {
    configure_callbacks_with_mocks();

    callbacks.evse_effective_operative_status_changed_callback = nullptr;
    EXPECT_FALSE(callbacks.all_callbacks_valid());

    testing::MockFunction<void(const int32_t evse_id, const OperationalStatusEnum new_status)>
        evse_effective_operative_status_changed_callback_mock;
    callbacks.evse_effective_operative_status_changed_callback =
        evse_effective_operative_status_changed_callback_mock.AsStdFunction();
    EXPECT_TRUE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfOptionalGetCustomerInformationCallbackIsNotSetOrNotNull) {
    configure_callbacks_with_mocks();

    callbacks.get_customer_information_callback = nullptr;
    EXPECT_FALSE(callbacks.all_callbacks_valid());

    testing::MockFunction<std::string(const std::optional<CertificateHashDataType> customer_certificate,
                                      const std::optional<IdToken> id_token,
                                      const std::optional<CiString<64>> customer_identifier)>
        get_customer_information_callback_mock;
    callbacks.get_customer_information_callback = get_customer_information_callback_mock.AsStdFunction();
    EXPECT_TRUE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfOptionalClearCustomerInformationCallbackIsNotSetOrNotNull) {
    configure_callbacks_with_mocks();

    callbacks.clear_customer_information_callback = nullptr;
    EXPECT_FALSE(callbacks.all_callbacks_valid());

    testing::MockFunction<std::string(const std::optional<CertificateHashDataType> customer_certificate,
                                      const std::optional<IdToken> id_token,
                                      const std::optional<CiString<64>> customer_identifier)>
        clear_customer_information_callback_mock;
    callbacks.clear_customer_information_callback = clear_customer_information_callback_mock.AsStdFunction();
    EXPECT_TRUE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfOptionalAllConnectorsUnavailableCallbackIsNotSetOrNotNull) {
    configure_callbacks_with_mocks();

    callbacks.all_connectors_unavailable_callback = nullptr;
    EXPECT_FALSE(callbacks.all_callbacks_valid());

    testing::MockFunction<void()> all_connectors_unavailable_callback_mock;
    callbacks.all_connectors_unavailable_callback = all_connectors_unavailable_callback_mock.AsStdFunction();
    EXPECT_TRUE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfOptionalDataTransferCallbackIsNotSetOrNotNull) {
    configure_callbacks_with_mocks();

    callbacks.data_transfer_callback = nullptr;
    EXPECT_FALSE(callbacks.all_callbacks_valid());

    testing::MockFunction<DataTransferResponse(const DataTransferRequest& request)> data_transfer_callback_mock;
    callbacks.data_transfer_callback = data_transfer_callback_mock.AsStdFunction();
    EXPECT_TRUE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfOptionalTransactionEventCallbackIsNotSetOrNotNull) {
    configure_callbacks_with_mocks();

    callbacks.transaction_event_callback = nullptr;
    EXPECT_FALSE(callbacks.all_callbacks_valid());

    testing::MockFunction<void(const TransactionEventRequest& transaction_event)> transaction_event_callback_mock;
    callbacks.transaction_event_callback = transaction_event_callback_mock.AsStdFunction();
    EXPECT_TRUE(callbacks.all_callbacks_valid());
}

TEST_F(ChargePointFixture, K01FR02_CallbacksValidityChecksIfOptionalTransactionEventResponseCallbackIsNotSetOrNotNull) {
    configure_callbacks_with_mocks();

    callbacks.transaction_event_response_callback = nullptr;
    EXPECT_FALSE(callbacks.all_callbacks_valid());

    testing::MockFunction<void(const TransactionEventRequest& transaction_event,
                               const TransactionEventResponse& transaction_event_response)>
        transaction_event_response_callback_mock;
    callbacks.transaction_event_response_callback = transaction_event_response_callback_mock.AsStdFunction();
    EXPECT_TRUE(callbacks.all_callbacks_valid());
}

} // namespace ocpp::v201
