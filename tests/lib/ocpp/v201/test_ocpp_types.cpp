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
using std::chrono::minutes;

// auto dt = [](std::string dt_string) -> ocpp::DateTime { return SmartChargingTestUtils::dt(dt_string); };

static ocpp::DateTime dt(std::string dt_string) {
    ocpp::DateTime dt;

    if (dt_string.length() == 4) {
        dt = ocpp::DateTime("2024-01-01T0" + dt_string + ":00Z");
    } else if (dt_string.length() == 5) {
        dt = ocpp::DateTime("2024-01-01T" + dt_string + ":00Z");
    } else if (dt_string.length() == 7) {
        dt = ocpp::DateTime("2024-01-0" + dt_string + ":00Z");
    } else if (dt_string.length() == 8) {
        dt = ocpp::DateTime("2024-01-" + dt_string + ":00Z");
    } else if (dt_string.length() == 11) {
        dt = ocpp::DateTime("2024-" + dt_string + ":00Z");
    } else if (dt_string.length() == 16) {
        dt = ocpp::DateTime(dt_string + ":00Z");
    }

    return dt;
}

period_entry_t gen_pe(ocpp::DateTime start, ocpp::DateTime end, ChargingProfile profile, int period_at) {
    return {.start = start,
            .end = end,
            .limit = profile.chargingSchedule.front().chargingSchedulePeriod[period_at].limit,
            .stack_level = profile.stackLevel,
            .charging_rate_unit = profile.chargingSchedule.front().chargingRateUnit};
}

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
const ChargingProfile weekly_profile_no_duration =
    SmartChargingTestUtils::get_charging_profile_from_file("singles/Recurring_Weekly_NoDuration_301.json");

class ChargingProfileType_Param_Test
    : public ::testing::TestWithParam<std::tuple<ocpp::DateTime, ocpp::DateTime, std::optional<ocpp::DateTime>,
                                                 ChargingProfile, ocpp::DateTime, std::optional<ocpp::DateTime>>> {};

INSTANTIATE_TEST_SUITE_P(
    ChargingProfileType_Param_Test_Instantiate, ChargingProfileType_Param_Test,
    testing::Values(
        // Absolute Profiles
        // not started, started, finished, session started
        std::make_tuple(dt("11:50"), dt("20:50"), nullopt, absolute_profile, dt("12:02"), nullopt),
        std::make_tuple(dt("12:10"), dt("20:50"), nullopt, absolute_profile, dt("12:02"), nullopt),
        std::make_tuple(dt("14:10"), dt("20:50"), nullopt, absolute_profile, dt("12:02"), nullopt),
        std::make_tuple(dt("12:10"), dt("20:50"), dt("12:05"), absolute_profile, dt("12:02"), nullopt),

        // Relative Profiles
        // not started, started, finished; session started: before, during & after profile
        std::make_tuple(dt("11:50"), dt("20:50"), nullopt, relative_profile, dt("11:50"), nullopt),
        std::make_tuple(dt("12:10"), dt("20:50"), nullopt, relative_profile, dt("12:10"), nullopt),
        std::make_tuple(dt("14:10"), dt("20:50"), nullopt, relative_profile, dt("14:10"), nullopt),
        std::make_tuple(dt("12:10"), dt("20:50"), dt("11:50"), relative_profile, dt("11:50"), nullopt),
        std::make_tuple(dt("12:55"), dt("20:50"), dt("12:50"), relative_profile, dt("12:50"), nullopt),
        std::make_tuple(dt("14:15"), dt("20:50"), dt("12:10"), relative_profile, dt("12:10"), nullopt),

        // Recurring Daily Profiles
        // profile not started yet - start time is before profile is valid
        std::make_tuple(dt("11:50"), dt("2T20:50"), nullopt, daily_profile, dt("8:00"), dt("2T08:00")),
        // profile started - start time is before profile is valid
        std::make_tuple(dt("12:10"), dt("2T20:50"), nullopt, daily_profile, dt("8:00"), dt("2T08:00")),
        // start time is before profile is valid (and the previous day)
        std::make_tuple(dt("2T07:10"), dt("2T20:50"), nullopt, daily_profile, dt("8:00"), dt("2T08:00")),
        std::make_tuple(dt("2T08:10"), dt("3T20:50"), nullopt, daily_profile, dt("2T08:00"), dt("3T08:00")),
        std::make_tuple(dt("2T23:10"), dt("3T20:50"), nullopt, daily_profile, dt("2T08:00"), dt("3T08:00")),
        std::make_tuple(dt("3T07:10"), dt("3T20:50"), nullopt, daily_profile, dt("2T08:00"), dt("3T08:00")),
        // profile finished
        std::make_tuple(dt("02-03T14:10"), dt("02-04T20:50"), nullopt, daily_profile, dt("02-03T08:00"),
                        dt("02-04T08:00")),
        // session started
        std::make_tuple(dt("5T12:10"), dt("6T20:50"), dt("6T08:00"), daily_profile, dt("5T08:00"), dt("6T08:00")),

        // Recurring Weekly Profiles
        // profile not started yet - start time is before profile is valid
        std::make_tuple(dt("11:50"), dt("7T20:50"), nullopt, weekly_profile, dt("2023-12-27T16:00"), dt("3T16:00")),
        // profile started
        std::make_tuple(dt("12:10"), dt("7T20:50"), nullopt, weekly_profile, dt("2023-12-27T16:00"), dt("3T16:00")),
        std::make_tuple(dt("3T07:10"), dt("7T20:50"), nullopt, weekly_profile, dt("2023-12-27T16:00"), dt("3T16:00")),
        std::make_tuple(dt("3T23:10"), dt("10T20:50"), nullopt, weekly_profile, dt("3T16:00"), dt("10T16:00")),
        std::make_tuple(dt("4T23:10"), dt("10T20:50"), nullopt, weekly_profile, dt("3T16:00"), dt("10T16:00")),
        std::make_tuple(dt("10T07:10"), dt("10T20:50"), nullopt, weekly_profile, dt("3T16:00"), dt("10T16:00")),
        std::make_tuple(dt("10T20:10"), dt("17T20:50"), nullopt, weekly_profile, dt("10T16:00"), dt("17T16:00")),
        // profile finished
        std::make_tuple(dt("02-03T14:10"), dt("02-10T20:50"), nullopt, weekly_profile, dt("31T16:00"),
                        dt("02-07T16:00")),
        // session started
        std::make_tuple(dt("4T23:10"), dt("12T20:50"), dt("5T11:50"), weekly_profile, dt("3T16:00"), dt("10T16:00"))));

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

INSTANTIATE_TEST_SUITE_P(CalculateProfileEntryType_Param_Test_Instantiate, CalculateProfileEntryType_Param_Test,
                         testing::Values(
                             // Absolute Profiles
                             // not started, started, finished, session started
                             std::make_tuple(dt("12:10"), dt("20:50"), nullopt, absolute_profile, dt("12:02"),
                                             dt("12:32"), 0, nullopt, nullopt),
                             std::make_tuple(dt("12:10"), dt("20:50"), nullopt, absolute_profile, dt("12:32"),
                                             dt("12:47"), 1, nullopt, nullopt),
                             std::make_tuple(dt("12:10"), dt("20:50"), nullopt, absolute_profile, dt("12:47"),
                                             dt("13:02"), 2, nullopt, nullopt),
                             std::make_tuple(dt("12:10"), dt("20:50"), nullopt, absolute_profile_no_duration,
                                             dt("12:47"), dt("14:00"), 2, nullopt, nullopt),
                             std::make_tuple(dt("12:20"), dt("20:50"), nullopt, relative_profile, dt("12:20"),
                                             dt("12:50"), 0, nullopt, nullopt),
                             std::make_tuple(dt("12:20"), dt("20:50"), dt("12:15"), relative_profile, dt("12:15"),
                                             dt("12:45"), 0, nullopt, nullopt),
                             std::make_tuple(dt("12:20"), dt("20:50"), nullopt, relative_profile, dt("12:50"),
                                             dt("13:05"), 1, nullopt, nullopt),
                             std::make_tuple(dt("12:20"), dt("20:50"), dt("12:15"), relative_profile, dt("12:45"),
                                             dt("13:00"), 1, nullopt, nullopt),
                             std::make_tuple(dt("12:20"), dt("20:50"), nullopt, relative_profile, dt("13:05"),
                                             dt("13:20"), 2, nullopt, nullopt),
                             std::make_tuple(dt("12:20"), dt("20:50"), dt("12:15"), relative_profile, dt("13:00"),
                                             dt("13:15"), 2, nullopt, nullopt),
                             std::make_tuple(dt("12:20"), dt("20:50"), nullopt, relative_profile_no_duration,
                                             dt("13:05"), dt("14:00"), 2, nullopt, nullopt),
                             std::make_tuple(dt("12:20"), dt("20:50"), dt("12:15"), relative_profile_no_duration,
                                             dt("13:00"), dt("14:00"), 2, nullopt, nullopt),
                             std::make_tuple(dt("2T08:10"), dt("3T20:50"), nullopt, daily_profile, dt("2T08:00"),
                                             dt("2T08:30"), 0, dt("3T08:00"), dt("3T08:30")),
                             std::make_tuple(dt("2T08:10"), dt("3T20:50"), nullopt, daily_profile, dt("2T08:30"),
                                             dt("2T08:45"), 1, dt("3T08:30"), dt("3T08:45")),
                             std::make_tuple(dt("2T08:10"), dt("3T20:50"), nullopt, daily_profile, dt("2T08:45"),
                                             dt("2T09:00"), 2, dt("3T08:45"), dt("3T09:00")),
                             std::make_tuple(dt("2T08:10"), dt("4T08:00"), nullopt, daily_profile_no_duration,
                                             dt("2T08:45"), dt("3T08:00"), 2, dt("3T08:45"), dt("4T08:00")),
                             std::make_tuple(dt("8:10"), dt("2T20:50"), nullopt, daily_profile, dt("2T08:45"),
                                             dt("2T09:00"), 2, nullopt, nullopt),
                             std::make_tuple(dt("8:10"), dt("3T20:50"), nullopt, daily_profile_no_duration, dt("12:00"),
                                             dt("2T08:00"), 2, dt("2T08:45"), dt("3T08:00")),
                             std::make_tuple(dt("3T16:10"), dt("10T20:50"), nullopt, weekly_profile, dt("3T16:00"),
                                             dt("3T16:30"), 0, dt("10T16:00"), dt("10T16:30")),
                             std::make_tuple(dt("3T16:10"), dt("10T20:50"), nullopt, weekly_profile, dt("3T16:30"),
                                             dt("3T16:45"), 1, dt("10T16:30"), dt("10T16:45")),
                             std::make_tuple(dt("3T16:10"), dt("10T20:50"), nullopt, weekly_profile_no_duration,
                                             dt("3T16:45"), dt("10T16:00"), 2, dt("10T16:45"), dt("17T16:00")),
                             std::make_tuple(dt("2023-12-30T08:10"), dt("3T20:50"), nullopt, weekly_profile,
                                             dt("3T16:45"), dt("3T17:00"), 2, nullopt, nullopt),
                             std::make_tuple(dt("2023-12-30T08:10"), dt("10T20:50"), nullopt,
                                             weekly_profile_no_duration, dt("12:00"), dt("3T16:00"), 2, dt("3T16:45"),
                                             dt("10T16:00"))

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
                             std::make_tuple(dt("12:10"), dt("20:50"), nullopt, absolute_profile, 3),
                             std::make_tuple(dt("18:00"), dt("20:50"), nullopt, absolute_profile, 1),
                             std::make_tuple(dt("12:20"), dt("20:50"), nullopt, relative_profile, 3),
                             std::make_tuple(dt("12:20"), dt("20:50"), dt("12:15"), relative_profile, 3),
                             std::make_tuple(dt("18:00"), dt("20:50"), nullopt, relative_profile_no_duration, 1),
                             std::make_tuple(dt("18:00"), dt("20:50"), dt("12:15"), relative_profile_no_duration, 1),
                             std::make_tuple(dt("8:10"), dt("20:50"), nullopt, daily_profile, 3),
                             std::make_tuple(dt("03-01T08:10"), dt("20:50"), nullopt, daily_profile_no_duration, 1),
                             std::make_tuple(dt("3T16:10"), dt("20:50"), nullopt, weekly_profile_no_duration, 3),
                             std::make_tuple(dt("03-01T08:10"), dt("03-10T20:50"), nullopt, weekly_profile, 1),
                             std::make_tuple(dt("2023-12-27T08:10"), dt("20:50"), nullopt, weekly_profile, 2)));

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
    period_entry_t actual_entry{.start = dt("2T08:45"),
                                .end = dt("3T08:00"),
                                .limit = absolute_profile.chargingSchedule.front().chargingSchedulePeriod[0].limit,
                                .stack_level = absolute_profile.stackLevel,
                                .charging_rate_unit = absolute_profile.chargingSchedule.front().chargingRateUnit};
    period_entry_t same_entry = actual_entry;

    period_entry_t different_entry{.start = dt("3T08:00"),
                                   .end = dt("3T08:00"),
                                   .limit = absolute_profile.chargingSchedule.front().chargingSchedulePeriod[0].limit,
                                   .stack_level = absolute_profile.stackLevel,
                                   .charging_rate_unit = absolute_profile.chargingSchedule.front().chargingRateUnit};

    ASSERT_EQ(actual_entry, same_entry);
    ASSERT_NE(actual_entry, different_entry);
}

class CalculateProfileType_Param_Test
    : public ::testing::TestWithParam<
          std::tuple<ocpp::DateTime, ocpp::DateTime, ocpp::DateTime, ChargingProfile, std::optional<int>>> {};

INSTANTIATE_TEST_SUITE_P(
    CalculateProfileType_Param_Test_Instantiate, CalculateProfileType_Param_Test,
    testing::Values(
        // Absolute Profiles
        // not started, started, finished, session started
        std::make_tuple(dt("8:10"), dt("20:50"), dt("2023-12-27T08:05"), absolute_profile, nullopt),
        std::make_tuple(dt("12:01"), dt("20:50"), dt("2023-12-27T08:05"), absolute_profile, nullopt),
        std::make_tuple(dt("12:40"), dt("20:50"), dt("2023-12-27T08:05"), absolute_profile, 2),
        std::make_tuple(dt("14:01"), dt("20:50"), dt("2023-12-27T08:05"), absolute_profile, 0)));

TEST_P(CalculateProfileType_Param_Test, CalculateProfileDirect) {
    ocpp::DateTime now = std::get<0>(GetParam());
    ocpp::DateTime end = std::get<1>(GetParam());
    ocpp::DateTime session_start = std::get<2>(GetParam());
    ChargingProfile profile = std::get<3>(GetParam());
    std::optional<int> size = std::get<4>(GetParam());

    std::vector<period_entry_t> period_entries_no_session = calculate_profile(now, end, nullopt, profile);
    std::vector<period_entry_t> period_entries = calculate_profile(now, end, session_start, absolute_profile);

    // If no size is passed than it's the size of the Profile's
    ASSERT_EQ(size.value_or(profile.chargingSchedule.front().chargingSchedulePeriod.size()),
              period_entries_no_session.size());
    EXPECT_EQ(period_entries_no_session, period_entries);
    EXPECT_TRUE(SmartChargingTestUtils::validate_profile_result(period_entries_no_session));

    // ASSERT_EQ(period_entries.size(), 0);
}

TEST(OCPPTypesTest, CalculateProfile_Absolute) {
    // before start expecting all periods to be included
    std::vector<period_entry_t> period_entries_no_session =
        calculate_profile(dt("8:10"), dt("20:50"), nullopt, absolute_profile);
    std::vector<period_entry_t> period_entries_before =
        calculate_profile(dt("8:10"), dt("20:50"), dt("2023-12-27T08:05"), absolute_profile);

    ASSERT_EQ(absolute_profile.chargingSchedule.front().chargingSchedulePeriod.size(),
              period_entries_no_session.size());
    ASSERT_EQ(absolute_profile.chargingSchedule.front().chargingSchedulePeriod.size(), period_entries_before.size());
    EXPECT_EQ(period_entries_no_session, period_entries_before);
    EXPECT_TRUE(SmartChargingTestUtils::validate_profile_result(period_entries_no_session));

    // just before start
    period_entries_no_session = calculate_profile(dt("12:01"), dt("20:50"), nullopt, absolute_profile);
    period_entries_before = calculate_profile(dt("12:01"), dt("20:50"), dt("2023-12-27T08:05"), absolute_profile);

    ASSERT_EQ(absolute_profile.chargingSchedule.front().chargingSchedulePeriod.size(),
              period_entries_no_session.size());
    ASSERT_EQ(absolute_profile.chargingSchedule.front().chargingSchedulePeriod.size(), period_entries_before.size());
    EXPECT_EQ(period_entries_no_session, period_entries_before);
    EXPECT_TRUE(SmartChargingTestUtils::validate_profile_result(period_entries_no_session));

    // during
    period_entries_no_session = calculate_profile(dt("12:40"), dt("20:50"), nullopt, absolute_profile);
    period_entries_before = calculate_profile(dt("12:40"), dt("20:50"), dt("2023-12-27T08:05"), absolute_profile);

    ASSERT_EQ(2, period_entries_no_session.size());
    ASSERT_EQ(2, period_entries_before.size());
    EXPECT_EQ(period_entries_no_session, period_entries_before);
    EXPECT_TRUE(SmartChargingTestUtils::validate_profile_result(period_entries_no_session));

    // after
    period_entries_no_session = calculate_profile(dt("14:01"), dt("20:50"), nullopt, absolute_profile);
    period_entries_before = calculate_profile(dt("14:01"), dt("20:50"), dt("2023-12-27T08:05"), absolute_profile);

    ASSERT_EQ(0, period_entries_no_session.size());
    ASSERT_EQ(0, period_entries_before.size());
    EXPECT_EQ(period_entries_no_session, period_entries_before);
    EXPECT_TRUE(SmartChargingTestUtils::validate_profile_result(period_entries_no_session));
}

TEST(OCPPTypesTest, CalculateProfile_AbsoluteLimited) {
    // Before start expecting no periods
    ASSERT_EQ(
        0, calculate_profile(dt("8:10"), DateTime(dt("8:10").to_time_point() + minutes(20)), nullopt, absolute_profile)
               .size());

    // Just before start expecting a single period
    std::vector<period_entry_t> period_entries_just_before_start =
        calculate_profile(dt("12:01"), DateTime(dt("12:01").to_time_point() + minutes(20)), nullopt, absolute_profile);

    ASSERT_EQ(1, period_entries_just_before_start.size());
    EXPECT_TRUE(SmartChargingTestUtils::validate_profile_result(period_entries_just_before_start));
    ASSERT_EQ(gen_pe(dt("12:02"), dt("12:32"), absolute_profile, 0), period_entries_just_before_start.front());

    // During start expecting 2 periods
    std::vector<period_entry_t> period_entries_during_start =
        calculate_profile(dt("12:40"), DateTime(dt("12:40").to_time_point() + minutes(20)), nullopt, absolute_profile);

    ASSERT_EQ(2, period_entries_during_start.size());
    ASSERT_EQ(gen_pe(dt("12:32"), dt("12:47"), absolute_profile, 1), period_entries_during_start.front());
    ASSERT_EQ(gen_pe(dt("12:47"), dt("13:02"), absolute_profile, 2), period_entries_during_start.back());
    EXPECT_TRUE(SmartChargingTestUtils::validate_profile_result(period_entries_during_start));

    // After expecting no periods
    ASSERT_EQ(0, calculate_profile(dt("14:01"), DateTime(dt("14:01").to_time_point() + minutes(20)), nullopt,
                                   absolute_profile)
                     .size());
}

TEST(OCPPTypesTest, CalculateProfile_Relative) {
    // Before start expecting no periods
    ASSERT_EQ(0, calculate_profile(dt("8:10"), dt("20:50"), std::nullopt, relative_profile).size());
    ASSERT_EQ(0, calculate_profile(dt("8:10"), dt("20:50"), dt("2023-12-27T08:05"), relative_profile).size());

    // JUST BEFORE START
    // Expecting all periods
    std::vector<period_entry_t> pe_before_no_session =
        calculate_profile(dt("11:58"), dt("20:50"), std::nullopt, relative_profile);
    std::vector<period_entry_t> pe_before = calculate_profile(dt("11:58"), dt("20:50"), dt("11:55"), relative_profile);

    // While the period entries should have the same length, adding a session start should change the result
    ASSERT_EQ(pe_before_no_session.size(), relative_profile.chargingSchedule.front().chargingSchedulePeriod.size());
    ASSERT_EQ(pe_before.size(), relative_profile.chargingSchedule.front().chargingSchedulePeriod.size());
    EXPECT_NE(pe_before_no_session, pe_before);

    // Validate period entries with no session
    ASSERT_EQ(gen_pe(dt("12:00"), dt("12:28"), relative_profile, 0), pe_before_no_session.at(0));
    ASSERT_EQ(gen_pe(dt("12:28"), dt("12:43"), relative_profile, 1), pe_before_no_session.at(1));
    ASSERT_EQ(gen_pe(dt("12:43"), dt("12:58"), relative_profile, 2), pe_before_no_session.at(2));

    // Validate period entries with session
    ASSERT_EQ(gen_pe(dt("12:00"), dt("12:25"), relative_profile, 0), pe_before.at(0));
    ASSERT_EQ(gen_pe(dt("12:25"), dt("12:40"), relative_profile, 1), pe_before.at(1));
    ASSERT_EQ(gen_pe(dt("12:40"), dt("12:55"), relative_profile, 2), pe_before.at(2));

    // During START
    // Expecting all periods for no session and 2 periods when there is an existing session
    std::vector<period_entry_t> pe_during_no_session =
        calculate_profile(dt("12:40"), dt("20:50"), nullopt, relative_profile);
    std::vector<period_entry_t> pe_during = calculate_profile(dt("12:40"), dt("20:50"), dt("12:38"), relative_profile);

    ASSERT_EQ(3, pe_during_no_session.size());
    ASSERT_EQ(3, pe_during.size());
    // the session start should change the result
    EXPECT_NE(pe_during_no_session, pe_during);
    EXPECT_TRUE(SmartChargingTestUtils::validate_profile_result(pe_during_no_session));
    EXPECT_TRUE(SmartChargingTestUtils::validate_profile_result(pe_during));

    ASSERT_EQ(gen_pe(dt("12:38"), dt("13:08"), relative_profile, 0), pe_during.at(0));
    ASSERT_EQ(gen_pe(dt("13:08"), dt("13:23"), relative_profile, 1), pe_during.at(1));
    ASSERT_EQ(gen_pe(dt("13:23"), dt("13:38"), relative_profile, 2), pe_during.at(2));

    // During, but a bit later now only creates 2 periods with an existing sesion
    std::vector<period_entry_t> pe_during_later_no_session =
        calculate_profile(dt("13:10"), dt("20:50"), nullopt, relative_profile);
    std::vector<period_entry_t> pe_during_later =
        calculate_profile(dt("13:10"), dt("20:50"), dt("12:38"), relative_profile);

    ASSERT_EQ(3, pe_during_later_no_session.size());
    ASSERT_EQ(2, pe_during_later.size());
    EXPECT_NE(pe_during_later_no_session, pe_during_later);
    EXPECT_TRUE(SmartChargingTestUtils::validate_profile_result(pe_during_later_no_session));
    EXPECT_TRUE(SmartChargingTestUtils::validate_profile_result(pe_during_later));

    for (period_entry_t pet : pe_during_later) {
        EVLOG_debug << ">>> " << pet;
    }

    ASSERT_EQ(gen_pe(dt("13:08"), dt("13:23"), relative_profile, 1), pe_during_later.at(0));
    ASSERT_EQ(gen_pe(dt("13:23"), dt("13:38"), relative_profile, 2), pe_during_later.at(1));

    // After
    ASSERT_EQ(0, calculate_profile(dt("14:02"), dt("14:01"), nullopt, relative_profile).size());
    ASSERT_EQ(0, calculate_profile(dt("14:02"), dt("14:01"), dt("14:01"), relative_profile).size());
}

} // namespace