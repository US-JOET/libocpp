// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2023 Pionix GmbH and Contributors to EVerest

#ifndef OCPP_V201_SMART_CHARGING_HPP
#define OCPP_V201_SMART_CHARGING_HPP

#include "ocpp/v201/device_model.hpp"
#include "ocpp/v201/enums.hpp"
#include "ocpp/v201/messages/SetChargingProfile.hpp"
#include <limits>

#include <memory>
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

const ocpp::DateTime MAX_DATE_TIME = ocpp::DateTime(std::chrono::time_point<date::utc_clock>().max());
const int MAX_PERIOD_LIMIT = std::numeric_limits<int>::max();

enum class ProfileValidationResultEnum {
    Valid,
    EvseDoesNotExist,
    InvalidProfileType,
    TxProfileMissingTransactionId,
    TxProfileEvseIdNotGreaterThanZero,
    TxProfileTransactionNotOnEvse,
    TxProfileEvseHasNoActiveTransaction,
    TxProfileConflictingStackLevel,
    ChargingProfileNoChargingSchedulePeriods,
    ChargingProfileFirstStartScheduleIsNotZero,
    ChargingProfileMissingRequiredStartSchedule,
    ChargingProfileExtraneousStartSchedule,
    ChargingScheduleChargingRateUnitUnsupported,
    ChargingSchedulePeriodsOutOfOrder,
    ChargingSchedulePeriodInvalidPhaseToUse,
    ChargingSchedulePeriodUnsupportedNumberPhases,
    ChargingSchedulePeriodExtraneousPhaseValues,
    ChargingSchedulePeriodPhaseToUseACPhaseSwitchingUnsupported,
    ChargingStationMaxProfileCannotBeRelative,
    ChargingStationMaxProfileEvseIdGreaterThanZero,
    DuplicateTxDefaultProfileFound,
    DuplicateProfileValidityPeriod
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
    std::shared_ptr<DeviceModel>& device_model;

    std::shared_ptr<ocpp::v201::DatabaseHandler> database_handler;
    // cppcheck-suppress unusedStructMember
    std::map<int32_t, std::vector<ChargingProfile>> charging_profiles;
    std::vector<ChargingProfile> station_wide_charging_profiles;

public:
    explicit SmartChargingHandler(std::map<int32_t, std::unique_ptr<EvseInterface>>& evses,
                                  std::shared_ptr<DeviceModel>& device_model);

    ///
    /// \brief validates the given \p profile according to the specification.
    /// If a profile does not have validFrom or validTo set, we conform the values
    /// to a representation that fits the spec.
    ///
    ProfileValidationResultEnum validate_profile(ChargingProfile& profile, int32_t evse_id);

    ///
    /// \brief Adds a given \p profile and associated \p evse_id to our stored list of profiles
    ///
    SetChargingProfileResponse add_profile(int32_t evse_id, ChargingProfile& profile);

    ///
    /// \brief Retrieves existing profiles on system.
    ///
    std::vector<ChargingProfile> get_profiles();

    CompositeSchedule calculate_composite_schedule(std::vector<ChargingProfile> valid_profiles,
                                                   const ocpp::DateTime& start_time, const ocpp::DateTime& end_time,
                                                   const ocpp::DateTime& activation_time, const int32_t evse_id,
                                                   ChargingRateUnitEnum charging_rate_unit);

    CompositeSchedule calculate_composite_schedule(std::vector<ChargingProfile> valid_profiles,
                                                   const ocpp::DateTime& start_time, const ocpp::DateTime& end_time,
                                                   const int32_t evse_id, ChargingRateUnitEnum charging_rate_unit);

    std::vector<ChargingProfile> align_profiles_for_composite_schedule(std::vector<ChargingProfile> valid_profiles,
                                                                       const ocpp::DateTime& activation_time,
                                                                       const int32_t evse_id);

    bool profile_transaction_active_on_evse(const ChargingProfile& profile, int evse_id) const;

    CompositeSchedule calculate_enhanced_composite_schedule(const std::vector<ChargingProfile>& valid_profiles,
                                                            const ocpp::DateTime& start_time,
                                                            const ocpp::DateTime& end_time, const int32_t evse_id,
                                                            ChargingRateUnitEnum charging_rate_unit);

protected:
    ///
    /// \brief validates the existence of the given \p evse_id according to the specification
    ///
    ProfileValidationResultEnum validate_evse_exists(int32_t evse_id) const;

    ///
    /// \brief validates requirements that apply only to the ChargingStationMaxProfile \p profile
    /// according to the specification
    ///
    ProfileValidationResultEnum validate_charging_station_max_profile(const ChargingProfile& profile,
                                                                      int32_t evse_id) const;

    ///
    /// \brief validates the given \p profile and associated \p evse_id according to the specification
    ///
    ProfileValidationResultEnum validate_tx_default_profile(ChargingProfile& profile, int32_t evse_id) const;

    ///
    /// \brief validates the given \p profile according to the specification
    ///
    ProfileValidationResultEnum validate_tx_profile(const ChargingProfile& profile, int32_t evse_id) const;

    /// \brief validates that the given \p profile has valid charging schedules.
    /// If a profiles charging schedule period does not have a valid numberPhases,
    /// we set it to the default value (3).
    ProfileValidationResultEnum validate_profile_schedules(ChargingProfile& profile,
                                                           std::optional<EvseInterface*> evse_opt = std::nullopt) const;

    ///
    /// \brief Checks a given \p profile and associated \p evse_id validFrom and validTo range
    /// This method assumes that the existing profile will have dates set for validFrom and validTo
    ///
    bool is_overlapping_validity_period(int evse_id, const ChargingProfile& profile) const;

    ///
    /// \brief Calculates the composite schedule for the given \p valid_profiles and the given \p connector_id
    ///
    CompositeSchedule calculate_composite_schedule_algo(std::vector<ChargingProfile> valid_profiles,
                                                        const ocpp::DateTime& start_time,
                                                        const ocpp::DateTime& end_time, const int32_t evse_id,
                                                        ChargingRateUnitEnum charging_rate_unit);

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

    static int32_t determine_duration(const ocpp::DateTime& start_time, const ocpp::DateTime& end_time);

    static bool within_time_window(const ocpp::DateTime& start_time, const ocpp::DateTime& end_time);

    ///
    /// \brief Converts a Relative ChargingProfile to an Absolute ChargingProfile with a start schedule
    /// time of now.
    ///
    static ChargingProfile convert_relative_to_absolute(const ChargingProfile& relative_profile);

    ///
    /// \brief Converts a Relative ChargingProfile to an Absolute ChargingProfile, setting the start schedule
    /// to the time passed in.
    ///
    static ChargingProfile convert_relative_to_absolute(const ChargingProfile& relative_profile,
                                                        const ocpp::DateTime& start_schedule);

    ///
    /// \brief Iterates over the periods of the given \p valid_profiles and determines the earliest next absolute
    /// period end time later than \p temp_time
    ///
    ocpp::DateTime get_next_temp_time(const ocpp::DateTime temp_time,
                                      const std::vector<ChargingProfile>& valid_profiles, const int32_t evse_id);

    ocpp::DateTime get_period_end_time(const int period_index, const ocpp::DateTime& period_start_time,
                                       const ChargingSchedule& schedule);

    ocpp::DateTime get_now();

private:
    std::vector<ChargingProfile> get_evse_specific_tx_default_profiles() const;
    std::vector<ChargingProfile> get_station_wide_tx_default_profiles() const;
    void conform_validity_periods(ChargingProfile& profile) const;
    CurrentPhaseType get_current_phase_type(const std::optional<EvseInterface*> evse_opt) const;
    CompositeSchedule initialize_composite_schedule(const ocpp::DateTime& start_time, const ocpp::DateTime& end_time,
                                                    const int32_t evse_id, ChargingRateUnitEnum charging_rate_unit);
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
