// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2023 Pionix GmbH and Contributors to EVerest

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

SmartChargingHandler::SmartChargingHandler() {
}

ProfileValidationResultEnum SmartChargingHandler::validate_tx_default_profile(const ChargingProfile& profile,
                                                                              Evse& evse) const {
    auto profiles =
        evse.is_station_wide() ? get_evse_specific_tx_default_profiles() : get_station_wide_tx_default_profiles();
    for (auto iter = profiles.begin(); iter != profiles.end(); ++iter) {
        if (iter->profile.stackLevel == profile.stackLevel) {
            if (iter->profile.id != profile.id) {
                return ProfileValidationResultEnum::DuplicateTxDefaultProfileFound;
            }
        }
    }

    return ProfileValidationResultEnum::Valid;
}

ProfileValidationResultEnum SmartChargingHandler::validate_tx_profile(const ChargingProfile& profile,
                                                                      Evse& evse) const {
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

    auto conflicts_with = [&profile](const EvseProfile& candidate) {
        return candidate.profile.transactionId == profile.transactionId &&
               candidate.profile.stackLevel == profile.stackLevel;
    };
    if (std::any_of(charging_profiles.begin(), charging_profiles.end(), conflicts_with)) {
        return ProfileValidationResultEnum::TxProfileConflictingStackLevel;
    }

    return ProfileValidationResultEnum::Valid;
}

void SmartChargingHandler::add_profile(int32_t evse_id, ChargingProfile& profile) {
    charging_profiles.push_back(EvseProfile{evse_id = evse_id, profile = profile});
}

std::vector<EvseProfile> SmartChargingHandler::get_evse_specific_tx_default_profiles() const {
    std::vector<EvseProfile> evse_specific_tx_default_profiles;
    auto pred = [](const EvseProfile& candidate) {
        return !Evse::is_station_wide_id(candidate.evse_id) &&
               candidate.profile.chargingProfilePurpose == ChargingProfilePurposeEnum::TxDefaultProfile;
    };
    std::copy_if(charging_profiles.begin(), charging_profiles.end(),
                 std::back_inserter(evse_specific_tx_default_profiles), pred);

    return evse_specific_tx_default_profiles;
}

std::vector<EvseProfile> SmartChargingHandler::get_station_wide_tx_default_profiles() const {
    std::vector<EvseProfile> station_wide_tx_default_profiles;
    auto pred = [](const EvseProfile& candidate) {
        return Evse::is_station_wide_id(candidate.evse_id) &&
               candidate.profile.chargingProfilePurpose == ChargingProfilePurposeEnum::TxDefaultProfile;
    };
    std::copy_if(charging_profiles.begin(), charging_profiles.end(),
                 std::back_inserter(station_wide_tx_default_profiles), pred);

    return station_wide_tx_default_profiles;
}

} // namespace ocpp::v201
