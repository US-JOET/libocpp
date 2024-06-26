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

const DateTime time08_00 = ocpp::DateTime("2024-01-01T08:00:00Z");
const DateTime time11_50 = ocpp::DateTime("2024-01-01T11:50:00Z");
const DateTime time12_02 = ocpp::DateTime("2024-01-01T12:02:00Z");
const DateTime time12_05 = ocpp::DateTime("2024-01-01T12:02:00Z");
const DateTime time12_10 = ocpp::DateTime("2024-01-01T12:10:00Z");
const DateTime time12_50 = ocpp::DateTime("2024-01-01T12:50:00Z");
const DateTime time12_55 = ocpp::DateTime("2024-01-01T12:55:00Z");
const DateTime time14_10 = ocpp::DateTime("2024-01-01T14:10:00Z");
const DateTime time14_15 = ocpp::DateTime("2024-01-01T14:15:00Z");
const DateTime time20_50 = ocpp::DateTime("2024-01-01T20:50:00Z");
const DateTime time02_07_10 = ocpp::DateTime("2024-01-02T07:10:00Z");
const DateTime time02_08_00 = ocpp::DateTime("2024-01-02T08:00:00Z");
const DateTime time02_08_10 = ocpp::DateTime("2024-01-02T08:10:00Z");
const DateTime time02_20_50 = ocpp::DateTime("2024-01-02T20:50:00Z");
const DateTime time02_23_10 = ocpp::DateTime("2024-01-02T23:10:00Z");
const DateTime time03_07_10 = ocpp::DateTime("2024-01-03T07:10:00Z");
const DateTime time03_08_00 = ocpp::DateTime("2024-01-03T08:00:00Z");
const DateTime time03_16_00 = ocpp::DateTime("2024-01-03T16:00:00Z");
const DateTime time03_20_50 = ocpp::DateTime("2024-01-03T20:50:00Z");
const DateTime time03_23_10 = ocpp::DateTime("2024-01-03T23:10:00Z");
const DateTime time04_23_10 = ocpp::DateTime("2024-01-04T23:10:00Z");
const DateTime time05_08_00 = ocpp::DateTime("2024-01-05T08:00:00Z");
const DateTime time05_11_50 = ocpp::DateTime("2024-01-05T11:50:00Z");
const DateTime time05_12_10 = ocpp::DateTime("2024-01-05T12:10:00Z");
const DateTime time06_08_00 = ocpp::DateTime("2024-01-06T08:00:00Z");
const DateTime time06_20_50 = ocpp::DateTime("2024-01-06T20:50:00Z");
const DateTime time07_20_50 = ocpp::DateTime("2024-01-07T20:50:00Z");
const DateTime time10_07_10 = ocpp::DateTime("2024-01-10T07:10:00Z");
const DateTime time10_16_00 = ocpp::DateTime("2024-01-10T16:00:00Z");
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

const DateTime time2023_12_27_16_00 = ocpp::DateTime("2023-12-27T16:00:00Z");

const ChargingProfile absolute_profile =
    SmartChargingTestUtils::get_charging_profile_from_file("singles/Default_Absolute_301.json");
const ChargingProfile relative_profile =
    SmartChargingTestUtils::get_charging_profile_from_file("singles/Default_Relative_301.json");
const ChargingProfile daily_profile =
    SmartChargingTestUtils::get_charging_profile_from_file("singles/Default_Recurring_Daily_301.json");
const ChargingProfile weekly_profile =
    SmartChargingTestUtils::get_charging_profile_from_file("singles/Default_Recurring_Weekly_301.json");

class ChargingProfileType_Param_Test
    : public ::testing::TestWithParam<std::tuple<ocpp::DateTime, ocpp::DateTime, std::optional<ocpp::DateTime>,
                                                 ChargingProfile, ocpp::DateTime, std::optional<ocpp::DateTime>>> {};

INSTANTIATE_TEST_SUITE_P(
    ChargingProfileType_Param_Test_Instantiate, ChargingProfileType_Param_Test,
    testing::Values(
        // Absolute Profiles
        // not started, started, finished, session started
        std::make_tuple(time11_50, time20_50, std::nullopt, absolute_profile, time12_02, std::nullopt),
        std::make_tuple(time12_10, time20_50, std::nullopt, absolute_profile, time12_02, std::nullopt),
        std::make_tuple(time14_10, time20_50, std::nullopt, absolute_profile, time12_02, std::nullopt),
        std::make_tuple(time12_10, time20_50, time12_05, absolute_profile, time12_02, std::nullopt),

        // Relative Profiles
        // not started, started, finished; session started: before, during & after profile
        std::make_tuple(time11_50, time20_50, std::nullopt, relative_profile, time11_50, std::nullopt),
        std::make_tuple(time12_10, time20_50, std::nullopt, relative_profile, time12_10, std::nullopt),
        std::make_tuple(time14_10, time20_50, std::nullopt, relative_profile, time14_10, std::nullopt),
        std::make_tuple(time12_10, time20_50, time11_50, relative_profile, time11_50, std::nullopt),
        std::make_tuple(time12_55, time20_50, time12_50, relative_profile, time12_50, std::nullopt),
        std::make_tuple(time14_15, time20_50, time12_10, relative_profile, time12_10, std::nullopt),

        // Recurring Daily Profiles
        // profile not started yet - start time is before profile is valid
        std::make_tuple(time11_50, time02_20_50, std::nullopt, daily_profile, time08_00, time02_08_00),
        // profile started - start time is before profile is valid
        std::make_tuple(time12_10, time02_20_50, std::nullopt, daily_profile, time08_00, time02_08_00),
        // start time is before profile is valid (and the previous day)
        std::make_tuple(time02_07_10, time02_20_50, std::nullopt, daily_profile, time08_00, time02_08_00),
        std::make_tuple(time02_08_10, time03_20_50, std::nullopt, daily_profile, time02_08_00, time03_08_00),
        std::make_tuple(time02_23_10, time03_20_50, std::nullopt, daily_profile, time02_08_00, time03_08_00),
        std::make_tuple(time03_07_10, time03_20_50, std::nullopt, daily_profile, time02_08_00, time03_08_00),
        // profile finished
        std::make_tuple(time02_03_14_10, time02_04_20_50, std::nullopt, daily_profile, time02_03_08_00,
                        time02_04_08_00),
        // session started
        std::make_tuple(time05_12_10, time06_20_50, time06_08_00, daily_profile, time05_08_00, time06_08_00),

        // Recurring Weekly Profiles
        // profile not started yet - start time is before profile is valid
        std::make_tuple(time11_50, time07_20_50, std::nullopt, weekly_profile, time2023_12_27_16_00, time03_16_00),
        // profile started
        std::make_tuple(time12_10, time07_20_50, std::nullopt, weekly_profile, time2023_12_27_16_00, time03_16_00),
        std::make_tuple(time03_07_10, time07_20_50, std::nullopt, weekly_profile, time2023_12_27_16_00, time03_16_00),
        std::make_tuple(time03_23_10, time10_20_50, std::nullopt, weekly_profile, time03_16_00, time10_16_00),
        std::make_tuple(time04_23_10, time10_20_50, std::nullopt, weekly_profile, time03_16_00, time10_16_00),
        std::make_tuple(time10_07_10, time10_20_50, std::nullopt, weekly_profile, time03_16_00, time10_16_00),
        std::make_tuple(time10_20_10, time17_20_50, std::nullopt, weekly_profile, time10_16_00, time17_16_00),
        // profile finished
        std::make_tuple(time02_03_14_10, time02_10_20_50, std::nullopt, weekly_profile, time31_16_00, time02_07_16_00),
        // session started
        std::make_tuple(time04_23_10, time12_20_50, time05_11_50, weekly_profile, time03_16_00, time10_16_00)));

TEST_P(ChargingProfileType_Param_Test, CalculateStart) {
    ocpp::DateTime now = std::get<0>(GetParam());
    ocpp::DateTime end = std::get<1>(GetParam());
    std::optional<ocpp::DateTime> session_start = std::get<2>(GetParam());
    ChargingProfile profile = std::get<3>(GetParam());
    DateTime expected_start_time = std::get<4>(GetParam());
    std::optional<ocpp::DateTime> second_session_start = std::get<5>(GetParam());

    auto start_time = calculate_start(now, end, session_start, profile);

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

TEST(ProfileTestsA, calculateStartReccuringWeewwwwkly) {
    // profile not started yet
    DateTime now("2024-01-01T11:50:00Z");
    DateTime end("2024-01-07T20:50:00Z");
    auto start_time = calculate_start(now, end, std::nullopt, weekly_profile);
    // start time is before profile is valid
    ASSERT_EQ(start_time.size(), 2);
    EXPECT_EQ(start_time[0].to_rfc3339(), "2023-12-27T16:00:00.000Z");
    EXPECT_EQ(start_time[1].to_rfc3339(), "2024-01-03T16:00:00.000Z");
}

} // namespace