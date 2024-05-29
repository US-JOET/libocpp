#include "date/tz.h"
#include "everest/logging.hpp"
#include "ocpp/v201/ctrlr_component_variables.hpp"
#include "ocpp/v201/device_model_storage_sqlite.hpp"
#include "ocpp/v201/ocpp_types.hpp"
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <filesystem>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

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

    ChargingProfile getChargingProfileFromFile(const std::string& filename) {
        const std::string base_path = "/tmp/EVerest/libocpp/json/";
        const std::string full_path = base_path + filename;

        std::ifstream f(full_path.c_str());
        json data = json::parse(f);

        ChargingProfile cp;
        from_json(data, cp);
        return cp;
    }

    /// \brief Returns a vector of ChargingProfiles to be used as a baseline for testing core functionality
    /// of generating an EnhancedChargingSchedule.
    std::vector<ChargingProfile> getBaselineProfileVector() {
        auto profile_01 = getChargingProfileFromFile("TxProfile_01.json");
        auto profile_100 = getChargingProfileFromFile("TxProfile_100.json");
        return {profile_01, profile_100};
    }

    std::string get_log_duration_string(int32_t duration) {
        if (duration < 1) {
            return "0 Seconds ";
        }

        int32_t remaining = duration;

        std::string log_str = "";

        if (remaining >= 86400) {
            int32_t days = remaining / 86400;
            remaining = remaining % 86400;
            if (days > 1) {
                log_str += std::to_string(days) + " Days ";
            } else {
                log_str += std::to_string(days) + " Day ";
            }
        }
        if (remaining >= 3600) {
            int32_t hours = remaining / 3600;
            remaining = remaining % 3600;
            log_str += std::to_string(hours) + " Hours ";
        }
        if (remaining >= 60) {
            int32_t minutes = remaining / 60;
            remaining = remaining % 60;
            log_str += std::to_string(minutes) + " Minutes ";
        }
        if (remaining > 0) {
            log_str += std::to_string(remaining) + " Seconds ";
        }
        return log_str;
    }

    void log_duration(int32_t duration) {
        EVLOG_info << get_log_duration_string(duration);
    }

    void log_me(ChargingProfile& cp) {
        json cp_json;
        to_json(cp_json, cp);

        EVLOG_info << "  ChargingProfile> " << cp_json.dump(2);
        // log_duration(cp.chargingSchedule.duration.value_or(0));
    }

    void log_me(std::vector<ChargingProfile> profiles) {
        EVLOG_info << "[";
        for (auto& profile : profiles) {
            log_me(profile);
        }
        EVLOG_info << "]";
    }

    void log_me(CompositeSchedule& ecs) {
        json ecs_json;
        to_json(ecs_json, ecs);

        EVLOG_info << "CompositeSchedule> " << ecs_json.dump(4);
    }

    // void log_me(CompositeSchedule& ecs, const DateTime start_time) {
    //     log_me(ecs);
    //     EVLOG_info << "Start Time> " << start_time.to_rfc3339();

    //     int32_t i = 0;
    //     for (auto& period : ecs.chargingSchedulePeriod) {
    //         i++;
    //         int32_t numberPhases = 0;
    //         if (period.numberPhases.has_value()) {
    //             numberPhases = period.numberPhases.value();
    //         }
    //         EVLOG_info << "   period #" << i << " {limit: " << period.limit << " numberPhases:" << numberPhases
    //                    << " stackLevel:" << period.stackLevel << "} starts "
    //                    << get_log_duration_string(period.startPeriod) << "in";
    //     }
    //     if (ecs.duration.has_value()) {
    //         EVLOG_info << "   period #" << i << " ends after " << get_log_duration_string(ecs.duration.value());
    //     } else {
    //         EVLOG_info << "   period #" << i << " ends in 0 Seconds";
    //     }
    // }

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

        int32_t duration = SmartChargingHandler::determine_duration(start_time, end_time);

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
    // GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);

    // First Test: Profile TxProfile_01.json, Absolute, Single Charging Period
    const DateTime period_start_time_01 = ocpp::DateTime("2024-01-17T17:59:59");
    const DateTime expected_period_end_time_01 = ocpp::DateTime("2024-01-17T18:17:59");
    const auto profile_01 = getChargingProfileFromFile("TxProfile_01.json");
    const ChargingSchedule schedule_01 = profile_01.chargingSchedule.front();
    const std::vector<ChargingSchedulePeriod> periods_01 = profile_01.chargingSchedule.front().chargingSchedulePeriod;

    EVLOG_debug << "DURATION = " << get_log_duration_string(schedule_01.duration.value());
    DateTime actual_period_end_time_01 = handler.get_period_end_time(0, period_start_time_01, schedule_01, periods_01);

    ASSERT_EQ(expected_period_end_time_01, actual_period_end_time_01);
    // ASSERT_EQ(time_6_00_00, handler.get_period_end_time(0, time_5_59_59, profile_01.chargingSchedule.front(),
    //                                                     profile_01.chargingSchedule.front().chargingSchedulePeriod));
    // ASSERT_EQ(time_6_10_00, handler.get_next_temp_time(time_6_00_00, profiles, DEFAULT_EVSE_ID));
    // ASSERT_EQ(time_6_10_00, handler.get_next_temp_time(time_6_01_00, profiles, DEFAULT_EVSE_ID));
    // ASSERT_EQ(time_6_10_00, handler.get_next_temp_time(time_6_10_01, profiles, DEFAULT_EVSE_ID));
}

TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_GetNextTempTime) {
    GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);
    const DateTime time_5_59_59 = ocpp::DateTime("2024-01-17T17:59:59");
    const DateTime time_6_00_00 = ocpp::DateTime("2024-01-17T18:00:00");
    const DateTime time_6_01_00 = ocpp::DateTime("2024-01-17T18:01:00");
    const DateTime time_6_10_00 = ocpp::DateTime("2024-01-17T18:10:00");
    const DateTime time_6_10_01 = ocpp::DateTime("2024-01-17T18:10:01");
    std::vector<ChargingProfile> profiles = getBaselineProfileVector();

    ASSERT_EQ(2, profiles.size());
    ASSERT_EQ(time_6_00_00, handler.get_next_temp_time(time_5_59_59, profiles, DEFAULT_EVSE_ID));
    // ASSERT_EQ(time_6_10_00, handler.get_next_temp_time(time_6_00_00, profiles, DEFAULT_EVSE_ID));
    // ASSERT_EQ(time_6_10_00, handler.get_next_temp_time(time_6_01_00, profiles, DEFAULT_EVSE_ID));
    // ASSERT_EQ(time_6_10_00, handler.get_next_temp_time(time_6_10_01, profiles, DEFAULT_EVSE_ID));
}

/**
 * Calculate Composite Schedule
 */
TEST_F(ChargepointTestFixtureV201, K08_CalculateCompositeSchedule_InitializeEnhancedCompositeSchedule) {
    // GTEST_SKIP();
    create_evse_with_id(DEFAULT_EVSE_ID);
    const DateTime start_time = ocpp::DateTime("2024-01-17T17:59:59");
    const DateTime end_time = ocpp::DateTime("2024-01-18T00:00:00");

    // auto profile_01 = getChargingProfileFromFile("TxProfile_01.json");
    // auto profile_100 = getChargingProfileFromFile("TxProfile_100.json");
    std::vector<ChargingProfile> profiles = getBaselineProfileVector();
    // std::vector<ChargingProfile> profiles = {profile_100};

    CompositeSchedule composite_schedule =
        handler.calculate_composite_schedule(profiles, start_time, end_time, DEFAULT_EVSE_ID, ChargingRateUnitEnum::A);

    ASSERT_EQ(ChargingRateUnitEnum::A, composite_schedule.chargingRateUnit);
    ASSERT_EQ(DEFAULT_EVSE_ID, composite_schedule.evseId);
    ASSERT_EQ(21601, composite_schedule.duration);
    ASSERT_EQ(start_time, composite_schedule.scheduleStart);
    ASSERT_EQ(composite_schedule.chargingSchedulePeriod.size(), 0);
    log_me(composite_schedule);
}

} // namespace ocpp::v201
