#include "date/tz.h"
#include "everest/logging.hpp"
#include "ocpp/v201/ctrlr_component_variables.hpp"
#include "ocpp/v201/device_model_storage_sqlite.hpp"
#include "ocpp/v201/ocpp_types.hpp"
#include "ocpp/v201/utils.hpp"
#include <algorithm>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdint>
#include <filesystem>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <sqlite3.h>
#include <string>

#include "ocpp/common/constants.hpp"
#include <component_state_manager_mock.hpp>
#include <device_model_storage_mock.hpp>
#include <evse_mock.hpp>
#include <evse_security_mock.hpp>
#include <ocpp/common/call_types.hpp>
#include <ocpp/v201/enums.hpp>
#include <ocpp/v201/evse.hpp>
#include <ocpp/v201/smart_charging.hpp>
#include <optional>

#include "smart_charging_test_utils.hpp"

#include <sstream>
#include <vector>

namespace ocpp::v201 {

static const int STATION_WIDE_ID = 0;
static const int DEFAULT_EVSE_ID = 1;
static const int DEFAULT_PROFILE_ID = 1;
static const int DEFAULT_STACK_LEVEL = 1;
static const DateTime DEFAULT_TRANSACTION_DATE_TIME = ocpp::DateTime("2024-05-20T17:00:00");
static const DateTime MOCKED_NOW = ocpp::DateTime("2024-06-05T14:00:00");

// static const std::string BASE_JSON_PATH = TEST_PROFILES_LOCATION_V201;

class TestSmartChargingHandler : public SmartChargingHandler {
public:
    TestSmartChargingHandler(std::map<int32_t, std::unique_ptr<EvseInterface>>& evses,
                             std::shared_ptr<DeviceModel>& device_model) :
        SmartChargingHandler(evses, device_model) {
    }

    ocpp::DateTime get_now() {
        return MOCKED_NOW;
    }
};

class ChargepointTestFixtureV201 : public testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
        sqlite3_close(this->db_handle);
    }

    ChargingSchedule create_charge_schedule(ChargingRateUnitEnum charging_rate_unit) {
        int32_t id;
        std::vector<ChargingSchedulePeriod> charging_schedule_period;
        std::optional<CustomData> custom_data;
        std::optional<ocpp::DateTime> start_schedule;
        std::optional<int32_t> duration;
        std::optional<float> min_charging_rate;
        std::optional<SalesTariff> sales_tariff;

        return ChargingSchedule{
            .id = id,
            .chargingRateUnit = charging_rate_unit,
            .chargingSchedulePeriod = charging_schedule_period,
            .customData = custom_data,
            .startSchedule = start_schedule,
            .duration = duration,
            .minChargingRate = min_charging_rate,
            .salesTariff = sales_tariff,
        };
    }

    ChargingSchedule create_charge_schedule(ChargingRateUnitEnum charging_rate_unit,
                                            std::vector<ChargingSchedulePeriod> charging_schedule_period,
                                            std::optional<ocpp::DateTime> start_schedule = std::nullopt) {
        int32_t id;
        std::optional<CustomData> custom_data;
        std::optional<int32_t> duration;
        std::optional<float> min_charging_rate;
        std::optional<SalesTariff> sales_tariff;

        return ChargingSchedule{
            .id = id,
            .chargingRateUnit = charging_rate_unit,
            .chargingSchedulePeriod = charging_schedule_period,
            .customData = custom_data,
            .startSchedule = start_schedule,
            .duration = duration,
            .minChargingRate = min_charging_rate,
            .salesTariff = sales_tariff,
        };
    }

    std::vector<ChargingSchedulePeriod>
    create_charging_schedule_periods(int32_t start_period, std::optional<int32_t> number_phases = std::nullopt,
                                     std::optional<int32_t> phase_to_use = std::nullopt) {
        auto charging_schedule_period = ChargingSchedulePeriod{
            .startPeriod = start_period,
            .numberPhases = number_phases,
            .phaseToUse = phase_to_use,
        };

        return {charging_schedule_period};
    }

    std::vector<ChargingSchedulePeriod> create_charging_schedule_periods(std::vector<int32_t> start_periods) {
        auto charging_schedule_periods = std::vector<ChargingSchedulePeriod>();
        for (auto start_period : start_periods) {
            auto charging_schedule_period = ChargingSchedulePeriod{
                .startPeriod = start_period,
            };
            charging_schedule_periods.push_back(charging_schedule_period);
        }

        return charging_schedule_periods;
    }

    std::vector<ChargingSchedulePeriod>
    create_charging_schedule_periods_with_phases(int32_t start_period, int32_t numberPhases, int32_t phaseToUse) {
        auto charging_schedule_period =
            ChargingSchedulePeriod{.startPeriod = start_period, .numberPhases = numberPhases, .phaseToUse = phaseToUse};

        return {charging_schedule_period};
    }

    ChargingProfile
    create_charging_profile(int32_t charging_profile_id, ChargingProfilePurposeEnum charging_profile_purpose,
                            ChargingSchedule charging_schedule, std::string transaction_id,
                            ChargingProfileKindEnum charging_profile_kind = ChargingProfileKindEnum::Absolute,
                            int stack_level = DEFAULT_STACK_LEVEL) {
        auto recurrency_kind = RecurrencyKindEnum::Daily;
        std::vector<ChargingSchedule> charging_schedules = {charging_schedule};
        return ChargingProfile{.id = charging_profile_id,
                               .stackLevel = stack_level,
                               .chargingProfilePurpose = charging_profile_purpose,
                               .chargingProfileKind = charging_profile_kind,
                               .chargingSchedule = charging_schedules,
                               .customData = {},
                               .recurrencyKind = recurrency_kind,
                               .validFrom = {},
                               .validTo = {},
                               .transactionId = transaction_id};
    }

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

    void create_evse_with_id(int id) {
        testing::MockFunction<void(const MeterValue& meter_value, const Transaction& transaction, const int32_t seq_no,
                                   const std::optional<int32_t> reservation_id)>
            transaction_meter_value_req_mock;
        testing::MockFunction<void()> pause_charging_callback_mock;
        auto e1 = std::make_unique<Evse>(
            id, 1, *device_model, database_handler, std::make_shared<ComponentStateManagerMock>(),
            transaction_meter_value_req_mock.AsStdFunction(), pause_charging_callback_mock.AsStdFunction());
        evses[id] = std::move(e1);
    }

    TestSmartChargingHandler create_smart_charging_handler() {
        return TestSmartChargingHandler(evses, device_model);
    }

    std::string uuid() {
        std::stringstream s;
        s << uuid_generator();
        return s.str();
    }

    void open_evse_transaction(int evse_id, std::string transaction_id) {
        auto connector_id = 1;
        auto meter_start = MeterValue();
        auto id_token = IdToken();
        evses[evse_id]->open_transaction(
            transaction_id, connector_id, DEFAULT_TRANSACTION_DATE_TIME, meter_start, id_token, {}, {},
            std::chrono::seconds(static_cast<int64_t>(1)), std::chrono::seconds(static_cast<int64_t>(1)),
            std::chrono::seconds(static_cast<int64_t>(1)), std::chrono::seconds(static_cast<int64_t>(1)));
    }

    void install_profile_on_evse(int evse_id, int profile_id) {
        if (evse_id != STATION_WIDE_ID) {
            create_evse_with_id(evse_id);
        }
        auto existing_profile = create_charging_profile(profile_id, ChargingProfilePurposeEnum::TxDefaultProfile,
                                                        create_charge_schedule(ChargingRateUnitEnum::A), uuid());
        handler.add_profile(evse_id, existing_profile);
    }

    void log_me(std::vector<ChargingProfile> profiles) {
        EVLOG_info << "[";
        for (auto& profile : profiles) {
            EVLOG_info << "  ChargingProfile> " << utils::to_string(profile);
        }
        EVLOG_info << "]";
    }

    // Default values used within the tests
    std::map<int32_t, std::unique_ptr<EvseInterface>> evses;
    std::shared_ptr<DatabaseHandler> database_handler;

    sqlite3* db_handle;

    bool ignore_no_transaction = true;
    std::shared_ptr<DeviceModel> device_model = create_device_model();
    TestSmartChargingHandler handler = create_smart_charging_handler();
    boost::uuids::random_generator uuid_generator = boost::uuids::random_generator();
};

// Based upon K01.FR11 K01.FR38
TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_GetPeriodEndTime_PAIN) {
    GTEST_SKIP();
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_FoundationTest_Grid) {
    GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);

    std::vector<ChargingProfile> profiles =
        SmartChargingTestUtils::get_charging_profiles_from_directory(BASE_JSON_PATH + "/grid/");

    const DateTime start_time = ocpp::DateTime("2024-01-17T00:00:00");
    const DateTime end_time = ocpp::DateTime("2024-01-18T00:00:00");

    CompositeSchedule composite_schedule =
        handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);

    EVLOG_info << "CompositeSchedule> " << utils::to_string(composite_schedule);
    ASSERT_EQ(start_time, composite_schedule.scheduleStart);
    ASSERT_EQ(composite_schedule.chargingSchedulePeriod.size(), 24);
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_LayeredTest_SameStartTime) {
    GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);
    std::vector<ChargingProfile> profiles =
        SmartChargingTestUtils::get_charging_profiles_from_directory(BASE_JSON_PATH + "/layered/");

    // Time Window: START = Stack #1 start time || END = Stack #1 end time
    {
        const DateTime start_time = ocpp::DateTime("2024-01-18T18:04:00");
        const DateTime end_time = ocpp::DateTime("2024-01-18T18:22:00");

        CompositeSchedule expected = {
            .chargingSchedulePeriod = {{
                .startPeriod = 0,
                .limit = 19.0,
                .numberPhases = 1,
            }},
            .evseId = DEFAULT_EVSE_ID,
            .duration = 1080,
            .scheduleStart = start_time,
            .chargingRateUnit = ChargingRateUnitEnum::W,
        };

        CompositeSchedule actual = handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID,
                                                                        ChargingRateUnitEnum::W);

        ASSERT_EQ(actual, expected);
    }

    // Time Window: START = Stack #1 start time || END = After Stack #1 end time Before next Start #0 start time
    {
        const DateTime start_time = ocpp::DateTime("2024-01-17T18:04:00");
        const DateTime end_time = ocpp::DateTime("2024-01-17T18:33:00");

        CompositeSchedule expected = {
            .chargingSchedulePeriod = {{
                                           .startPeriod = 0,
                                           .limit = 2000.0,
                                           .numberPhases = 1,
                                       },
                                       {
                                           .startPeriod = 1080,
                                           .limit = 19.0,
                                           .numberPhases = 1,
                                       }},
            .evseId = DEFAULT_EVSE_ID,
            .duration = 1740,
            .scheduleStart = start_time,
            .chargingRateUnit = ChargingRateUnitEnum::W,
        };

        CompositeSchedule actual = handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID,
                                                                        ChargingRateUnitEnum::W);

        ASSERT_EQ(actual, expected);
    }

    // Time Window: START = Stack #1 start time || END = After next Start #0 start time
    {
        const DateTime start_time = ocpp::DateTime("2024-01-17T18:04:00");
        const DateTime end_time = ocpp::DateTime("2024-01-17T19:04:00");

        CompositeSchedule expected = {
            .chargingSchedulePeriod = {{
                                           .startPeriod = 0,
                                           .limit = 2000.0,
                                           .numberPhases = 1,
                                       },
                                       {
                                           .startPeriod = 1080,
                                           .limit = 19.0,
                                           .numberPhases = 1,
                                       },
                                       {
                                           .startPeriod = 3360,
                                           .limit = 20.0,
                                           .numberPhases = 1,
                                       }},
            .evseId = DEFAULT_EVSE_ID,
            .duration = 3600,
            .scheduleStart = start_time,
            .chargingRateUnit = ChargingRateUnitEnum::W,
        };

        CompositeSchedule actual = handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID,
                                                                        ChargingRateUnitEnum::W);

        ASSERT_EQ(actual, expected);
    }
}

// TODO: Fix me
TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_LayeredTest_FutureStartTime) {
    GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);
    std::vector<ChargingProfile> profiles =
        SmartChargingTestUtils::get_charging_profiles_from_directory(BASE_JSON_PATH + "/layered_recurring/");

    const DateTime start_time = ocpp::DateTime("2024-02-17T18:04:00");
    const DateTime end_time = ocpp::DateTime("2024-02-17T18:05:00");

    CompositeSchedule expected = {
        .chargingSchedulePeriod = {{
            .startPeriod = 0,
            .limit = 2000.0,
            .numberPhases = 1,
        }},
        .evseId = DEFAULT_EVSE_ID,
        .duration = 60,
        .scheduleStart = start_time,
        .chargingRateUnit = ChargingRateUnitEnum::W,
    };

    CompositeSchedule actual =
        handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);

    ASSERT_EQ(actual, expected);
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_LayeredTest_PreviousStartTime) {
    GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);
    std::vector<ChargingProfile> profiles =
        SmartChargingTestUtils::get_charging_profiles_from_directory(BASE_JSON_PATH + "/null_start/");
    const DateTime start_time = ocpp::DateTime("2024-01-17T18:00:00");
    const DateTime end_time = ocpp::DateTime("2024-01-17T18:05:00");
    CompositeSchedule expected = {
        .chargingSchedulePeriod = {{
                                       .startPeriod = 0,
                                       .limit = DEFAULT_LIMIT_WATTS,
                                       .numberPhases = DEFAULT_AND_MAX_NUMBER_PHASES,
                                   },
                                   {
                                       .startPeriod = 240,
                                       .limit =
                                           profiles.at(0).chargingSchedule.front().chargingSchedulePeriod.front().limit,
                                       .numberPhases = 1,
                                   }},
        .evseId = DEFAULT_EVSE_ID,
        .duration = 300,
        .scheduleStart = start_time,
        .chargingRateUnit = ChargingRateUnitEnum::W,
    };

    CompositeSchedule actual =
        handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);

    ASSERT_EQ(actual, expected);
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_LayeredRecurringTest_PreviousStartTime) {
    GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);
    std::vector<ChargingProfile> profiles =
        SmartChargingTestUtils::get_charging_profiles_from_directory(BASE_JSON_PATH + "/layered_recurring/");
    const DateTime start_time = ocpp::DateTime("2024-02-19T18:00:00");
    const DateTime end_time = ocpp::DateTime("2024-02-19T19:04:00");
    CompositeSchedule expected = {
        .chargingSchedulePeriod =
            {{
                 .startPeriod = 0,
                 .limit = 19.0,
                 .numberPhases = 1,
             },
             {
                 .startPeriod = 240,
                 .limit = profiles.back().chargingSchedule.front().chargingSchedulePeriod.front().limit,
                 .numberPhases = 1,
             },
             {
                 .startPeriod = 1320,
                 .limit = 19.0,
                 .numberPhases = 1,
             },
             {
                 .startPeriod = 3600,
                 .limit = 20.0,
                 .numberPhases = 1,
             }},
        .evseId = DEFAULT_EVSE_ID,
        .duration = 3840,
        .scheduleStart = start_time,
        .chargingRateUnit = ChargingRateUnitEnum::W,
    };

    CompositeSchedule actual =
        handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);

    ASSERT_EQ(actual, expected);
}

/**
 * Calculate Composite Schedule
 */
TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_ValidateBaselineProfileVector) {
    GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);
    const DateTime start_time = ocpp::DateTime("2024-01-17T18:01:00");
    const DateTime end_time = ocpp::DateTime("2024-01-18T06:00:00");
    std::vector<ChargingProfile> profiles = SmartChargingTestUtils::get_baseline_profile_vector();
    CompositeSchedule expected = {
        .chargingSchedulePeriod = {{
                                       .startPeriod = 0,
                                       .limit = 2000.0,
                                       .numberPhases = 1,
                                   },
                                   {
                                       .startPeriod = 1020,
                                       .limit = 11000.0,
                                       .numberPhases = 3,
                                   },
                                   {
                                       .startPeriod = 25140,
                                       .limit = 6000.0,
                                       .numberPhases = 3,
                                   }},
        .evseId = DEFAULT_EVSE_ID,
        .duration = 43140,
        .scheduleStart = start_time,
        .chargingRateUnit = ChargingRateUnitEnum::W,
    };

    CompositeSchedule actual =
        handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);

    ASSERT_EQ(actual, expected);
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_RelativeProfile_minutia) {
    GTEST_SKIP();
    const DateTime start_time = ocpp::DateTime("2024-05-17T05:00:00");
    const DateTime end_time = ocpp::DateTime("2024-05-17T06:00:00");

    std::vector<ChargingProfile> profiles =
        SmartChargingTestUtils::get_charging_profiles_from_directory(BASE_JSON_PATH + "/relative/");
    create_evse_with_id(DEFAULT_EVSE_ID);
    open_evse_transaction(DEFAULT_EVSE_ID, profiles.at(0).transactionId.value());

    CompositeSchedule expected = {
        .chargingSchedulePeriod = {{
            .startPeriod = 0,
            .limit = 2000.0,
            .numberPhases = 1,
        }},
        .evseId = DEFAULT_EVSE_ID,
        .duration = 3600,
        .scheduleStart = start_time,
        .chargingRateUnit = ChargingRateUnitEnum::W,
    };

    CompositeSchedule actual =
        handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);

    ASSERT_EQ(actual, expected);
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_RelativeProfile_e2e) {
    GTEST_SKIP();
    std::vector<ChargingProfile> profiles =
        SmartChargingTestUtils::get_charging_profiles_from_directory(BASE_JSON_PATH + "/relative/");
    create_evse_with_id(DEFAULT_EVSE_ID);
    open_evse_transaction(DEFAULT_EVSE_ID, profiles.at(0).transactionId.value());
    const DateTime start_time = ocpp::DateTime("2024-05-17T05:00:00");
    const DateTime end_time = ocpp::DateTime("2024-05-17T06:01:00");

    CompositeSchedule expected = {
        .chargingSchedulePeriod = {{
                                       .startPeriod = 0,
                                       .limit = 2000.0,
                                       .numberPhases = 1,
                                   },
                                   {
                                       .startPeriod = 3601,
                                       .limit = 7.0,
                                       .numberPhases = 1,
                                   }},
        .evseId = DEFAULT_EVSE_ID,
        .duration = 3660,
        .scheduleStart = start_time,
        .chargingRateUnit = ChargingRateUnitEnum::W,
    };

    CompositeSchedule actual =
        handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);

    ASSERT_EQ(actual, expected);
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_ValidateProfileTransactionActiveOnEVSE) {
    ChargingProfile relative_profile =
        SmartChargingTestUtils::get_charging_profile_from_file("relative/TxProfile_relative.json");

    create_evse_with_id(DEFAULT_EVSE_ID);
    open_evse_transaction(DEFAULT_EVSE_ID, relative_profile.transactionId.value());
    ASSERT_TRUE(handler.profile_transaction_active_on_evse(relative_profile, DEFAULT_EVSE_ID));

    create_evse_with_id(DEFAULT_EVSE_ID + 1);
    open_evse_transaction(DEFAULT_EVSE_ID + 1, "another-transaction-id");
    ASSERT_FALSE(handler.profile_transaction_active_on_evse(relative_profile, DEFAULT_EVSE_ID + 1));
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_ValidateProfileTransaction_NoEVSE) {
    ChargingProfile relative_profile =
        SmartChargingTestUtils::get_charging_profile_from_file("relative/TxProfile_relative.json");

    ASSERT_FALSE(handler.profile_transaction_active_on_evse(relative_profile, DEFAULT_EVSE_ID));
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_ValidateProfileTransaction_NoActiveTransaction) {
    create_evse_with_id(DEFAULT_EVSE_ID);

    ChargingProfile relative_profile =
        SmartChargingTestUtils::get_charging_profile_from_file("relative/TxProfile_relative.json");

    ASSERT_FALSE(handler.profile_transaction_active_on_evse(relative_profile, DEFAULT_EVSE_ID));
}

// TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_RelativeProfilePowerPathClosed) {
//     // device_model->set_value(ControllerComponentVariables::TxStartPoint.component,
//     //                         ControllerComponentVariables::TxStartPoint.variable.value(), AttributeEnum::Actual,
//     //                         std::to_string(1), true);

//     std::vector<ChargingProfile> profiles =
//         SmartChargingTestUtils::get_charging_profiles_from_file("relative/TxProfile_relative.json");
//     ChargingProfile relative_profile = profiles.front();
//     auto transaction_id = relative_profile.transactionId.value();
//     create_evse_with_id(DEFAULT_EVSE_ID);
//     open_evse_transaction(DEFAULT_EVSE_ID, transaction_id);
//     const DateTime june_5th_5pm = ocpp::DateTime("2024-06-05T17:00:00");

//     auto tx_start_point = device_model->get_value<std::string>(ControllerComponentVariables::TxStartPoint);

//     auto aligned_profiles = handler.align_profiles_for_composite_schedule(profiles, june_5th_5pm, DEFAULT_EVSE_ID);

//     ASSERT_EQ("PowerPathClosed", tx_start_point);
//     // TODO: Expected result
//     // ASSERT_EQ(DEFAULT_TRANSACTION_DATE_TIME,
//     // aligned_profiles.at(0).chargingSchedule.at(0).startSchedule.value());
//     ASSERT_EQ(june_5th_5pm, aligned_profiles.at(0).chargingSchedule.at(0).startSchedule.value());

//     log_me(aligned_profiles);
// }

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_DemoCaseOne_17th) {
    // GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);
    std::vector<ChargingProfile> profiles =
        SmartChargingTestUtils::get_charging_profiles_from_directory(BASE_JSON_PATH + "/case_one/");
    ChargingProfile relative_profile = profiles.front();
    auto transaction_id = relative_profile.transactionId.value();
    open_evse_transaction(DEFAULT_EVSE_ID, transaction_id);
    const DateTime start_time = ocpp::DateTime("2024-01-17T18:00:00");
    const DateTime end_time = ocpp::DateTime("2024-01-18T06:00:00");

    CompositeSchedule expected = {
        .chargingSchedulePeriod = {{
                                       .startPeriod = 0,
                                       .limit = 2000.0,
                                       .numberPhases = 1,
                                   },
                                   {
                                       .startPeriod = 1080,
                                       .limit = 11000.0,
                                       .numberPhases = 1,
                                   },
                                   {
                                       .startPeriod = 25200,
                                       .limit = 6000.0,
                                       .numberPhases = 1,
                                   }},
        .evseId = DEFAULT_EVSE_ID,
        .duration = 43200,
        .scheduleStart = start_time,
        .chargingRateUnit = ChargingRateUnitEnum::W,
    };

    CompositeSchedule actual =
        handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);

    ASSERT_EQ(actual, expected);
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_DemoCaseOne_19th) {
    // GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);
    std::vector<ChargingProfile> profiles =
        SmartChargingTestUtils::get_charging_profiles_from_directory(BASE_JSON_PATH + "/case_one/");
    ChargingProfile first_profile = profiles.front();
    auto transaction_id = first_profile.transactionId.value();
    open_evse_transaction(DEFAULT_EVSE_ID, transaction_id);
    const DateTime start_time = ocpp::DateTime("2024-01-19T18:00:00");
    const DateTime end_time = ocpp::DateTime("2024-01-20T06:00:00");

    CompositeSchedule expected = {
        .chargingSchedulePeriod = {{
                                       .startPeriod = 0,
                                       .limit = 2000.0,
                                       .numberPhases = 1,
                                   },
                                   {
                                       .startPeriod = 1080,
                                       .limit = 11000.0,
                                       .numberPhases = 1,
                                   },
                                   {
                                       .startPeriod = 25200,
                                       .limit = 6000.0,
                                       .numberPhases = 1,
                                   }},
        .evseId = DEFAULT_EVSE_ID,
        .duration = 43200,
        .scheduleStart = start_time,
        .chargingRateUnit = ChargingRateUnitEnum::W,
    };

    CompositeSchedule actual =
        handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);

    ASSERT_EQ(ProfileValidationResultEnum::Valid, handler.validate_profile(profiles.at(0), DEFAULT_EVSE_ID));
    ASSERT_EQ(ProfileValidationResultEnum::Valid, handler.validate_profile(profiles.at(1), DEFAULT_EVSE_ID));
    ASSERT_EQ(actual, expected);
}

TEST_F(ChargepointTestFixtureV201, K08_EnhancedCompositeSchedule_ProfileA) {
    const auto now = date::utc_clock::now();
    const ocpp::DateTime start_time(now - std::chrono::seconds(600));
    const ocpp::DateTime end_time(now + std::chrono::hours(2));

    std::vector<ChargingProfile> profiles =
        SmartChargingTestUtils::get_charging_profiles_from_file("singles/ProfileA.json");
    profiles.at(0).validFrom = start_time;
    profiles.at(0).validTo = end_time;
    profiles.at(0).chargingSchedule.at(0).startSchedule = start_time;

    create_evse_with_id(DEFAULT_EVSE_ID);

    ChargingProfile profileA = profiles.front();
    auto transaction_id = profileA.transactionId.value();
    open_evse_transaction(DEFAULT_EVSE_ID, transaction_id);

    EVLOG_info << "profileA> " << utils::to_string(profileA);

    CompositeSchedule actual =
        handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);

    // ASSERT_EQ(ProfileValidationResultEnum::Valid, handler.validate_profile(profiles.at(0), DEFAULT_EVSE_ID));
    // ASSERT_EQ(ProfileValidationResultEnum::Valid, handler.validate_profile(profiles.at(1), DEFAULT_EVSE_ID));
    // ASSERT_EQ(43200, composite_schedule.duration);
    // ASSERT_EQ(start_time, composite_schedule.scheduleStart);
    // ASSERT_EQ(composite_schedule.chargingSchedulePeriod.size(), 3);
}

} // namespace ocpp::v201
