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

#include <sstream>
#include <vector>

namespace ocpp::v201 {

static const int STATION_WIDE_ID = 0;
static const int DEFAULT_EVSE_ID = 1;
static const int DEFAULT_PROFILE_ID = 1;
static const int DEFAULT_STACK_LEVEL = 1;

static const std::string BASE_JSON_PATH = "/tmp/EVerest/libocpp/v201/json/";

class ChargepointTestFixtureV201 : public testing::Test {
protected:
    void SetUp() override {
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
            id,
            charging_rate_unit,
            charging_schedule_period,
            custom_data,
            start_schedule,
            duration,
            min_charging_rate,
            sales_tariff,
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
            id,
            charging_rate_unit,
            charging_schedule_period,
            custom_data,
            start_schedule,
            duration,
            min_charging_rate,
            sales_tariff,
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

    DeviceModel create_device_model() {
        std::unique_ptr<DeviceModelStorageMock> storage_mock =
            std::make_unique<testing::NiceMock<DeviceModelStorageMock>>();
        ON_CALL(*storage_mock, get_device_model).WillByDefault(testing::Return(DeviceModelMap()));
        return DeviceModel(std::move(storage_mock));
    }

    void create_evse_with_id(int id) {
        testing::MockFunction<void(const MeterValue& meter_value, const Transaction& transaction, const int32_t seq_no,
                                   const std::optional<int32_t> reservation_id)>
            transaction_meter_value_req_mock;
        testing::MockFunction<void()> pause_charging_callback_mock;
        auto e1 = std::make_unique<Evse>(
            id, 1, device_model, database_handler, std::make_shared<ComponentStateManagerMock>(),
            transaction_meter_value_req_mock.AsStdFunction(), pause_charging_callback_mock.AsStdFunction());
        evses[id] = std::move(e1);
    }

    SmartChargingHandler create_smart_charging_handler() {
        return SmartChargingHandler(evses);
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

    std::vector<ChargingProfile> getChargingProfilesFromDirectory(const std::string& path) {
        std::vector<ChargingProfile> profiles;
        for (const auto& entry : fs::directory_iterator(path)) {
            if (!entry.is_directory()) {
                fs::path path = entry.path();
                if (path.extension() == ".json") {
                    ChargingProfile profile = getChargingProfileFromPath(path);
                    std::cout << path << std::endl;
                    profiles.push_back(profile);
                }
            }
        }
        return profiles;
    }

    ChargingProfile getChargingProfileFromPath(const std::string& path) {
        std::ifstream f(path.c_str());
        json data = json::parse(f);

        ChargingProfile cp;
        from_json(data, cp);
        return cp;
    }

    ChargingProfile getChargingProfileFromFile(const std::string& filename) {
        const std::string full_path = BASE_JSON_PATH + filename;

        return getChargingProfileFromPath(full_path);
    }

    /// \brief Returns a vector of ChargingProfiles to be used as a baseline for testing core functionality
    /// of generating an EnhancedChargingSchedule.
    std::vector<ChargingProfile> getBaselineProfileVector() {
        return getChargingProfilesFromDirectory(BASE_JSON_PATH + "baseline/");
    }

    void log_duration(int32_t duration) {
        EVLOG_info << utils::get_log_duration_string(duration);
    }

    void log_me(ChargingProfile& cp) {
        EVLOG_info << "  ChargingProfile> " << utils::to_string(cp);
    }

    void log_me(std::vector<ChargingProfile> profiles) {
        EVLOG_info << "[";
        for (auto& profile : profiles) {
            log_me(profile);
        }
        EVLOG_info << "]";
    }

    void log_me(CompositeSchedule& ecs) {
        EVLOG_info << "CompositeSchedule> " << utils::to_string(ecs);
    }

    // Default values used within the tests
    std::map<int32_t, std::unique_ptr<EvseInterface>> evses;
    std::shared_ptr<DatabaseHandler> database_handler;

    bool ignore_no_transaction = true;
    DeviceModel device_model = create_device_model();
    SmartChargingHandler handler = create_smart_charging_handler();
    boost::uuids::random_generator uuid_generator = boost::uuids::random_generator();
};

/**
 * Validates the SmartChargingHandler::determined_duraction and SmartChargingHandler::within_time_window
 * utility functions.
 */
TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_DetermineDurationAndWithinTimeWindow) {
    GTEST_SKIP();
    // Test 1: Start time before end time
    {
        DateTime start_time = ocpp::DateTime("2024-01-17T17:59:59");
        DateTime end_time = ocpp::DateTime("2024-01-17T18:00:00");

        int32_t duration = SmartChargingHandler::determine_duration(end_time, start_time);

        ASSERT_EQ(duration, 1);
        ASSERT_TRUE(SmartChargingHandler::within_time_window(start_time, end_time));
    }

    // Test 2: Start time equals end time
    {
        DateTime start_time = ocpp::DateTime("2024-01-17T17:59:59");
        DateTime end_time = ocpp::DateTime("2024-01-17T17:59:59");

        int32_t duration = SmartChargingHandler::determine_duration(start_time, end_time);
        bool within_time_window = SmartChargingHandler::within_time_window(start_time, end_time);

        ASSERT_EQ(duration, 0);
        ASSERT_FALSE(SmartChargingHandler::within_time_window(start_time, end_time));
    }

    // Test 3: Start time after end time
    {
        DateTime start_time = ocpp::DateTime("2024-01-17T18:00:00");
        DateTime end_time = ocpp::DateTime("2024-01-17T17:59:59");

        int32_t duration = SmartChargingHandler::determine_duration(start_time, end_time);
        bool within_time_window = SmartChargingHandler::within_time_window(start_time, end_time);

        ASSERT_EQ(duration, -1);
        ASSERT_FALSE(SmartChargingHandler::within_time_window(start_time, end_time));
    }
}

/**
 * Validates the SmartChargingHandler::determined_duraction and SmartChargingHandler::within_time_window
 * utility functions.
 */
TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_GetProfileStartTime_KindAbsolute) {
    GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);
    DateTime time = ocpp::DateTime("2024-01-17T17:59:59");
    ChargingProfile profile = getChargingProfileFromFile("TxProfile_01.json");
    DateTime expected = ocpp::DateTime("2024-01-17T18:00:00");

    std::optional<ocpp::DateTime> actual = handler.get_profile_start_time(profile, time, DEFAULT_EVSE_ID);

    ASSERT_EQ(expected, actual.value());
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_GetProfileStartTime_KindRecurring) {
    GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);
    DateTime time = ocpp::DateTime("2024-01-17T18:00:00");
    ChargingProfile profile = getChargingProfileFromFile("TxProfile_100.json");
    DateTime expected = ocpp::DateTime("2024-01-17T18:10:00");

    std::optional<ocpp::DateTime> actual = handler.get_profile_start_time(profile, time, DEFAULT_EVSE_ID);

    ASSERT_EQ(expected, actual.value());
}

// TODO: functionality currently not supported.
TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_GetProfileStartTime_KindRelative) {
    GTEST_SKIP();
    // create_evse_with_id(DEFAULT_EVSE_ID);
    // DateTime time = ocpp::DateTime("2024-01-17T18:00:00");
    // ChargingProfile profile = getChargingProfileFromFile("TxProfile_100.json");
    // DateTime expected = ocpp::DateTime("2024-01-17T18:10:00");

    // std::optional<ocpp::DateTime> actual = handler.get_profile_start_time(profile, time, DEFAULT_EVSE_ID);

    // ASSERT_EQ(expected, actual.value());
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_GetPeriodEndTime) {
    GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);

    // Test 1: Profile TxProfile_01.json, Absolute, Single Charging Period
    const auto profile_01 = getChargingProfileFromFile("TxProfile_01.json");

    const DateTime period_start_time_01 = ocpp::DateTime("2024-01-17T18:00:00");
    const DateTime expected_period_end_time_01 = ocpp::DateTime("2024-01-17T18:18:00");
    const ChargingSchedule schedule_01 = profile_01.chargingSchedule.front();

    EVLOG_debug << "DURATION = " << utils::get_log_duration_string(schedule_01.duration.value());
    DateTime actual_period_end_time_01 = handler.get_period_end_time(0, period_start_time_01, schedule_01);

    ASSERT_EQ(expected_period_end_time_01, actual_period_end_time_01);

    // Test 2: Profile TxProfile_100.json Period #1
    const auto profile_100 = getChargingProfileFromFile("TxProfile_100.json");

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
    GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);

    const DateTime time_17_17_59_59 = ocpp::DateTime("2024-01-17T17:59:59");
    const DateTime time_17_18_18_00 = ocpp::DateTime("2024-01-17T18:18:00");
    const DateTime time_18_01_00_00 = ocpp::DateTime("2024-01-18T01:00:00");
    const DateTime time_18_02_00_00 = ocpp::DateTime("2024-01-18T02:00:00");
    const DateTime time_18_13_00_00 = ocpp::DateTime("2024-01-18T13:00:00");
    const DateTime time_18_17_00_00 = ocpp::DateTime("2024-01-18T17:00:00");
    std::vector<ChargingProfile> profiles = getBaselineProfileVector();

    ASSERT_EQ(2, profiles.size());
    ASSERT_EQ(time_17_18_18_00, handler.get_next_temp_time(time_17_17_59_59, profiles, DEFAULT_EVSE_ID));
    ASSERT_EQ(time_18_01_00_00, handler.get_next_temp_time(time_17_18_18_00, profiles, DEFAULT_EVSE_ID));
    ASSERT_EQ(time_18_13_00_00, handler.get_next_temp_time(time_18_02_00_00, profiles, DEFAULT_EVSE_ID));
    ASSERT_EQ(time_18_17_00_00, handler.get_next_temp_time(time_18_13_00_00, profiles, DEFAULT_EVSE_ID));
}

/**
 * Calculate Composite Schedule
 */
TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_ValidateBaselineProfileVector) {
    // GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);

    const DateTime time_17_17_59_59 = ocpp::DateTime("2024-01-17T17:59:59");
    const DateTime start_time = ocpp::DateTime("2024-01-17T18:01:00");
    const DateTime end_time = ocpp::DateTime("2024-01-18T06:00:00");
    const int32_t expected_duration = 43140;

    std::vector<ChargingProfile> profiles = getBaselineProfileVector();

    CompositeSchedule composite_schedule =
        handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);

    log_me(composite_schedule);

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
    ASSERT_EQ(period_02.numberPhases, 3);
    ASSERT_EQ(period_02.startPeriod, 1020);
    auto& period_03 = composite_schedule.chargingSchedulePeriod.at(2);
    ASSERT_EQ(period_03.limit, 6000.0);
    ASSERT_EQ(period_03.numberPhases, 3);
    ASSERT_EQ(period_03.startPeriod, 25140);

    // Validate that reversing the profile vector returns the same result
    std::vector<ChargingProfile> reverse_profiles = getBaselineProfileVector();
    std::reverse(reverse_profiles.begin(), reverse_profiles.end());
    CompositeSchedule reverse_composite_schedule = handler.calculate_composite_schedule(
        reverse_profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::W);
    ASSERT_EQ(utils::to_string(composite_schedule), utils::to_string(reverse_composite_schedule));

    log_me(composite_schedule);
}

TEST_F(ChargepointTestFixtureV201, getChargingProfilesFromDirectory) {
    std::vector<ChargingProfile> vic = getChargingProfilesFromDirectory(BASE_JSON_PATH + "baseline/");
}

} // namespace ocpp::v201
