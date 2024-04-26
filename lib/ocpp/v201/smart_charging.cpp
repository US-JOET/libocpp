// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2023 Pionix GmbH and Contributors to EVerest

#include "date/tz.h"
#include "everest/logging.hpp"
#include "ocpp/common/types.hpp"
#include "ocpp/v201/enums.hpp"
#include "ocpp/v201/evse.hpp"
#include "ocpp/v201/messages/SetChargingProfile.hpp"
#include "ocpp/v201/ocpp_types.hpp"
#include "ocpp/v201/transaction.hpp"
#include "ocpp/v201/utils.hpp"

#include <algorithm>
#include <iterator>
#include <memory>
#include <ocpp/v201/smart_charging.hpp>

using namespace std::chrono;

namespace ocpp::v201 {

namespace conversions {
std::string profile_validation_result_to_string(ProfileValidationResultEnum e) {
    switch (e) {
    case ProfileValidationResultEnum::Valid:
        return "Valid";
    case ProfileValidationResultEnum::EvseDoesNotExist:
        return "EvseDoesNotExist";
    case ProfileValidationResultEnum::InvalidProfileType:
        return "InvalidProfileType";
    case ProfileValidationResultEnum::TxProfileMissingTransactionId:
        return "TxProfileMissingTransactionId";
    case ProfileValidationResultEnum::TxProfileEvseIdNotGreaterThanZero:
        return "TxProfileEvseIdNotGreaterThanZero";
    case ProfileValidationResultEnum::TxProfileTransactionNotOnEvse:
        return "TxProfileTransactionNotOnEvse";
    case ProfileValidationResultEnum::TxProfileEvseHasNoActiveTransaction:
        return "TxProfileEvseHasNoActiveTransaction";
    case ProfileValidationResultEnum::TxProfileConflictingStackLevel:
        return "TxProfileConflictingStackLevel";
    case ProfileValidationResultEnum::ChargingProfileNoChargingSchedulePeriods:
        return "ChargingProfileNoChargingSchedulePeriods";
    case ProfileValidationResultEnum::ChargingProfileFirstStartScheduleIsNotZero:
        return "ChargingProfileFirstStartScheduleIsNotZero";
    case ProfileValidationResultEnum::ChargingProfileMissingRequiredStartSchedule:
        return "ChargingProfileMissingRequiredStartSchedule";
    case ProfileValidationResultEnum::ChargingProfileExtraneousStartSchedule:
        return "ChargingProfileExtraneousStartSchedule";
    case ProfileValidationResultEnum::ChargingSchedulePeriodsOutOfOrder:
        return "ChargingSchedulePeriodsOutOfOrder";
    case ProfileValidationResultEnum::ChargingSchedulePeriodInvalidPhaseToUse:
        return "ChargingSchedulePeriodInvalidPhaseToUse";
    case ProfileValidationResultEnum::ChargingSchedulePeriodUnsupportedNumberPhases:
        return "ChargingSchedulePeriodUnsupportedNumberPhases";
    case ProfileValidationResultEnum::ChargingSchedulePeriodExtraneousPhaseValues:
        return "ChargingSchedulePeriodExtraneousPhaseValues";
    case ProfileValidationResultEnum::ChargingStationMaxProfileCannotBeRelative:
        return "ChargingStationMaxProfileCannotBeRelative";
    case ProfileValidationResultEnum::ChargingStationMaxProfileEvseIdGreaterThanZero:
        return "ChargingStationMaxProfileEvseIdGreaterThanZero";
    case ProfileValidationResultEnum::DuplicateTxDefaultProfileFound:
        return "DuplicateTxDefaultProfileFound";
    }

    throw std::out_of_range("No known string conversion for provided enum of type ProfileValidationResultEnum");
}
} // namespace conversions

std::ostream& operator<<(std::ostream& os, const ProfileValidationResultEnum validation_result) {
    os << conversions::profile_validation_result_to_string(validation_result);
    return os;
}

const int32_t STATION_WIDE_ID = 0;

SmartChargingHandler::SmartChargingHandler(std::map<int32_t, std::unique_ptr<EvseInterface>>& evses) : evses(evses) {
}

ProfileValidationResultEnum SmartChargingHandler::validate_evse_exists(int32_t evse_id) const {
    return evses.find(evse_id) == evses.end() ? ProfileValidationResultEnum::EvseDoesNotExist
                                              : ProfileValidationResultEnum::Valid;
}

ProfileValidationResultEnum SmartChargingHandler::validate_charging_station_max_profile(const ChargingProfile& profile,
                                                                                        int32_t evse_id) const {
    if (profile.chargingProfilePurpose != ChargingProfilePurposeEnum::ChargingStationMaxProfile) {
        return ProfileValidationResultEnum::InvalidProfileType;
    }

    if (evse_id > 0) {
        return ProfileValidationResultEnum::ChargingStationMaxProfileEvseIdGreaterThanZero;
    }

    if (profile.chargingProfileKind == ChargingProfileKindEnum::Relative) {
        return ProfileValidationResultEnum::ChargingStationMaxProfileCannotBeRelative;
    }

    return ProfileValidationResultEnum::Valid;
}

ProfileValidationResultEnum SmartChargingHandler::validate_tx_default_profile(ChargingProfile& profile,
                                                                              int32_t evse_id) const {
    auto profiles = evse_id == 0 ? get_evse_specific_tx_default_profiles() : get_station_wide_tx_default_profiles();

    if (is_overlapping_validity_period(evse_id, profile)) {
        return ProfileValidationResultEnum::DuplicateProfileValidityPeriod;
    }

    for (auto candidate : profiles) {
        if (candidate.stackLevel == profile.stackLevel) {
            if (candidate.id != profile.id) {
                return ProfileValidationResultEnum::DuplicateTxDefaultProfileFound;
            }
        }
    }

    return ProfileValidationResultEnum::Valid;
}

ProfileValidationResultEnum SmartChargingHandler::validate_tx_profile(const ChargingProfile& profile,
                                                                      EvseInterface& evse) const {
    if (!profile.transactionId.has_value()) {
        return ProfileValidationResultEnum::TxProfileMissingTransactionId;
    }

    int32_t evseId = evse.get_evse_info().id;
    if (evseId <= 0) {
        return ProfileValidationResultEnum::TxProfileEvseIdNotGreaterThanZero;
    }

    if (!evse.has_active_transaction()) {
        return ProfileValidationResultEnum::TxProfileEvseHasNoActiveTransaction;
    }

    auto& transaction = evse.get_transaction();
    if (transaction->transactionId != profile.transactionId.value()) {
        return ProfileValidationResultEnum::TxProfileTransactionNotOnEvse;
    }

    auto conflicts_with = [&profile](const std::pair<int32_t, std::vector<ChargingProfile>>& candidate) {
        return std::any_of(candidate.second.begin(), candidate.second.end(),
                           [&profile](const ChargingProfile& candidateProfile) {
                               return candidateProfile.transactionId == profile.transactionId &&
                                      candidateProfile.stackLevel == profile.stackLevel;
                           });
    };

    if (std::any_of(charging_profiles.begin(), charging_profiles.end(), conflicts_with)) {
        return ProfileValidationResultEnum::TxProfileConflictingStackLevel;
    }

    return ProfileValidationResultEnum::Valid;
}

/* TODO: Implement the following functional requirements:
 * - K01.FR.20
 * - K01.FR.34
 * - K01.FR.43
 * - K01.FR.48
 */

ProfileValidationResultEnum
SmartChargingHandler::validate_profile_schedules(ChargingProfile& profile,
                                                 std::optional<EvseInterface*> evse_opt) const {
    for (auto& schedule : profile.chargingSchedule) {

        // A schedule must have at least one chargingSchedulePeriod
        if (schedule.chargingSchedulePeriod.empty()) {
            return ProfileValidationResultEnum::ChargingProfileNoChargingSchedulePeriods;
        }

        for (auto i = 0; i < schedule.chargingSchedulePeriod.size(); i++) {
            auto& charging_schedule_period = schedule.chargingSchedulePeriod[i];
            // K01.FR.19
            if (charging_schedule_period.numberPhases != 1 && charging_schedule_period.phaseToUse.has_value()) {
                return ProfileValidationResultEnum::ChargingSchedulePeriodInvalidPhaseToUse;
            }

            // K01.FR.31
            if (i == 0 && charging_schedule_period.startPeriod != 0) {
                return ProfileValidationResultEnum::ChargingProfileFirstStartScheduleIsNotZero;
            }

            // K01.FR.35
            if (i + 1 < schedule.chargingSchedulePeriod.size()) {
                auto next_charging_schedule_period = schedule.chargingSchedulePeriod[i + 1];
                if (next_charging_schedule_period.startPeriod <= charging_schedule_period.startPeriod) {
                    return ProfileValidationResultEnum::ChargingSchedulePeriodsOutOfOrder;
                }
            }

            if (evse_opt.has_value()) {
                auto evse = evse_opt.value();
                // K01.FR.44 for EVSEs; We reject profiles that provide invalid numberPhases/phaseToUse instead
                // of silently acccepting them.
                if (evse->get_current_phase_type() == CurrentPhaseType::DC &&
                    (charging_schedule_period.numberPhases.has_value() ||
                     charging_schedule_period.phaseToUse.has_value())) {
                    return ProfileValidationResultEnum::ChargingSchedulePeriodExtraneousPhaseValues;
                }

                if (evse->get_current_phase_type() == CurrentPhaseType::AC) {
                    // K01.FR.45; Once again rejecting invalid values
                    if (charging_schedule_period.numberPhases.has_value() &&
                        charging_schedule_period.numberPhases > DEFAULT_AND_MAX_NUMBER_PHASES) {
                        return ProfileValidationResultEnum::ChargingSchedulePeriodUnsupportedNumberPhases;
                    }

                    // K01.FR.49
                    if (!charging_schedule_period.numberPhases.has_value()) {
                        charging_schedule_period.numberPhases.emplace(DEFAULT_AND_MAX_NUMBER_PHASES);
                    }
                }
            }
        }

        // K01.FR.40
        if (profile.chargingProfileKind != ChargingProfileKindEnum::Relative && !schedule.startSchedule.has_value()) {
            return ProfileValidationResultEnum::ChargingProfileMissingRequiredStartSchedule;
            // K01.FR.41
        } else if (profile.chargingProfileKind == ChargingProfileKindEnum::Relative &&
                   schedule.startSchedule.has_value()) {
            return ProfileValidationResultEnum::ChargingProfileExtraneousStartSchedule;
        }
    }

    return ProfileValidationResultEnum::Valid;
}

SetChargingProfileResponse SmartChargingHandler::add_profile(int32_t evse_id, ChargingProfile& profile) {
    SetChargingProfileResponse response;
    response.status = ChargingProfileStatusEnum::Accepted;

    auto& profile_storage = (STATION_WIDE_ID == evse_id) ? station_wide_charging_profiles : charging_profiles[evse_id];
    auto profile_with_id =
        std::find_if(profile_storage.begin(), profile_storage.end(),
                     [&profile](const ChargingProfile existing_profile) { return profile.id == existing_profile.id; });

    if (profile_with_id != profile_storage.end() &&
        profile_with_id->chargingProfilePurpose != ChargingProfilePurposeEnum::ChargingStationExternalConstraints) {
        *profile_with_id = profile;
    } else {
        profile_storage.push_back(profile);
    }

    return response;
}

std::vector<ChargingProfile> SmartChargingHandler::get_profiles() {
    auto all_profiles = station_wide_charging_profiles;
    for (auto evse_profile_pair : charging_profiles) {
        all_profiles.insert(all_profiles.end(), evse_profile_pair.second.begin(), evse_profile_pair.second.end());
    }
    return all_profiles;
}

std::vector<ChargingProfile> SmartChargingHandler::get_evse_specific_tx_default_profiles() const {
    std::vector<ChargingProfile> evse_specific_tx_default_profiles;

    for (auto evse_profile_pair : charging_profiles) {
        for (auto profile : evse_profile_pair.second)
            if (profile.chargingProfilePurpose == ChargingProfilePurposeEnum::TxDefaultProfile) {
                evse_specific_tx_default_profiles.push_back(profile);
            }
    }

    return evse_specific_tx_default_profiles;
}

std::vector<ChargingProfile> SmartChargingHandler::get_station_wide_tx_default_profiles() const {
    std::vector<ChargingProfile> station_wide_tx_default_profiles;
    for (auto profile : station_wide_charging_profiles) {
        if (profile.chargingProfilePurpose == ChargingProfilePurposeEnum::TxDefaultProfile) {
            station_wide_tx_default_profiles.push_back(profile);
        }
    }

    return station_wide_tx_default_profiles;
}

void log_period_date_time_pair(PeriodDateTimePair period_date_time_pair) {
    std::string log_str = "PeriodDateTimePair> ";

    if (period_date_time_pair.period.has_value()) {
        log_str += " period: " + utils::to_string(period_date_time_pair.period.value());
        // log_str += " period: " + std::to_string(period_date_time_pair.period.value().startPeriod);
        // EVLOG_info << "   Period: " << period_date_time_pair.period.value().startPeriod
        //            << " End Time: " << period_date_time_pair.end_time.to_rfc3339();
    }
    log_str += " end_time: " + period_date_time_pair.end_time.to_rfc3339();
    EVLOG_info << log_str;
}

// TODO: The structs type is float but it's being passed in as an int. Significant?
int get_requested_limit(const int limit, const int nr_phases, const ChargingRateUnitEnum& requested_unit) {
    if (requested_unit == ChargingRateUnitEnum::A) {
        return limit / (LOW_VOLTAGE * nr_phases);
    } else {
        return limit;
    }
}

CompositeSchedule
SmartChargingHandler::initialize_enhanced_composite_schedule(const ocpp::DateTime& start_time,
                                                             const ocpp::DateTime& end_time, const int32_t evse_id,
                                                             ChargingRateUnitEnum charging_rate_unit) {
    CompositeSchedule composite_schedule;

    composite_schedule.evseId = evse_id;
    composite_schedule.duration = duration_cast<seconds>(end_time.to_time_point() - start_time.to_time_point()).count();
    composite_schedule.scheduleStart = start_time;
    composite_schedule.chargingRateUnit = charging_rate_unit;

    return composite_schedule;
}

int32_t SmartChargingHandler::determine_duration(const ocpp::DateTime& start_time, const ocpp::DateTime& end_time) {
    return duration_cast<seconds>(end_time.to_time_point() - start_time.to_time_point()).count();
}

bool SmartChargingHandler::within_time_window(const ocpp::DateTime& end_time, const ocpp::DateTime& temp_time) {
    return SmartChargingHandler::determine_duration(temp_time, end_time) > 0;
    // auto end_time_point = end_time.to_time_point();
    // auto temp_time_point = temp_time.to_time_point();
    // auto count = duration_cast<seconds>(end_time_point - temp_time_point).count();

    // EVLOG_info << "";
    // EVLOG_info << "time_window> " << temp_time.to_rfc3339() << " - " << end_time.to_rfc3339() << " = " << count << "
    // ("
    //            << get_log_duration_string(count) << ")";
    // if (count > 0) {
    //     return true;
    // } else {
    //     EVLOG_info << "time_window> count = " << count << " RETURN FALSE ";
    //     return false;
    // }
}

PeriodDateTimePair SmartChargingHandler::find_period_at(const ocpp::DateTime& time, const ChargingProfile& profile,
                                                        const int evse_id) {
    auto period_start_time = this->get_profile_start_time(profile, time, evse_id);

    EVLOG_info << "#" << profile.id << " find_period_at> " << period_start_time.value().to_rfc3339();

    // TODO: Only processing first charging schedule
    const auto schedule = profile.chargingSchedule.front();

    if (period_start_time) {
        const auto periods = schedule.chargingSchedulePeriod;
        time_point<date::utc_clock> period_end_time;
        for (size_t i = 0; i < periods.size(); i++) {
            const auto period_end_time = get_period_end_time(i, period_start_time.value(), schedule);

            EVLOG_info << "   find_period_at>        start_time> " << time.to_rfc3339();
            EVLOG_info << "   find_period_at> period_start_time> " << period_start_time.value().to_rfc3339();
            EVLOG_info << "   find_period_at>   period_end_time> " << period_end_time.to_rfc3339();

            if (time >= period_start_time.value() && time < period_end_time) {
                PeriodDateTimePair date_time_pair = {periods.at(i), ocpp::DateTime(period_end_time)};
                log_period_date_time_pair(date_time_pair);
                return date_time_pair;
            }
            period_start_time.emplace(ocpp::DateTime(period_end_time));
        }
    }

    PeriodDateTimePair date_time_pair = {std::nullopt, MAX_DATE_TIME};
    log_period_date_time_pair(date_time_pair);
    return date_time_pair;
}

///
/// \brief Calculates the composite schedule for the given \p valid_profiles and the given \p connector_id
///
/// Step 01-000 - Pass in profiles, start and end time, evse od add charging_rate_unit
/// Step 01-001 - initialize_enhanced_composite_schedule(start_time, end_time, evse_id, charging_rate_unit)
/// Step 01-002 - Instantiate vector of ChargingSchedulePeriod periods
CompositeSchedule SmartChargingHandler::calculate_composite_schedule(std::vector<ChargingProfile> valid_profiles,
                                                                     const ocpp::DateTime& start_time,
                                                                     const ocpp::DateTime& end_time,
                                                                     const int32_t evse_id,
                                                                     ChargingRateUnitEnum charging_rate_unit) {

    EVLOG_info << "01-001 - initialize_enhanced_composite_schedule(start_time, end_time, evse_id, charging_rate_unit)";
    CompositeSchedule composite_schedule =
        this->initialize_enhanced_composite_schedule(start_time, end_time, evse_id, charging_rate_unit);

    /// Step 01-002 - Instantiate vector of ChargingSchedulePeriod periods
    std::vector<ChargingSchedulePeriod> periods;

    ocpp::DateTime temp_time(start_time);
    ocpp::DateTime last_period_end_time(end_time);
    int current_period_limit = MAX_PERIOD_LIMIT;
    LimitStackLevelPair significant_limit_stack_level_pair = {MAX_PERIOD_LIMIT, -1};

    // calculate every ChargingSchedulePeriod of result within this while loop
    while (SmartChargingHandler::within_time_window(end_time, temp_time)) {
        // while (duration_cast<seconds>(end_time.to_time_point() - temp_time.to_time_point()).count() > 0) {

        std::map<ChargingProfilePurposeEnum, LimitStackLevelPair> current_purpose_and_stack_limits =
            get_initial_purpose_and_stack_limits(); // this data structure holds the current lowest limit and stack
                                                    // level for every purpose
        ocpp::DateTime temp_period_end_time;
        int32_t temp_number_phases;

        for (const ChargingProfile& profile : valid_profiles) {
            EVLOG_info << "ProfileId #" << profile.id << " Kind: " << profile.chargingProfileKind;

            if (profile.stackLevel > current_purpose_and_stack_limits.at(profile.chargingProfilePurpose).stack_level) {
                // EVLOG_info << "calculate_composite_schedule> profile.stackLevel > current_purpose_and_stack_limits";
                EVLOG_info << "boop";

                // this data structure holds the respective period and period end time for temp_time point in time
                const PeriodDateTimePair period_date_time_pair = this->find_period_at(temp_time, profile, evse_id);

                if (period_date_time_pair.period.has_value()) {
                    const ChargingSchedulePeriod period = period_date_time_pair.period.value();

                    temp_period_end_time = period_date_time_pair.end_time;

                    if (period.numberPhases) {
                        temp_number_phases = period.numberPhases.value();
                    } else {
                        temp_number_phases = DEFAULT_AND_MAX_NUMBER_PHASES;
                    }

                    ChargingSchedule first_charging_schedule = profile.chargingSchedule.front();

                    float limit =
                        get_power_limit(period.limit, temp_number_phases, first_charging_schedule.chargingRateUnit);
                    int32_t stackLevel = profile.stackLevel;

                    EVLOG_info << "period.has_value() limit = " << limit;
                    EVLOG_info << "period.has_value() stackLevel = " << limit;

                    // update data structure with limit and stack level for this profile
                    current_purpose_and_stack_limits.at(profile.chargingProfilePurpose).limit = limit;
                    current_purpose_and_stack_limits.at(profile.chargingProfilePurpose).stack_level = stackLevel;
                } else {
                    // skip profiles with a lower stack level if a higher stack level already has a limit for this
                    // point in time
                    continue;
                }
            }
        };

        // break;

        // if there is a limit with purpose TxProfile it overrules the limit of purpose TxDefaultProfile
        if (current_purpose_and_stack_limits.at(ChargingProfilePurposeEnum::TxProfile).limit != MAX_PERIOD_LIMIT) {
            significant_limit_stack_level_pair =
                current_purpose_and_stack_limits.at(ChargingProfilePurposeEnum::TxProfile);
        } else {
            significant_limit_stack_level_pair =
                current_purpose_and_stack_limits.at(ChargingProfilePurposeEnum::TxDefaultProfile);
        }

        if (current_purpose_and_stack_limits.at(ChargingProfilePurposeEnum::ChargingStationMaxProfile).limit <
            significant_limit_stack_level_pair.limit) {
            significant_limit_stack_level_pair =
                current_purpose_and_stack_limits.at(ChargingProfilePurposeEnum::ChargingStationMaxProfile);
        }

        bool should_insert_period = significant_limit_stack_level_pair.limit != current_period_limit and
                                    significant_limit_stack_level_pair.limit != MAX_PERIOD_LIMIT;

        EVLOG_debug << "stack_level_pair.limit(" << significant_limit_stack_level_pair.limit
                    << ") != current_period_limit(" << current_period_limit << ") and stack_level_pair.limit("
                    << significant_limit_stack_level_pair.limit << ") != MAX_PERIOD_LIMIT " << MAX_PERIOD_LIMIT
                    << " == " << should_insert_period;

        // insert new period to result only if limit changed or period was found
        if (should_insert_period) {

            ChargingSchedulePeriod new_period = ChargingSchedulePeriod{
                .startPeriod =
                    static_cast<int32_t>(duration_cast<seconds>(temp_time.to_time_point() - start_time.to_time_point())
                                             .count()), // Declare and assign the value of start_period to startPeriod
                .limit = static_cast<float>(get_requested_limit(
                    significant_limit_stack_level_pair.limit, // TODO: Again why float to int to float?
                    temp_number_phases, charging_rate_unit)),
                .numberPhases = temp_number_phases,
            };

            EVLOG_debug << "calculate_composite_schedule> pushing period " << utils::to_string(new_period);

            periods.push_back(new_period);

            last_period_end_time = temp_period_end_time;
            current_period_limit = significant_limit_stack_level_pair.limit;
        }
        temp_time = this->get_next_temp_time(temp_time, valid_profiles, evse_id);
    }

    // update duration if end time of last period is smaller than requested end time
    if (last_period_end_time.to_time_point() - start_time.to_time_point() <
        (end_time.to_time_point() - start_time.to_time_point())) {
        composite_schedule.duration =
            duration_cast<seconds>(last_period_end_time.to_time_point() - start_time.to_time_point()).count();
    }
    composite_schedule.chargingSchedulePeriod = periods;

    return composite_schedule;
}

ocpp::DateTime get_current_period_end_time(const ocpp::DateTime& period_start_time,
                                           std::optional<int32_t> charging_schedule_duration,
                                           const ChargingSchedulePeriod& period) {
    return period_start_time;
}

ocpp::DateTime SmartChargingHandler::get_period_end_time(const int period_index,
                                                         const ocpp::DateTime& period_start_time,
                                                         const ChargingSchedule& schedule) {

    const std::vector<ChargingSchedulePeriod> periods = schedule.chargingSchedulePeriod;

    std::optional<ocpp::DateTime> period_end_time;

    int period_diff_in_seconds;
    if ((size_t)period_index + 1 < periods.size()) {
        int duration;
        if (schedule.duration) {
            duration = schedule.duration.value();
        } else {
            duration = MAX_PERIOD_LIMIT;
        }

        if (periods.at(period_index + 1).startPeriod < duration) {
            period_diff_in_seconds = periods.at(period_index + 1).startPeriod - periods.at(period_index).startPeriod;
        } else {
            period_diff_in_seconds = duration - periods.at(period_index).startPeriod;
        }
        const ocpp::DateTime dt = ocpp::DateTime(period_start_time.to_time_point() + seconds(period_diff_in_seconds));
        return dt;
    } else if (schedule.duration) {
        period_diff_in_seconds = schedule.duration.value() - periods.at(period_index).startPeriod;
        return ocpp::DateTime(period_start_time.to_time_point() + seconds(period_diff_in_seconds));
    } else {
        return MAX_DATE_TIME;
    }
}

bool continue_time_arrow(const ocpp::DateTime& temp_time, const ocpp::DateTime& period_start_time,
                         const ocpp::DateTime& period_end_time, const ocpp::DateTime& lowest_next_time) {
    // original
    // return (temp_time >= period_start_time && temp_time < period_end_time && period_end_time < lowest_next_time);

    return (temp_time < period_end_time && period_end_time < lowest_next_time);
}

// Step 1 - lowest_next_time is set to maximum time in the future
// Step 2 - Iterate through the profiles
// Step 3 - Get first starting schedule (only one currently supported)
// Step 4 - Get period_start_time and continue if available
// Step 5 - Iterate through the ChargingSchedulePeriods
// Step 6 - Get Period end time
// Step 7 - Continue if not within final time window
ocpp::DateTime SmartChargingHandler::get_next_temp_time(const ocpp::DateTime temp_time,
                                                        const std::vector<ChargingProfile>& valid_profiles,
                                                        const int32_t evse_id) {
    EVLOG_debug << "get_next_temp_time> temp_time = " << temp_time;

    // Step 1 - lowest_next_time is set to maximum tine in the future
    ocpp::DateTime lowest_next_time = MAX_DATE_TIME;

    EVLOG_debug << "get_next_temp_time> lowest_next_time = " << lowest_next_time;

    // Step 2 - Iterate through the profiles
    for (const ChargingProfile& profile : valid_profiles) {
        EVLOG_debug << "get_next_temp_time> ChargingProfile #" << profile.id;

        if (profile.chargingSchedule.size() > 1) {
            // TODO: Add support for Profiles with more than one ChargingSchedule.
            EVLOG_warning << "Charging Profiles with more than one ChargingSchedule are not currently supported.";
        }

        // Step 3 - Get first starting schedule (only one currently supported)
        ChargingSchedule schedule = profile.chargingSchedule.front();
        const std::vector<ChargingSchedulePeriod> periods = schedule.chargingSchedulePeriod;

        // Step 4 - Get period_start_time and continue if available
        const std::optional<ocpp::DateTime> period_start_time_opt =
            this->get_profile_start_time(profile, temp_time, evse_id);

        if (period_start_time_opt.has_value()) {
            ocpp::DateTime period_start_time = period_start_time_opt.value();

            EVLOG_debug << "get_next_temp_time> ChargingSchedule #" << schedule.id
                        << " duration: " << utils::get_log_duration_string(schedule.duration.value())
                        << " startSchedule: " << schedule.startSchedule.value();

            // Step 5 - Iterate through the ChargingSchedulePeriods
            for (size_t i = 0; i < periods.size(); i++) {
                ChargingSchedulePeriod period = periods.at(i);

                // for (const ChargingSchedulePeriod period : schedule.chargingSchedulePeriod) {
                EVLOG_debug << "get_next_temp_time> ChargingSchedulePeriod #" << i << " limit: " << period.limit
                            << " startPeriod: " << period.startPeriod;

                // Step 6 - Get Period end time
                const ocpp::DateTime period_end_time = get_period_end_time(i, period_start_time, schedule);
                EVLOG_debug << "get_next_temp_time> period_end_time: " << period_end_time;

                bool within_window =
                    continue_time_arrow(temp_time, period_start_time, period_end_time, lowest_next_time);

                EVLOG_debug << "get_next_temp_time> Profile #" << profile.id << " " << temp_time << " < "
                            << period_end_time << " && " << period_end_time << " < " << lowest_next_time << " = "
                            << within_window;

                // Step 7 - Continue if not within final time window
                if (within_window) {
                    lowest_next_time = period_end_time;
                    EVLOG_debug << "get_next_temp_time> Profile #" << profile.id << " " << lowest_next_time
                                << " is new lowest_next_time";
                } else {
                    EVLOG_debug << "get_next_temp_time> Profile #" << profile.id << " " << lowest_next_time
                                << " is current lowest_next_time NO CHANGE";
                }

                period_start_time = period_end_time;
            }
        }
    }
    return lowest_next_time;
}

std::optional<ocpp::DateTime>
SmartChargingHandler::get_absolute_profile_start_time(const std::optional<ocpp::DateTime> startSchedule) {
    std::optional<ocpp::DateTime> period_start_time;

    if (startSchedule) {
        period_start_time.emplace(ocpp::DateTime(floor<seconds>(startSchedule.value().to_time_point())));
    } else {
        EVLOG_warning << "Absolute profile with no startSchedule, this should not be possible";
    }

    return period_start_time;
}

std::optional<ocpp::DateTime>
SmartChargingHandler::get_recurring_profile_start_time(const ocpp::DateTime& time,
                                                       const std::optional<ocpp::DateTime> startSchedule,
                                                       const std::optional<RecurrencyKindEnum> recurrencyKind) {
    std::optional<ocpp::DateTime> period_start_time;

    if (startSchedule) {
        // TODO ?: What if startSchedule is not set? This use case is not handled in the 1.6 code
        const ocpp::DateTime start_schedule =
            ocpp::DateTime(std::chrono::floor<seconds>(startSchedule.value().to_time_point()));
        int seconds_to_go_back;

        if (recurrencyKind.value() == RecurrencyKindEnum::Daily) {
            seconds_to_go_back = duration_cast<seconds>(time.to_time_point() - start_schedule.to_time_point()).count() %
                                 (HOURS_PER_DAY * SECONDS_PER_HOUR);
        } else {
            seconds_to_go_back = duration_cast<seconds>(time.to_time_point() - start_schedule.to_time_point()).count() %
                                 (SECONDS_PER_DAY * DAYS_PER_WEEK);
        }
        auto time_minus_seconds_to_go_back = time.to_time_point() - seconds(seconds_to_go_back);
        period_start_time.emplace(ocpp::DateTime(time_minus_seconds_to_go_back));
    } else {
        EVLOG_warning << "Recurring profile with no startSchedule, this should not be possible";
    }

    return period_start_time;
}

std::optional<ocpp::DateTime> SmartChargingHandler::get_relative_profile_start_time(const int32_t evse_id) {
    std::optional<ocpp::DateTime> period_start_time;

    if (this->evses.at(evse_id)) {
        EVLOG_debug << "And we are in!";
    }

    return period_start_time;
}

std::optional<ocpp::DateTime> SmartChargingHandler::get_profile_start_time(const ChargingProfile& profile,
                                                                           const ocpp::DateTime& time,
                                                                           const int32_t evse_id) {

    std::optional<ocpp::DateTime> period_start_time;

    // TODO add test logic for returning only one value for multiple Charging Schedules. Currently not supported.
    for (const ChargingSchedule schedule : profile.chargingSchedule) {
        if (profile.chargingProfileKind == ChargingProfileKindEnum::Absolute) {
            period_start_time = SmartChargingHandler::get_absolute_profile_start_time(schedule.startSchedule);
        } else if (profile.chargingProfileKind == ChargingProfileKindEnum::Relative) {

            // if (this->evses(evse_id))

            // Python psuedo code from Dr. Dan "The Man" Moore
            // def calculate_relative_profile_charging_schedule_periods_start_time(charging_station, relative_profile,
            //                                                                     profile_receipt_time, transaction) :
            //     if transaction.is_active and transaction.id == relative_profile.transactionId:
            //     : return calculate_relative_profile_charging_schedule_periods_start_time_in_active_transaction(
            //             charging_station, transaction, profile_receipt_time) else
            //     : return THE END OF TIME

            //           def
            //           calculate_relative_profile_charging_schedule_periods_start_time_in_active_transaction(
            //               charging_station, transaction, profile_receipt_time) :
            //     if PowerPathClosed in charging_station.device_model.TxCtrlr.TxStartPoint
            //     : transaction_profile_activation_time = transaction.power_path_closed_time else
            //     : transaction_profile_activation_time = transaction.start_time

            //                                             if profile_receipt_time <=
            //                                             transaction_profile_activation_time
            //     : return transaction_profile_activation_time else : return profile_receipt_time

            // TODO
            // if (this->evses.at(evse_id)->has_active_transaction()) {
            //     period_start_time.emplace(ocpp::DateTime(floor<seconds>(
            //         this->evses.at(evse_id)->get_transaction()->get_start_energy_wh()->timestamp.to_time_point())));
            // }
        } else if (profile.chargingProfileKind == ChargingProfileKindEnum::Recurring) {
            period_start_time = SmartChargingHandler::get_recurring_profile_start_time(time, schedule.startSchedule,
                                                                                       profile.recurrencyKind);
        }

        // EVLOG_info << "ChargingProfile: " << to_string(profile);
    }
    EVLOG_verbose << "get_profile_start_time> profile #" << profile.id << " temp_time: " << time.to_rfc3339()
                  << " period_start_time: " << period_start_time.value().to_rfc3339() << " EVSE_ID #" << evse_id;
    return period_start_time;
}

std::map<ChargingProfilePurposeEnum, LimitStackLevelPair> SmartChargingHandler::get_initial_purpose_and_stack_limits() {
    std::map<ChargingProfilePurposeEnum, LimitStackLevelPair> map;
    map[ChargingProfilePurposeEnum::ChargingStationMaxProfile] = {MAX_PERIOD_LIMIT, -1};
    map[ChargingProfilePurposeEnum::TxDefaultProfile] = {MAX_PERIOD_LIMIT, -1};
    map[ChargingProfilePurposeEnum::TxProfile] = {MAX_PERIOD_LIMIT, -1};
    return map;
}

int SmartChargingHandler::get_power_limit(const int limit, const int nr_phases,
                                          const ChargingRateUnitEnum& unit_of_limit) {
    if (unit_of_limit == ChargingRateUnitEnum::W) {
        return limit;
    } else {
        return limit * LOW_VOLTAGE * nr_phases;
    }
}

bool SmartChargingHandler::is_overlapping_validity_period(int candidate_evse_id,
                                                          ChargingProfile& candidate_profile) const {

    conform_validity_periods(candidate_profile);

    if (candidate_profile.chargingProfilePurpose == ChargingProfilePurposeEnum::TxProfile) {
        // This only applies to non TxProfile types.
        return false;
    }

    auto conflicts_with = [candidate_evse_id, &candidate_profile](
                              const std::pair<int32_t, std::vector<ChargingProfile>>& existing_profiles) {
        auto existing_evse_id = existing_profiles.first;
        if (existing_evse_id == candidate_evse_id) {
            return std::any_of(existing_profiles.second.begin(), existing_profiles.second.end(),
                               [&candidate_profile](const ChargingProfile& existing_profile) {
                                   if (existing_profile.stackLevel == candidate_profile.stackLevel &&
                                       existing_profile.chargingProfileKind == candidate_profile.chargingProfileKind &&
                                       existing_profile.id != candidate_profile.id) {

                                       return candidate_profile.validFrom <= existing_profile.validTo &&
                                              candidate_profile.validTo >= existing_profile.validFrom; // reject
                                   }
                                   return false;
                               });
        }
        return false;
    };

    return std::any_of(charging_profiles.begin(), charging_profiles.end(), conflicts_with);
}

void SmartChargingHandler::conform_validity_periods(ChargingProfile& profile) const {
    profile.validFrom =
        profile.validFrom.has_value() ? profile.validFrom.value() : ocpp::DateTime(date::utc_clock::now());
    profile.validTo =
        profile.validTo.has_value() ? profile.validTo.value() : ocpp::DateTime(date::utc_clock::time_point::max());
}

} // namespace ocpp::v201
