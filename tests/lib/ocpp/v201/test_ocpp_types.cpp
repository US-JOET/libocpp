#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <optional>

#include "everest/logging.hpp"
#include "ocpp/common/types.hpp"
#include "ocpp/v201/ocpp_types.hpp"
#include "ocpp/v201/utils.hpp"

#include "smart_charging_test_utils.hpp"

namespace {
using namespace ocpp::v201;
using namespace ocpp;
using std::nullopt;

const DateTime time08_00 = ocpp::DateTime("2024-01-01T08:00:00Z");
const DateTime time08_10 = ocpp::DateTime("2024-01-01T08:10:00Z");
const DateTime time11_50 = ocpp::DateTime("2024-01-01T11:50:00Z");
const DateTime time12_00 = ocpp::DateTime("2024-01-01T12:00:00Z");
const DateTime time12_02 = ocpp::DateTime("2024-01-01T12:02:00Z");
const DateTime time12_05 = ocpp::DateTime("2024-01-01T12:02:00Z");
const DateTime time12_10 = ocpp::DateTime("2024-01-01T12:10:00Z");
const DateTime time12_15 = ocpp::DateTime("2024-01-01T12:15:00Z");
const DateTime time12_20 = ocpp::DateTime("2024-01-01T12:20:00Z");
const DateTime time12_32 = ocpp::DateTime("2024-01-01T12:32:00Z");
const DateTime time12_45 = ocpp::DateTime("2024-01-01T12:45:00Z");
const DateTime time12_47 = ocpp::DateTime("2024-01-01T12:47:00Z");
const DateTime time12_50 = ocpp::DateTime("2024-01-01T12:50:00Z");
const DateTime time12_55 = ocpp::DateTime("2024-01-01T12:55:00Z");
const DateTime time13_00 = ocpp::DateTime("2024-01-01T13:00:00Z");
const DateTime time13_02 = ocpp::DateTime("2024-01-01T13:02:00Z");
const DateTime time13_05 = ocpp::DateTime("2024-01-01T13:05:00Z");
const DateTime time13_15 = ocpp::DateTime("2024-01-01T13:15:00Z");
const DateTime time13_20 = ocpp::DateTime("2024-01-01T13:20:00Z");
const DateTime time14_00 = ocpp::DateTime("2024-01-01T14:00:00Z");
const DateTime time14_10 = ocpp::DateTime("2024-01-01T14:10:00Z");
const DateTime time14_15 = ocpp::DateTime("2024-01-01T14:15:00Z");
const DateTime time18_00 = ocpp::DateTime("2024-01-01T18:00:00Z");
const DateTime time20_50 = ocpp::DateTime("2024-01-01T20:50:00Z");
const DateTime time02_07_10 = ocpp::DateTime("2024-01-02T07:10:00Z");
const DateTime time02_08_00 = ocpp::DateTime("2024-01-02T08:00:00Z");
const DateTime time02_08_10 = ocpp::DateTime("2024-01-02T08:10:00Z");
const DateTime time02_08_30 = ocpp::DateTime("2024-01-02T08:30:00Z");
const DateTime time02_08_45 = ocpp::DateTime("2024-01-02T08:45:00Z");
const DateTime time02_09_00 = ocpp::DateTime("2024-01-02T09:00:00Z");
const DateTime time02_20_50 = ocpp::DateTime("2024-01-02T20:50:00Z");
const DateTime time02_23_10 = ocpp::DateTime("2024-01-02T23:10:00Z");
const DateTime time03_07_10 = ocpp::DateTime("2024-01-03T07:10:00Z");
const DateTime time03_08_00 = ocpp::DateTime("2024-01-03T08:00:00Z");
const DateTime time03_08_30 = ocpp::DateTime("2024-01-03T08:30:00Z");
const DateTime time03_08_45 = ocpp::DateTime("2024-01-03T08:45:00Z");
const DateTime time03_09_00 = ocpp::DateTime("2024-01-03T09:00:00Z");
const DateTime time03_16_00 = ocpp::DateTime("2024-01-03T16:00:00Z");
const DateTime time03_16_10 = ocpp::DateTime("2024-01-03T16:10:00Z");
const DateTime time03_16_30 = ocpp::DateTime("2024-01-03T16:30:00Z");
const DateTime time03_16_45 = ocpp::DateTime("2024-01-03T16:45:00Z");
const DateTime time03_17_00 = ocpp::DateTime("2024-01-03T17:00:00Z");
const DateTime time03_20_50 = ocpp::DateTime("2024-01-03T20:50:00Z");
const DateTime time03_23_10 = ocpp::DateTime("2024-01-03T23:10:00Z");
const DateTime time04_08_00 = ocpp::DateTime("2024-01-04T08:00:00Z");
const DateTime time04_23_10 = ocpp::DateTime("2024-01-04T23:10:00Z");
const DateTime time05_08_00 = ocpp::DateTime("2024-01-05T08:00:00Z");
const DateTime time05_11_50 = ocpp::DateTime("2024-01-05T11:50:00Z");
const DateTime time05_12_10 = ocpp::DateTime("2024-01-05T12:10:00Z");
const DateTime time06_08_00 = ocpp::DateTime("2024-01-06T08:00:00Z");
const DateTime time06_20_50 = ocpp::DateTime("2024-01-06T20:50:00Z");
const DateTime time07_20_50 = ocpp::DateTime("2024-01-07T20:50:00Z");
const DateTime time10_07_10 = ocpp::DateTime("2024-01-10T07:10:00Z");
const DateTime time10_16_00 = ocpp::DateTime("2024-01-10T16:00:00Z");
const DateTime time10_16_30 = ocpp::DateTime("2024-01-10T16:30:00Z");
const DateTime time10_16_45 = ocpp::DateTime("2024-01-10T16:45:00Z");
const DateTime time10_17_00 = ocpp::DateTime("2024-01-10T17:00:00Z");
const DateTime time10_20_10 = ocpp::DateTime("2024-01-10T20:10:00Z");
const DateTime time10_20_50 = ocpp::DateTime("2024-01-10T20:50:00Z");
const DateTime time12_20_50 = ocpp::DateTime("2024-01-12T20:50:00Z");
const DateTime time17_16_00 = ocpp::DateTime("2024-01-17T16:00:00Z");
const DateTime time17_20_50 = ocpp::DateTime("2024-01-17T20:50:00Z");
const DateTime time31_16_00 = ocpp::DateTime("2024-01-31T16:00:00Z");

const DateTime time02_03_08_00 = ocpp::DateTime("2024-02-03T08:00:00Z");
const DateTime time02_03_14_10 = ocpp::DateTime("2024-02-03T14:10:00Z");
const DateTime time02_04_20_50 = ocpp::DateTime("2024-02-04T20:50:00Z");
const DateTime time02_04_08_00 = ocpp::DateTime("2024-02-04T08:00:00Z");
const DateTime time02_07_16_00 = ocpp::DateTime("2024-02-07T16:00:00Z");
const DateTime time02_10_20_50 = ocpp::DateTime("2024-02-10T20:50:00Z");
const DateTime time03_01_08_10 = ocpp::DateTime("2024-02-01T08:10:00Z");

const DateTime time2023_12_27_16_00 = ocpp::DateTime("2023-12-27T16:00:00Z");
const DateTime time2023_12_28_08_10 = ocpp::DateTime("2023-12-28T08:10:00Z");
const DateTime time2023_12_30_20_50 = ocpp::DateTime("2023-12-30T20:50:00Z");

ChargingProfile absolute_profile = SmartChargingTestUtils::get_charging_profile_from_file("singles/Absolute_301.json");
const ChargingProfile absolute_profile_no_duration =
    SmartChargingTestUtils::get_charging_profile_from_file("singles/Absolute_NoDuration_301.json");
const ChargingProfile relative_profile =
    SmartChargingTestUtils::get_charging_profile_from_file("singles/Relative_301.json");
const ChargingProfile relative_profile_no_duration =
    SmartChargingTestUtils::get_charging_profile_from_file("singles/Relative_NoDuration_301.json");
const ChargingProfile daily_profile =
    SmartChargingTestUtils::get_charging_profile_from_file("singles/Recurring_Daily_301.json");
const ChargingProfile daily_profile_no_duration =
    SmartChargingTestUtils::get_charging_profile_from_file("singles/Recurring_Daily_NoDuration_301.json");
const ChargingProfile weekly_profile =
    SmartChargingTestUtils::get_charging_profile_from_file("singles/Recurring_Weekly_301.json");

class ChargingProfileType_Param_Test
    : public ::testing::TestWithParam<std::tuple<ocpp::DateTime, ocpp::DateTime, std::optional<ocpp::DateTime>,
                                                 ChargingProfile, ocpp::DateTime, std::optional<ocpp::DateTime>>> {};

INSTANTIATE_TEST_SUITE_P(
    ChargingProfileType_Param_Test_Instantiate, ChargingProfileType_Param_Test,
    testing::Values(
        // Absolute Profiles
        // not started, started, finished, session started
        std::make_tuple(time11_50, time20_50, nullopt, absolute_profile, time12_02, nullopt),
        std::make_tuple(time12_10, time20_50, nullopt, absolute_profile, time12_02, nullopt),
        std::make_tuple(time14_10, time20_50, nullopt, absolute_profile, time12_02, nullopt),
        std::make_tuple(time12_10, time20_50, time12_05, absolute_profile, time12_02, nullopt),

        // Relative Profiles
        // not started, started, finished; session started: before, during & after profile
        std::make_tuple(time11_50, time20_50, nullopt, relative_profile, time11_50, nullopt),
        std::make_tuple(time12_10, time20_50, nullopt, relative_profile, time12_10, nullopt),
        std::make_tuple(time14_10, time20_50, nullopt, relative_profile, time14_10, nullopt),
        std::make_tuple(time12_10, time20_50, time11_50, relative_profile, time11_50, nullopt),
        std::make_tuple(time12_55, time20_50, time12_50, relative_profile, time12_50, nullopt),
        std::make_tuple(time14_15, time20_50, time12_10, relative_profile, time12_10, nullopt),

        // Recurring Daily Profiles
        // profile not started yet - start time is before profile is valid
        std::make_tuple(time11_50, time02_20_50, nullopt, daily_profile, time08_00, time02_08_00),
        // profile started - start time is before profile is valid
        std::make_tuple(time12_10, time02_20_50, nullopt, daily_profile, time08_00, time02_08_00),
        // start time is before profile is valid (and the previous day)
        std::make_tuple(time02_07_10, time02_20_50, nullopt, daily_profile, time08_00, time02_08_00),
        std::make_tuple(time02_08_10, time03_20_50, nullopt, daily_profile, time02_08_00, time03_08_00),
        std::make_tuple(time02_23_10, time03_20_50, nullopt, daily_profile, time02_08_00, time03_08_00),
        std::make_tuple(time03_07_10, time03_20_50, nullopt, daily_profile, time02_08_00, time03_08_00),
        // profile finished
        std::make_tuple(time02_03_14_10, time02_04_20_50, nullopt, daily_profile, time02_03_08_00, time02_04_08_00),
        // session started
        std::make_tuple(time05_12_10, time06_20_50, time06_08_00, daily_profile, time05_08_00, time06_08_00),

        // Recurring Weekly Profiles
        // profile not started yet - start time is before profile is valid
        std::make_tuple(time11_50, time07_20_50, nullopt, weekly_profile, time2023_12_27_16_00, time03_16_00),
        // profile started
        std::make_tuple(time12_10, time07_20_50, nullopt, weekly_profile, time2023_12_27_16_00, time03_16_00),
        std::make_tuple(time03_07_10, time07_20_50, nullopt, weekly_profile, time2023_12_27_16_00, time03_16_00),
        std::make_tuple(time03_23_10, time10_20_50, nullopt, weekly_profile, time03_16_00, time10_16_00),
        std::make_tuple(time04_23_10, time10_20_50, nullopt, weekly_profile, time03_16_00, time10_16_00),
        std::make_tuple(time10_07_10, time10_20_50, nullopt, weekly_profile, time03_16_00, time10_16_00),
        std::make_tuple(time10_20_10, time17_20_50, nullopt, weekly_profile, time10_16_00, time17_16_00),
        // profile finished
        std::make_tuple(time02_03_14_10, time02_10_20_50, nullopt, weekly_profile, time31_16_00, time02_07_16_00),
        // session started
        std::make_tuple(time04_23_10, time12_20_50, time05_11_50, weekly_profile, time03_16_00, time10_16_00)));

TEST_P(ChargingProfileType_Param_Test, CalculateSessionStart) {
    ocpp::DateTime now = std::get<0>(GetParam());
    ocpp::DateTime end = std::get<1>(GetParam());
    std::optional<ocpp::DateTime> session_start = std::get<2>(GetParam());
    ChargingProfile profile = std::get<3>(GetParam());
    DateTime expected_start_time = std::get<4>(GetParam());
    std::optional<ocpp::DateTime> second_session_start = std::get<5>(GetParam());

    std::vector<ocpp::DateTime> start_time = calculate_start(now, end, session_start, profile);

    for (DateTime t : start_time) {
        EVLOG_debug << "Start time: " << t.to_rfc3339();
    }

    if (!second_session_start.has_value()) {
        ASSERT_EQ(start_time.size(), 1);
        EXPECT_EQ(start_time[0], expected_start_time);
    } else {
        ASSERT_EQ(start_time.size(), 2);
        EXPECT_EQ(start_time[0], expected_start_time);
        EXPECT_EQ(start_time[1], second_session_start);
    }
}

TEST(ChargingProfileTypeTest, CalculateStartSingle) {
    // profile not started yet
    DateTime now("2024-01-01T11:50:00Z");
    DateTime end("2024-01-07T20:50:00Z");
    auto start_time = calculate_start(now, end, nullopt, weekly_profile);
    // start time is before profile is valid
    ASSERT_EQ(start_time.size(), 2);
    EXPECT_EQ(start_time[0].to_rfc3339(), "2023-12-27T16:00:00.000Z");
    EXPECT_EQ(start_time[1].to_rfc3339(), "2024-01-03T16:00:00.000Z");
}

class CalculateProfileEntryType_Param_Test
    : public ::testing::TestWithParam<
          std::tuple<ocpp::DateTime, ocpp::DateTime, std::optional<ocpp::DateTime>, ChargingProfile, ocpp::DateTime,
                     ocpp::DateTime, int, std::optional<ocpp::DateTime>, std::optional<ocpp::DateTime>>> {};

INSTANTIATE_TEST_SUITE_P(
    CalculateProfileEntryType_Param_Test_Instantiate, CalculateProfileEntryType_Param_Test,
    testing::Values(
        // Absolute Profiles
        // not started, started, finished, session started
        std::make_tuple(time12_10, time20_50, nullopt, absolute_profile, time12_02, time12_32, 0, nullopt, nullopt),
        std::make_tuple(time12_10, time20_50, nullopt, absolute_profile, time12_32, time12_47, 1, nullopt, nullopt),
        std::make_tuple(time12_10, time20_50, nullopt, absolute_profile, time12_47, time13_02, 2, nullopt, nullopt),
        std::make_tuple(time12_10, time20_50, nullopt, absolute_profile_no_duration, time12_47, time14_00, 2, nullopt,
                        nullopt),
        std::make_tuple(time12_20, time20_50, nullopt, relative_profile, time12_20, time12_50, 0, nullopt, nullopt),
        std::make_tuple(time12_20, time20_50, time12_15, relative_profile, time12_15, time12_45, 0, nullopt, nullopt),
        std::make_tuple(time12_20, time20_50, nullopt, relative_profile, time12_50, time13_05, 1, nullopt, nullopt),
        std::make_tuple(time12_20, time20_50, time12_15, relative_profile, time12_45, time13_00, 1, nullopt, nullopt),
        std::make_tuple(time12_20, time20_50, nullopt, relative_profile, time13_05, time13_20, 2, nullopt, nullopt),
        std::make_tuple(time12_20, time20_50, time12_15, relative_profile, time13_00, time13_15, 2, nullopt, nullopt),
        std::make_tuple(time12_20, time20_50, nullopt, relative_profile_no_duration, time13_05, time14_00, 2, nullopt,
                        nullopt),
        std::make_tuple(time12_20, time20_50, time12_15, relative_profile_no_duration, time13_00, time14_00, 2, nullopt,
                        nullopt),
        std::make_tuple(time02_08_10, time03_20_50, nullopt, daily_profile, time02_08_00, time02_08_30, 0, time03_08_00,
                        time03_08_30),
        std::make_tuple(time02_08_10, time03_20_50, nullopt, daily_profile, time02_08_30, time02_08_45, 1, time03_08_30,
                        time03_08_45),
        std::make_tuple(time02_08_10, time03_20_50, nullopt, daily_profile, time02_08_45, time02_09_00, 2, time03_08_45,
                        time03_09_00),
        std::make_tuple(time02_08_10, time04_08_00, nullopt, daily_profile_no_duration, time02_08_45, time03_08_00, 2,
                        time03_08_45, time04_08_00),
        std::make_tuple(time08_10, time02_20_50, nullopt, daily_profile, time02_08_45, time02_09_00, 2, nullopt,
                        nullopt),
        std::make_tuple(time08_10, time03_20_50, nullopt, daily_profile_no_duration, time12_00, time02_08_00, 2,
                        time02_08_45, time03_08_00),
        std::make_tuple(time03_16_10, time10_20_50, nullopt, weekly_profile, time03_16_00, time03_16_30, 0,
                        time10_16_00, time10_16_30),
        std::make_tuple(time03_16_10, time10_20_50, nullopt, weekly_profile, time03_16_30, time03_16_45, 1,
                        time10_16_30, time10_16_45),
        std::make_tuple(time03_16_10, time10_20_50, nullopt, weekly_profile, time03_16_45, time03_17_00, 2,
                        time10_16_45, time10_17_00)

            ));

TEST_P(CalculateProfileEntryType_Param_Test, CalculateProfileEntry_Positive) {

    ChargingProfile profile = std::get<3>(GetParam());
    int period_index = std::get<6>(GetParam());

    std::vector<period_entry_t> period_entries = calculate_profile_entry(
        std::get<0>(GetParam()), std::get<1>(GetParam()), std::get<2>(GetParam()), profile, std::get<6>(GetParam()));

    std::optional<ocpp::DateTime> expected_2nd_entry_start = std::get<7>(GetParam());
    std::optional<ocpp::DateTime> expected_2nd_entry_end = std::get<8>(GetParam());

    period_entry_t expected_entry{.start = std::get<4>(GetParam()),
                                  .end = std::get<5>(GetParam()),
                                  .limit = profile.chargingSchedule.front().chargingSchedulePeriod[period_index].limit,
                                  .stack_level = profile.stackLevel,
                                  .charging_rate_unit = profile.chargingSchedule.front().chargingRateUnit};

    for (period_entry_t pet : period_entries) {
        EVLOG_debug << ">>> " << pet;
    }

    EXPECT_EQ(period_entries.front(), expected_entry);

    if (!expected_2nd_entry_start.has_value()) {
        EXPECT_EQ(1, period_entries.size());
    } else {
        // EXPECT_EQ(2, period_entries.size());

        period_entry_t expected_second_entry{
            .start = expected_2nd_entry_start.value(),
            .end = expected_2nd_entry_end.value(),
            .limit = daily_profile_no_duration.chargingSchedule.front().chargingSchedulePeriod[2].limit,
            .stack_level = daily_profile_no_duration.stackLevel,
            .charging_rate_unit = daily_profile_no_duration.chargingSchedule.front().chargingRateUnit};
    }
}

class CalculateProfileEntryType_NegativeBoundary_Param_Test
    : public ::testing::TestWithParam<
          std::tuple<ocpp::DateTime, ocpp::DateTime, std::optional<ocpp::DateTime>, ChargingProfile, int>> {};

INSTANTIATE_TEST_SUITE_P(CalculateProfileEntryType_NegativeBoundary_Param_Test_Instantiate,
                         CalculateProfileEntryType_NegativeBoundary_Param_Test,
                         testing::Values(
                             // Absolute Profiles
                             // not started, started, finished, session started
                             std::make_tuple(time12_10, time20_50, nullopt, absolute_profile, 3),
                             std::make_tuple(time18_00, time20_50, nullopt, absolute_profile, 1),
                             std::make_tuple(time12_20, time20_50, nullopt, relative_profile, 3),
                             std::make_tuple(time12_20, time20_50, time12_15, relative_profile, 3),
                             std::make_tuple(time18_00, time20_50, nullopt, relative_profile_no_duration, 1),
                             std::make_tuple(time18_00, time20_50, time12_15, relative_profile_no_duration, 1),
                             std::make_tuple(time08_10, time20_50, nullopt, daily_profile, 3),
                             std::make_tuple(time03_01_08_10, time20_50, nullopt, daily_profile_no_duration, 1),
                             std::make_tuple(time2023_12_28_08_10, time2023_12_30_20_50, nullopt, daily_profile, 2)
                             //
                             ));

TEST_P(CalculateProfileEntryType_NegativeBoundary_Param_Test, CalculateProfileEntry_Negative) {
    ocpp::DateTime now = std::get<0>(GetParam());
    ocpp::DateTime end = std::get<1>(GetParam());
    std::optional<ocpp::DateTime> session_start = std::get<2>(GetParam());
    ChargingProfile profile = std::get<3>(GetParam());
    int period_index = std::get<4>(GetParam());

    std::vector<period_entry_t> period_entries =
        calculate_profile_entry(now, end, session_start, profile, period_index);

    ASSERT_EQ(period_entries.size(), 0);
}

TEST(OCPPTypesTest, PeriodEntry_Equality) {
    period_entry_t actual_entry{.start = time02_08_45,
                                .end = time03_08_00,
                                .limit = absolute_profile.chargingSchedule.front().chargingSchedulePeriod[0].limit,
                                .stack_level = absolute_profile.stackLevel,
                                .charging_rate_unit = absolute_profile.chargingSchedule.front().chargingRateUnit};
    period_entry_t same_entry = actual_entry;

    period_entry_t different_entry{.start = time03_08_00,
                                   .end = time03_08_00,
                                   .limit = absolute_profile.chargingSchedule.front().chargingSchedulePeriod[0].limit,
                                   .stack_level = absolute_profile.stackLevel,
                                   .charging_rate_unit = absolute_profile.chargingSchedule.front().chargingRateUnit};

    ASSERT_EQ(actual_entry, same_entry);
    ASSERT_NE(actual_entry, different_entry);
}

///
/// 2.0.1 version of the calculateProfileEntryRecurringDailyBeforeValid test
/// Daily at 08:00 valid from 12:00 2024-01-01 duration 1 hour
///
TEST(OCPPTypesTest, CalculateProfileEntry_DailyBeforeValid) {
    // Test Phase 0 - Both periods complete before the profile is valid
    ASSERT_EQ(
        0, calculate_profile_entry(time2023_12_28_08_10, time2023_12_30_20_50, std::nullopt, daily_profile, 2).size());

    // Test Phase 1 - only second period is valid
    period_entry_t expected_phase1_period_entry{
        .start = time02_08_45,
        .end = time02_09_00,
        .limit = daily_profile.chargingSchedule.front().chargingSchedulePeriod[2].limit,
        .stack_level = daily_profile.stackLevel,
        .charging_rate_unit = daily_profile.chargingSchedule.front().chargingRateUnit};

    std::vector<period_entry_t> period_entries =
        calculate_profile_entry(time08_10, time02_20_50, std::nullopt, daily_profile, 2);
    period_entry_t phase1_period_entry = period_entries.back();

    ASSERT_EQ(1, period_entries.size());
    // ASSERT_EQ(expected_phase1_period_entry, phase1_period_entry);
}

} // namespace