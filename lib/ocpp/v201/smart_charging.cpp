// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2023 Pionix GmbH and Contributors to EVerest

#include "everest/logging.hpp"
#include "ocpp/common/types.hpp"
#include "ocpp/v201/enums.hpp"
#include "ocpp/v201/evse.hpp"
#include "ocpp/v201/ocpp_types.hpp"
#include "ocpp/v201/transaction.hpp"
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

ProfileValidationResultEnum SmartChargingHandler::validate_tx_default_profile(const ChargingProfile& profile,
                                                                              int32_t evse_id) const {
    auto profiles = evse_id == 0 ? get_evse_specific_tx_default_profiles() : get_station_wide_tx_default_profiles();
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
    for (ChargingSchedule& schedule : profile.chargingSchedule) {
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

void SmartChargingHandler::add_profile(int32_t evse_id, ChargingProfile& profile) {
    if (STATION_WIDE_ID == evse_id) {
        station_wide_charging_profiles.push_back(profile);
    } else {
        charging_profiles[evse_id].push_back(profile);
    }
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

std::string to_string(ChargingProfile cp) {
    json cp_json;
    to_json(cp_json, cp);

    return cp_json.dump(4);
}

std::string to_string(ChargingSchedule cs) {
    json cs_json;
    to_json(cs_json, cs);

    return cs_json.dump(4);
}

std::string to_string(ChargingSchedulePeriod csp) {
    json csp_json;
    to_json(csp_json, csp);

    return csp_json.dump(4);
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

bool SmartChargingHandler::within_time_window(const ocpp::DateTime& start_time, const ocpp::DateTime& end_time) {
    return SmartChargingHandler::determine_duration(start_time, end_time) > 0;
}

PeriodDateTimePair SmartChargingHandler::find_period_at(const ocpp::DateTime& time, const ChargingProfile& profile,
                                                        const int evse_id) {
    PeriodDateTimePair ptp;

    auto period_start_time = this->get_profile_start_time(profile, time, evse_id);
    EVLOG_info << "#" << profile.id << " find_period_at> " << period_start_time.value().to_rfc3339();

    if (period_start_time) {
    }

    return ptp;
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
    int current_period_limit = std::numeric_limits<int>::max();
    LimitStackLevelPair significant_limit_stack_level_pair = {std::numeric_limits<int>::max(), -1};

    // calculate every ChargingSchedulePeriod of result within this while loop
    while (SmartChargingHandler::within_time_window(end_time, temp_time)) {
        std::map<ChargingProfilePurposeEnum, LimitStackLevelPair> current_purpose_and_stack_limits =
            get_initial_purpose_and_stack_limits(); // this data structure holds the current lowest limit and stack
                                                    // level for every purpose
        ocpp::DateTime temp_period_end_time;
        int32_t temp_number_phases;

        for (const ChargingProfile& profile : valid_profiles) {
            // EVLOG_info << "boop";
            EVLOG_info << "ProfileId #" << profile.id << " Kind: " << profile.chargingProfileKind;

            if (profile.stackLevel > current_purpose_and_stack_limits.at(profile.chargingProfilePurpose).stack_level) {
                // EVLOG_info << "calculate_composite_schedule> profile.stackLevel > current_purpose_and_stack_limits";
                EVLOG_info << "boop";

                // this data structure holds the respective period and period end time for temp_time point in time
                const PeriodDateTimePair period_date_time_pair = this->find_period_at(temp_time, profile, evse_id);

                if (period_date_time_pair.period) {
                    const ChargingSchedulePeriod period = period_date_time_pair.period.value();

                    temp_period_end_time = period_date_time_pair.end_time;

                    if (period.numberPhases) {
                        temp_number_phases = period.numberPhases.value();
                    } else {
                        temp_number_phases = DEFAULT_AND_MAX_NUMBER_PHASES;
                    }

                    ChargingSchedule first_charging_schedule = profile.chargingSchedule.front();

                    // update data structure with limit and stack level for this profile
                    current_purpose_and_stack_limits.at(profile.chargingProfilePurpose).limit =
                        get_power_limit(period.limit, temp_number_phases, first_charging_schedule.chargingRateUnit);
                    current_purpose_and_stack_limits.at(profile.chargingProfilePurpose).stack_level =
                        profile.stackLevel;
                } else {
                    // skip profiles with a lower stack level if a higher stack level already has a limit for this
                    // point in time
                    continue;
                }
            }
        };

        // break;

        // if there is a limit with purpose TxProfile it overrules the limit of purpose TxDefaultProfile
        if (current_purpose_and_stack_limits.at(ChargingProfilePurposeEnum::TxProfile).limit !=
            std::numeric_limits<int>::max()) {
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

        // insert new period to result only if limit changed or period was found
        if (significant_limit_stack_level_pair.limit != current_period_limit and
            significant_limit_stack_level_pair.limit != std::numeric_limits<int>::max()) {

            const auto start_period =
                duration_cast<seconds>(temp_time.to_time_point() - start_time.to_time_point()).count();

            ChargingSchedulePeriod new_period = ChargingSchedulePeriod{
                .startPeriod =
                    static_cast<int32_t>(duration_cast<seconds>(temp_time.to_time_point() - start_time.to_time_point())
                                             .count()), // Declare and assign the value of start_period to startPeriod
                .limit = static_cast<float>(get_requested_limit(
                    significant_limit_stack_level_pair.limit, // TODO: Again why float to int to float?
                    temp_number_phases, charging_rate_unit)),
                .numberPhases = temp_number_phases,
            };
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
                                                         const ChargingSchedule& schedule,
                                                         const std::vector<ChargingSchedulePeriod>& periods) {
    std::optional<ocpp::DateTime> period_end_time;

    int period_diff_in_seconds;
    if ((size_t)period_index + 1 < periods.size()) {
        int duration;
        if (schedule.duration) {
            duration = schedule.duration.value();
        } else {
            duration = std::numeric_limits<int>::max();
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
        return ocpp::DateTime(date::utc_clock::now() + hours(std::numeric_limits<int>::max()));
    }
}

bool continue_time_arrow(const ocpp::DateTime& temp_time, const ocpp::DateTime& period_start_time,
                         const ocpp::DateTime& period_end_time, const ocpp::DateTime& lowest_next_time) {
    // original
    // return (temp_time >= period_start_time && temp_time < period_end_time && period_end_time < lowest_next_time);

    return (temp_time < period_end_time && period_end_time < lowest_next_time);
}

// Step 1 - lowest_next_time is set to maximum tine in the future
// Step 2 - Iterate through the profiles
// Step 3 - Get first starting schedule (only one currently supported)
// Step 4 - Get period_start_time and continue if available
// Step 5 - Iterate through the ChargingSchedulePeriods
// Step 6 - Get Period end time
// Step 7 - Continue if not within final time window
ocpp::DateTime SmartChargingHandler::get_next_temp_time(const ocpp::DateTime temp_time,
                                                        const std::vector<ChargingProfile>& valid_profiles,
                                                        const int32_t evse_id) {
    // Step 1 - lowest_next_time is set to maximum tine in the future
    ocpp::DateTime lowest_next_time = ocpp::DateTime(date::utc_clock::now() + hours(std::numeric_limits<int>::max()));

    // Step 2 - Iterate through the profiles
    for (const ChargingProfile& profile : valid_profiles) {
        EVLOG_debug << "ChargingProfile> " << to_string(profile);

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

            // Step 5 - Iterate through the ChargingSchedulePeriods
            for (size_t i = 0; i < periods.size(); i++) {
                // for (const ChargingSchedulePeriod period : schedule.chargingSchedulePeriod) {
                EVLOG_debug << "ChargingSchedulePeriod> " << to_string(periods.at(i));

                // Step 6 - Get Period end time
                const ocpp::DateTime period_end_time = get_period_end_time(i, period_start_time, schedule, periods);

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
    EVLOG_verbose << "get_profile_start_time> " << to_string(profile) << " temp_time: " << time.to_rfc3339()
                  << " period_start_time: " << period_start_time.value().to_rfc3339() << " EVSE_ID #" << evse_id;
    return period_start_time;
}

std::map<ChargingProfilePurposeEnum, LimitStackLevelPair> SmartChargingHandler::get_initial_purpose_and_stack_limits() {
    std::map<ChargingProfilePurposeEnum, LimitStackLevelPair> map;
    map[ChargingProfilePurposeEnum::ChargingStationMaxProfile] = {std::numeric_limits<int>::max(), -1};
    map[ChargingProfilePurposeEnum::TxDefaultProfile] = {std::numeric_limits<int>::max(), -1};
    map[ChargingProfilePurposeEnum::TxProfile] = {std::numeric_limits<int>::max(), -1};
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

// ChargingSchedulePeriod SmartChargingHandler::lowest_limit(std::vector<ChargingSchedulePeriod> periods) {

// }

} // namespace ocpp::v201
