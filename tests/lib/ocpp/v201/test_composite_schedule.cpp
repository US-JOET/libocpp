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

// static const std::string BASE_JSON_PATH = TEST_PROFILES_LOCATION_V201;

class TestSmartChargingHandler : public SmartChargingHandler {
public:
    using SmartChargingHandler::convert_relative_to_absolute;
    using SmartChargingHandler::determine_duration;
    using SmartChargingHandler::get_next_temp_time;
    using SmartChargingHandler::get_period_end_time;
    using SmartChargingHandler::get_profile_start_time;
    using SmartChargingHandler::within_time_window;

    TestSmartChargingHandler(std::map<int32_t, std::unique_ptr<EvseInterface>>& evses,
                             std::shared_ptr<DeviceModel>& device_model) :
        SmartChargingHandler(evses, device_model) {
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
        auto date_time = ocpp::DateTime("2024-01-17T17:00:00");
        evses[evse_id]->open_transaction(
            transaction_id, connector_id, date_time, meter_start, id_token, {}, {},
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

/**
 * Validates the SmartChargingHandler::determined_duraction and SmartChargingHandler::within_time_window
 * utility functions.
 */
TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_DetermineDurationAndWithinTimeWindow) {
    // Test 1: Start time before end time
    {
        DateTime start_time = ocpp::DateTime("2024-01-17T17:59:59");
        DateTime end_time = ocpp::DateTime("2024-01-17T18:00:00");

        int32_t duration = handler.determine_duration(start_time, end_time);

        ASSERT_EQ(duration, 1);
        ASSERT_TRUE(handler.within_time_window(start_time, end_time));
    }

    // Test 2 : Start time equals end time
    {
        DateTime start_time = ocpp::DateTime("2024-01-17T17:59:59");
        DateTime end_time = ocpp::DateTime("2024-01-17T17:59:59");

        int32_t duration = handler.determine_duration(start_time, end_time);
        bool within_time_window = handler.within_time_window(start_time, end_time);

        ASSERT_EQ(duration, 0);
        ASSERT_FALSE(handler.within_time_window(start_time, end_time));
    }

    // Test 3: Start time after end time
    {
        DateTime start_time = ocpp::DateTime("2024-01-17T18:00:00");
        DateTime end_time = ocpp::DateTime("2024-01-17T17:59:59");

        int32_t duration = handler.determine_duration(start_time, end_time);
        bool within_time_window = handler.within_time_window(start_time, end_time);

        ASSERT_EQ(duration, -1);
        ASSERT_FALSE(handler.within_time_window(start_time, end_time));
    }
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_GetProfileStartTime_KindAbsolute) {
    create_evse_with_id(DEFAULT_EVSE_ID);
    DateTime time = ocpp::DateTime("2024-01-17T17:59:59");
    ChargingProfile profile = SmartChargingTestUtils::get_charging_profile_from_file("baseline/TxProfile_1.json");
    DateTime expected = ocpp::DateTime("2024-01-17T18:00:00");

    std::optional<ocpp::DateTime> actual = handler.get_profile_start_time(profile, time, DEFAULT_EVSE_ID);

    ASSERT_EQ(expected, actual.value());
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_GetProfileStartTime_KindRecurring) {
    // GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);
    // NOTE: Fisrt time period for this schedule is 28800 seconds, or 8 hours long
    ChargingProfile profile = SmartChargingTestUtils::get_charging_profile_from_file("baseline/TxProfile_100.json");
    DateTime expected = ocpp::DateTime("2024-01-17T18:10:00");

    ASSERT_EQ(ocpp::DateTime("2024-01-17T17:00:00"),
              handler.get_profile_start_time(profile, ocpp::DateTime("2024-01-17T17:00:00"), DEFAULT_EVSE_ID).value());

    // NOTE: This fails returning the previous day  2024-01-16T17:00:00.000Z
    // ASSERT_EQ(ocpp::DateTime("2024-01-17T17:00:00"),
    //           handler.get_profile_start_time(profile, ocpp::DateTime("2024-01-17T16:00:00"),
    //           DEFAULT_EVSE_ID).value());

    // NOTE: This requires more exploration. Is this as expected
    ASSERT_EQ(ocpp::DateTime("2024-01-17T17:00:00"),
              handler.get_profile_start_time(profile, ocpp::DateTime("2024-01-17T17:01:00"), DEFAULT_EVSE_ID).value());
}

// TODO: functionality currently not supported.
TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_GetProfileStartTime_KindRelative) {
    create_evse_with_id(DEFAULT_EVSE_ID);
    ChargingProfile profile = SmartChargingTestUtils::get_charging_profile_from_file("relative/TxProfile_relative.json");
    open_evse_transaction(DEFAULT_EVSE_ID, profile.transactionId.value());

    DateTime time = ocpp::DateTime("2024-01-17T18:00:00");
    std::optional<ocpp::DateTime> actual = handler.get_profile_start_time(profile, time, DEFAULT_EVSE_ID);

    DateTime expected = ocpp::DateTime("2024-01-17T18:10:00");
    ASSERT_EQ(expected, actual.value());
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_GetPeriodEndTime) {
    create_evse_with_id(DEFAULT_EVSE_ID);

    // Test 1: Profile TxProfile_01.json, Absolute, Single Charging Period
    const auto profile_01 = SmartChargingTestUtils::get_charging_profile_from_file("baseline/TxProfile_1.json");

    const DateTime period_start_time_01 = ocpp::DateTime("2024-01-17T18:00:00");
    const DateTime expected_period_end_time_01 = ocpp::DateTime("2024-01-17T18:18:00");
    const ChargingSchedule schedule_01 = profile_01.chargingSchedule.front();

    EVLOG_debug << "DURATION = " << utils::get_log_duration_string(schedule_01.duration.value());
    DateTime actual_period_end_time_01 = handler.get_period_end_time(0, period_start_time_01, schedule_01);

    ASSERT_EQ(expected_period_end_time_01, actual_period_end_time_01);

    // Test 2: Profile TxProfile_100.json Period #1
    const auto profile_100 = SmartChargingTestUtils::get_charging_profile_from_file("baseline/TxProfile_100.json");

    const DateTime period_start_time_02 = ocpp::DateTime("2024-01-17T17:00:00");
    const DateTime expected_period_end_time_02 = ocpp::DateTime("2024-01-18T01:00:00");
    const ChargingSchedule schedule_02 = profile_100.chargingSchedule.front();

    EVLOG_debug << "DURATION = " << utils::get_log_duration_string(28800);
    DateTime actual_period_end_time_02 = handler.get_period_end_time(0, period_start_time_02, schedule_02);

    ASSERT_EQ(expected_period_end_time_02, actual_period_end_time_02);

    // Test 2a: Profile TxProfile_100.json Period #1 - Start time 1 hour later than startSchedule. Not enforcing
    // startSchedule as fixed time point for chargingSchedulePeriod calculations. TODO: overly complex, begging for
    // defects logic matches 1.6. Refactoring opportunity? Probability of defect: LOW
    // const DateTime period_start_time_02a = ocpp::DateTime("2024-01-17T18:00:00");
    // DateTime actual_period_end_time_02a = handler.get_period_end_time(0, period_start_time_02a, schedule_02);
    // ASSERT_EQ(expected_period_end_time_02, actual_period_end_time_02a);

    // Test 3: Profile TxProfile_100.json Period #2
    const DateTime period_start_time_03 = ocpp::DateTime("2024-01-18T13:00:00");
    const DateTime expected_period_end_time_03 = ocpp::DateTime("2024-01-19T01:00:00");

    EVLOG_debug << "DURATION = " << utils::get_log_duration_string(72000);
    DateTime actual_period_end_time_03 = handler.get_period_end_time(1, period_start_time_03, schedule_02);

    ASSERT_EQ(expected_period_end_time_03, actual_period_end_time_03);
}

// Based upon K01.FR11 K01.FR38
TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_GetPeriodEndTime_PAIN) {
    GTEST_SKIP();
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_GetNextTempTime) {
    // GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);

    const DateTime time_17_17_59_59 = ocpp::DateTime("2024-01-17T17:59:59");
    const DateTime time_17_18_18_00 = ocpp::DateTime("2024-01-17T18:18:00");
    const DateTime time_18_01_00_00 = ocpp::DateTime("2024-01-18T01:00:00");
    const DateTime time_18_02_00_00 = ocpp::DateTime("2024-01-18T02:00:00");
    const DateTime time_18_13_00_00 = ocpp::DateTime("2024-01-18T13:00:00");
    const DateTime time_18_17_00_00 = ocpp::DateTime("2024-01-18T17:00:00");
    std::vector<ChargingProfile> profiles = SmartChargingTestUtils::get_baseline_profile_vector();

    ASSERT_EQ(2, profiles.size());
    ASSERT_EQ(time_17_18_18_00, handler.get_next_temp_time(time_17_17_59_59, profiles, DEFAULT_EVSE_ID));
    ASSERT_EQ(time_18_01_00_00, handler.get_next_temp_time(time_17_18_18_00, profiles, DEFAULT_EVSE_ID));
    ASSERT_EQ(time_18_13_00_00, handler.get_next_temp_time(time_18_02_00_00, profiles, DEFAULT_EVSE_ID));
    ASSERT_EQ(time_18_17_00_00, handler.get_next_temp_time(time_18_13_00_00, profiles, DEFAULT_EVSE_ID));
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_FoundationTest_Grid) {
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
    create_evse_with_id(DEFAULT_EVSE_ID);
    std::vector<ChargingProfile> profiles =
        SmartChargingTestUtils::get_charging_profiles_from_directory(BASE_JSON_PATH + "/layered/");

    // Time Window: START = Stack #1 start time || END = Stack #1 end time
    {
        const DateTime start_time = ocpp::DateTime("2024-01-18T18:04:00");
        const DateTime end_time = ocpp::DateTime("2024-01-18T18:22:00");

        CompositeSchedule composite_schedule = handler.calculate_composite_schedule(
            profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);

        EVLOG_info << "CompositeSchedule> " << utils::to_string(composite_schedule);
        EVLOG_info << "CompositeSchedule duration> " << utils::get_log_duration_string(composite_schedule.duration);
        ASSERT_EQ(start_time, composite_schedule.scheduleStart);
        ASSERT_EQ(composite_schedule.chargingSchedulePeriod.size(), 1);
        ASSERT_EQ(composite_schedule.duration, 1080);
    }

    // Time Window: START = Stack #1 start time || END = After Stack #1 end time Before next Start #0 start time
    {
        const DateTime start_time = ocpp::DateTime("2024-01-17T18:04:00");
        const DateTime end_time = ocpp::DateTime("2024-01-17T18:33:00");

        CompositeSchedule composite_schedule = handler.calculate_composite_schedule(
            profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);

        EVLOG_info << "CompositeSchedule> " << utils::to_string(composite_schedule);
        EVLOG_info << "CompositeSchedule duration> " << utils::get_log_duration_string(composite_schedule.duration);
        ASSERT_EQ(start_time, composite_schedule.scheduleStart);
        ASSERT_EQ(composite_schedule.chargingSchedulePeriod.size(), 2);
        ASSERT_EQ(composite_schedule.duration, 1740);
    }

    // Time Window: START = Stack #1 start time || END = After next Start #0 start time
    {
        const DateTime start_time = ocpp::DateTime("2024-01-17T18:04:00");
        const DateTime end_time = ocpp::DateTime("2024-01-17T19:04:00");

        CompositeSchedule composite_schedule = handler.calculate_composite_schedule(
            profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);

        EVLOG_info << "CompositeSchedule> " << utils::to_string(composite_schedule);
        EVLOG_info << "CompositeSchedule duration> " << utils::get_log_duration_string(composite_schedule.duration);
        ASSERT_EQ(start_time, composite_schedule.scheduleStart);
        ASSERT_EQ(composite_schedule.chargingSchedulePeriod.size(), 3);
        ASSERT_EQ(composite_schedule.duration, 3600);
    }
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_LayeredTest_FutureStartTime) {
    create_evse_with_id(DEFAULT_EVSE_ID);
    std::vector<ChargingProfile> profiles =
        SmartChargingTestUtils::get_charging_profiles_from_directory(BASE_JSON_PATH + "/layered/");

    // TODO: Why doesn't the layered profile show up if the start_time date is a month ahead?
    const DateTime start_time = ocpp::DateTime("2024-02-17T18:04:00");
    const DateTime end_time = ocpp::DateTime("2024-02-17T18:05:00");

    CompositeSchedule composite_schedule =
        handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);

    EVLOG_info << "CompositeSchedule> " << utils::to_string(composite_schedule);
    EVLOG_info << "CompositeSchedule duration> " << utils::get_log_duration_string(composite_schedule.duration);
    ASSERT_EQ(start_time, composite_schedule.scheduleStart);
    ASSERT_EQ(composite_schedule.chargingSchedulePeriod.size(), 1);
    ASSERT_EQ(composite_schedule.duration, 60);
}

// TODO Question: Is this expected behaviour when the time window starts before the first period of the only profile.
TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_LayeredTest_PreviousStartTime) {
    create_evse_with_id(DEFAULT_EVSE_ID);
    std::vector<ChargingProfile> profiles =
        SmartChargingTestUtils::get_charging_profiles_from_directory(BASE_JSON_PATH + "/null_start/");

    // TODO: Why doesn't the layered profile show up if the start_time is before the profile's start time?
    const DateTime start_time = ocpp::DateTime("2024-01-17T18:00:00");
    const DateTime end_time = ocpp::DateTime("2024-01-17T18:05:00");

    CompositeSchedule composite_schedule =
        handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);

    EVLOG_info << "CompositeSchedule> " << utils::to_string(composite_schedule);
    EVLOG_info << "CompositeSchedule duration> " << utils::get_log_duration_string(composite_schedule.duration);
    ASSERT_EQ(start_time, composite_schedule.scheduleStart);
    ASSERT_EQ(composite_schedule.chargingSchedulePeriod.size(), 0);
    ASSERT_EQ(composite_schedule.duration, 300);
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_LayeredRecurringTest_PreviousStartTime) {
    GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);
    std::vector<ChargingProfile> profiles =
        SmartChargingTestUtils::get_charging_profiles_from_directory(BASE_JSON_PATH + "/layered_recurring/");

    // TODO: Why doesn't the layered profile show up if the start_time is before the profile's start time?
    const DateTime start_time = ocpp::DateTime("2024-02-19T18:00:00");
    const DateTime end_time = ocpp::DateTime("2024-02-19T19:04:00");

    CompositeSchedule composite_schedule =
        handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);

    EVLOG_info << "CompositeSchedule> " << utils::to_string(composite_schedule);
    EVLOG_info << "CompositeSchedule duration> " << utils::get_log_duration_string(composite_schedule.duration);
    ASSERT_EQ(start_time, composite_schedule.scheduleStart);
    ASSERT_EQ(composite_schedule.chargingSchedulePeriod.size(), 4);
    ASSERT_EQ(composite_schedule.chargingSchedulePeriod.at(0).limit, 19.0);
    ASSERT_EQ(composite_schedule.chargingSchedulePeriod.at(1).limit, 2000.0);
    ASSERT_EQ(composite_schedule.chargingSchedulePeriod.at(2).limit, 19.0);
    ASSERT_EQ(composite_schedule.chargingSchedulePeriod.at(3).limit, 20.0);
    ASSERT_EQ(composite_schedule.duration, 3840);
}

/**
 * Calculate Composite Schedule
 */
TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_ValidateBaselineProfileVector) {
    create_evse_with_id(DEFAULT_EVSE_ID);

    const DateTime time_17_17_59_59 = ocpp::DateTime("2024-01-17T17:59:59");
    const DateTime start_time = ocpp::DateTime("2024-01-17T18:01:00");
    const DateTime end_time = ocpp::DateTime("2024-01-18T06:00:00");
    const int32_t expected_duration = 43140;

    std::vector<ChargingProfile> profiles = SmartChargingTestUtils::get_baseline_profile_vector();

    CompositeSchedule composite_schedule =
        handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);

    EVLOG_info << "CompositeSchedule> " << utils::to_string(composite_schedule);

    // Validate base fields
    ASSERT_EQ(ChargingRateUnitEnum::W, composite_schedule.chargingRateUnit);
    ASSERT_EQ(DEFAULT_EVSE_ID, composite_schedule.evseId);
    ASSERT_EQ(expected_duration, composite_schedule.duration);
    ASSERT_EQ(start_time, composite_schedule.scheduleStart);
    ASSERT_EQ(composite_schedule.chargingSchedulePeriod.size(), 3);

    // Validate each period
    auto& period_01 = composite_schedule.chargingSchedulePeriod.at(0);
    ASSERT_EQ(period_01.limit, 2000);
    ASSERT_EQ(period_01.numberPhases, 1);
    ASSERT_EQ(period_01.startPeriod, 0);
    auto& period_02 = composite_schedule.chargingSchedulePeriod.at(1);
    ASSERT_EQ(period_02.limit, 11000);
    SmartChargingTestUtils utils;
    std::vector<ChargingProfile> reverse_profiles = utils.get_baseline_profile_vector();
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_RelativeProfile) {
    create_evse_with_id(DEFAULT_EVSE_ID);
    ChargingProfile profile =
        SmartChargingTestUtils::get_charging_profile_from_file("relative/TxProfile_relative.json");
    open_evse_transaction(DEFAULT_EVSE_ID, profile.transactionId.value());
    ProfileValidationResultEnum validate = handler.validate_profile(profile, 1);

    ASSERT_EQ(ProfileValidationResultEnum::Valid, validate);

    // The Plan™
}

TEST_F(ChargepointTestFixtureV201,
       K08_CalculateCompositeSchedule_ConvertRelativeProfileToAbsoluteWithInvalidProfileType_ReturnsSameProfile) {
    ChargingProfile absolute_profile =
        SmartChargingTestUtils::get_charging_profile_from_file("baseline/TxProfile_1.json");

    ChargingProfile resulting_profile = handler.convert_relative_to_absolute(absolute_profile, MAX_DATE_TIME);

    ASSERT_EQ(utils::to_string(absolute_profile), utils::to_string(resulting_profile));
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_ConvertRelativeProfileToAbsolute) {
    ChargingProfile relative_profile =
        SmartChargingTestUtils::get_charging_profile_from_file("singles/Relative_MultipleChargingSchedules.json");
    const DateTime time_20_17_59_59 = ocpp::DateTime("2024-01-20T17:59:59");

    ChargingProfile resulting_profile = handler.convert_relative_to_absolute(relative_profile, time_20_17_59_59);

    ASSERT_EQ(resulting_profile.chargingProfileKind, ChargingProfileKindEnum::Absolute);
    ASSERT_EQ(resulting_profile.chargingSchedule.at(0).startSchedule.value(), time_20_17_59_59);
    ASSERT_EQ(resulting_profile.chargingSchedule.at(1).startSchedule.value(), time_20_17_59_59);
    ASSERT_EQ(resulting_profile.chargingSchedule.at(2).startSchedule.value(), time_20_17_59_59);
    ASSERT_FALSE(resulting_profile.recurrencyKind.has_value());
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_ConvertRelativeProfileToAbsoluteWithNow) {
    ChargingProfile relative_profile =
        SmartChargingTestUtils::get_charging_profile_from_file("singles/Relative_MultipleChargingSchedules.json");

    ChargingProfile resulting_profile = handler.convert_relative_to_absolute(relative_profile);

    ASSERT_EQ(resulting_profile.chargingProfileKind, ChargingProfileKindEnum::Absolute);
    ASSERT_FALSE(resulting_profile.recurrencyKind.has_value());
}

} // namespace ocpp::v201
