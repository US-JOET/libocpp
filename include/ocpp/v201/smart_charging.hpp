// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2023 Pionix GmbH and Contributors to EVerest

#ifndef OCPP_V201_SMART_CHARGING_HPP
#define OCPP_V201_SMART_CHARGING_HPP

#include "ocpp/v201/enums.hpp"
#include <limits>

#include <ocpp/v201/database_handler.hpp>
#include <ocpp/v201/evse.hpp>
#include <ocpp/v201/ocpp_types.hpp>
#include <ocpp/v201/transaction.hpp>

namespace ocpp::v201 {

const int LOW_VOLTAGE = 230;
const int DEFAULT_AND_MAX_NUMBER_PHASES = 3;
const int HOURS_PER_DAY = 24;
const int SECONDS_PER_HOUR = 3600;
const int SECONDS_PER_DAY = 86400;
const int DAYS_PER_WEEK = 7;

enum class ProfileValidationResultEnum {
    Valid,
    EvseDoesNotExist,
    TxProfileMissingTransactionId,
    TxProfileEvseIdNotGreaterThanZero,
    TxProfileTransactionNotOnEvse,
    TxProfileEvseHasNoActiveTransaction,
    TxProfileConflictingStackLevel,
    ChargingProfileNoChargingSchedulePeriods,
    ChargingProfileFirstStartScheduleIsNotZero,
    ChargingProfileMissingRequiredStartSchedule,
    ChargingProfileExtraneousStartSchedule,
    ChargingSchedulePeriodsOutOfOrder,
    ChargingSchedulePeriodInvalidPhaseToUse,
    ChargingSchedulePeriodUnsupportedNumberPhases,
    ChargingSchedulePeriodExtraneousPhaseValues,
    DuplicateTxDefaultProfileFound
};

namespace conversions {
/// \brief Converts the given ProfileValidationResultEnum \p e to human readable string
/// \returns a string representation of the ProfileValidationResultEnum
std::string profile_validation_result_to_string(ProfileValidationResultEnum e);
} // namespace conversions

std::ostream& operator<<(std::ostream& os, const ProfileValidationResultEnum validation_result);

/// \brief Helper struct to calculate Composite Schedule
struct LimitStackLevelPair {
    int limit;
    int stack_level;
};

/// \brief Helper struct to calculate Composite Schedule
struct PeriodDateTimePair {
    std::optional<ChargingSchedulePeriod> period;
    ocpp::DateTime end_time;
};

/// \brief This class handles and maintains incoming ChargingProfiles and contains the logic
/// to calculate the composite schedules
class SmartChargingHandler {
private:
    std::map<int32_t, std::unique_ptr<EvseInterface>>& evses;

    std::shared_ptr<ocpp::v201::DatabaseHandler> database_handler;
    // cppcheck-suppress unusedStructMember
    std::map<int32_t, std::vector<ChargingProfile>> charging_profiles;
    std::vector<ChargingProfile> station_wide_charging_profiles;

public:
    explicit SmartChargingHandler(std::map<int32_t, std::unique_ptr<EvseInterface>>& evses);

    ///
    /// \brief validates the existence of the given \p evse_id according to the specification
    ///
    ProfileValidationResultEnum validate_evse_exists(int32_t evse_id) const;

    ///
    /// \brief validates the given \p profile and associated \p evse_id according to the specification
    ///
    ProfileValidationResultEnum validate_tx_default_profile(const ChargingProfile& profile, int32_t evse_id) const;

    ///
    /// \brief validates the given \p profile according to the specification
    ///
    ProfileValidationResultEnum validate_tx_profile(const ChargingProfile& profile, EvseInterface& evse) const;

    /// \brief validates that the given \p profile has valid charging schedules.
    /// If a profiles charging schedule period does not have a valid numberPhases,
    /// we set it to the default value (3).
    ProfileValidationResultEnum validate_profile_schedules(ChargingProfile& profile,
                                                           std::optional<EvseInterface*> evse_opt = std::nullopt) const;

    ///
    /// \brief Adds a given \p profile and associated \p evse_id to our stored list of profiles
    ///
    void add_profile(int32_t evse_id, ChargingProfile& profile);

    ///
    /// \brief Iterates over the periods of the given \p profile and returns a struct that contains the period and the
    /// absolute end time of the period that refers to the given absoulte \p time as a pair.
    ///
    PeriodDateTimePair find_period_at(const ocpp::DateTime& time, const ChargingProfile& profile, const int evse);

    ///
    /// \brief Gets the absolute start time of the given \p profile for the given \p connector_id for different profile
    /// purposes
    ///
    std::optional<ocpp::DateTime> get_profile_start_time(const ChargingProfile& profile, const ocpp::DateTime& time,
                                                         const int evse);

    ///
    /// \brief Calculates the composite schedule for the given \p valid_profiles and the given \p connector_id
    ///
    CompositeSchedule calculate_composite_schedule(std::vector<ChargingProfile> valid_profiles,
                                                   const ocpp::DateTime& start_time, const ocpp::DateTime& end_time,
                                                   const int32_t evse_id, ChargingRateUnitEnum charging_rate_unit);

    static int32_t determine_duration(const ocpp::DateTime& start_time, const ocpp::DateTime& end_time);

    static bool within_time_window(const ocpp::DateTime& start_time, const ocpp::DateTime& end_time);

    ///
    /// \brief Iterates over the periods of the given \p valid_profiles and determines the earliest next absolute period
    /// end time later than \p temp_time
    ///
    ocpp::DateTime get_next_temp_time(const ocpp::DateTime temp_time,
                                      const std::vector<ChargingProfile>& valid_profiles, const int32_t evse_id);

    ocpp::DateTime get_period_end_time(const int period_index, const ocpp::DateTime& period_start_time,
                                       const ChargingSchedule& schedule);

    ///
    /// \brief Returns the ChargingSchedulePeriod with the lowest limit. Working under the idea that when there are
    /// ChargingSchedulePeriods that apply within a Profile thenone with the lowest limit takes precedence
    ///
    // ChargingSchedulePeriod lowest_limit(std::vector<ChargingSchedulePeriod> periods);

private:
    std::vector<ChargingProfile> get_evse_specific_tx_default_profiles() const;
    std::vector<ChargingProfile> get_station_wide_tx_default_profiles() const;
    CompositeSchedule initialize_enhanced_composite_schedule(const ocpp::DateTime& start_time,
                                                             const ocpp::DateTime& end_time, const int32_t evse_id,
                                                             ChargingRateUnitEnum charging_rate_unit);
    std::map<ChargingProfilePurposeEnum, LimitStackLevelPair> get_initial_purpose_and_stack_limits();

    std::optional<ocpp::DateTime> get_absolute_profile_start_time(const std::optional<ocpp::DateTime> startSchedule);
    std::optional<ocpp::DateTime>
    get_recurring_profile_start_time(const ocpp::DateTime& time, const std::optional<ocpp::DateTime> startSchedule,
                                     const std::optional<RecurrencyKindEnum> recurrencyKind);
    std::optional<ocpp::DateTime> get_relative_profile_start_time(const int32_t evse_id);
    int get_power_limit(const int limit, const int nr_phases, const ChargingRateUnitEnum& unit_of_limit);
};

} // namespace ocpp::v201

#endif // OCPP_V201_SMART_CHARGING_HPP
