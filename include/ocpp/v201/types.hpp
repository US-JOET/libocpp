// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2023 Pionix GmbH and Contributors to EVerest
#ifndef V201_TYPES_HPP
#define V201_TYPES_HPP

#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include <ocpp/common/types.hpp>

#include <nlohmann/json_fwd.hpp>

using json = nlohmann::json;

namespace ocpp {
namespace v201 {

/// \brief Contains all supported OCPP 2.0.1 message types
enum class MessageType {
    Authorize,
    AuthorizeResponse,
    BootNotification,
    BootNotificationResponse,
    CancelReservation,
    CancelReservationResponse,
    CertificateSigned,
    CertificateSignedResponse,
    ChangeAvailability,
    ChangeAvailabilityResponse,
    ClearCache,
    ClearCacheResponse,
    ClearChargingProfile,
    ClearChargingProfileResponse,
    ClearDisplayMessage,
    ClearDisplayMessageResponse,
    ClearedChargingLimit,
    ClearedChargingLimitResponse,
    ClearVariableMonitoring,
    ClearVariableMonitoringResponse,
    CostUpdated,
    CostUpdatedResponse,
    CustomerInformation,
    CustomerInformationResponse,
    DataTransfer,
    DataTransferResponse,
    DeleteCertificate,
    DeleteCertificateResponse,
    FirmwareStatusNotification,
    FirmwareStatusNotificationResponse,
    Get15118EVCertificate,
    Get15118EVCertificateResponse,
    GetBaseReport,
    GetBaseReportResponse,
    GetCertificateStatus,
    GetCertificateStatusResponse,
    GetChargingProfiles,
    GetChargingProfilesResponse,
    GetCompositeSchedule,
    GetCompositeScheduleResponse,
    GetDisplayMessages,
    GetDisplayMessagesResponse,
    GetInstalledCertificateIds,
    GetInstalledCertificateIdsResponse,
    GetLocalListVersion,
    GetLocalListVersionResponse,
    GetLog,
    GetLogResponse,
    GetMonitoringReport,
    GetMonitoringReportResponse,
    GetReport,
    GetReportResponse,
    GetTransactionStatus,
    GetTransactionStatusResponse,
    GetVariables,
    GetVariablesResponse,
    Heartbeat,
    HeartbeatResponse,
    InstallCertificate,
    InstallCertificateResponse,
    LogStatusNotification,
    LogStatusNotificationResponse,
    MeterValues,
    MeterValuesResponse,
    NotifyChargingLimit,
    NotifyChargingLimitResponse,
    NotifyCustomerInformation,
    NotifyCustomerInformationResponse,
    NotifyDisplayMessages,
    NotifyDisplayMessagesResponse,
    NotifyEVChargingNeeds,
    NotifyEVChargingNeedsResponse,
    NotifyEVChargingSchedule,
    NotifyEVChargingScheduleResponse,
    NotifyEvent,
    NotifyEventResponse,
    NotifyMonitoringReport,
    NotifyMonitoringReportResponse,
    NotifyReport,
    NotifyReportResponse,
    PublishFirmware,
    PublishFirmwareResponse,
    PublishFirmwareStatusNotification,
    PublishFirmwareStatusNotificationResponse,
    ReportChargingProfiles,
    ReportChargingProfilesResponse,
    RequestStartTransaction,
    RequestStartTransactionResponse,
    RequestStopTransaction,
    RequestStopTransactionResponse,
    ReservationStatusUpdate,
    ReservationStatusUpdateResponse,
    ReserveNow,
    ReserveNowResponse,
    Reset,
    ResetResponse,
    SecurityEventNotification,
    SecurityEventNotificationResponse,
    SendLocalList,
    SendLocalListResponse,
    SetChargingProfile,
    SetChargingProfileResponse,
    SetDisplayMessage,
    SetDisplayMessageResponse,
    SetMonitoringBase,
    SetMonitoringBaseResponse,
    SetMonitoringLevel,
    SetMonitoringLevelResponse,
    SetNetworkProfile,
    SetNetworkProfileResponse,
    SetVariableMonitoring,
    SetVariableMonitoringResponse,
    SetVariables,
    SetVariablesResponse,
    SignCertificate,
    SignCertificateResponse,
    StatusNotification,
    StatusNotificationResponse,
    TransactionEvent,
    TransactionEventResponse,
    TriggerMessage,
    TriggerMessageResponse,
    UnlockConnector,
    UnlockConnectorResponse,
    UnpublishFirmware,
    UnpublishFirmwareResponse,
    UpdateFirmware,
    UpdateFirmwareResponse,
    InternalError, // not in spec, for internal use
};

namespace conversions {
/// \brief Converts the given MessageType \p m to std::string
/// \returns a string representation of the MessageType
std::string messagetype_to_string(MessageType m);

/// \brief Converts the given std::string \p s to MessageType
/// \returns a MessageType from a string representation
MessageType string_to_messagetype(const std::string& s);

} // namespace conversions

/// \brief Writes the string representation of the given \p message_type to the given output stream \p os
/// \returns an output stream with the MessageType written to
std::ostream& operator<<(std::ostream& os, const MessageType& message_type);

// struct ChargingSchedulePeriod {
//     int32_t startPeriod;
//     float limit;
//     std::optional<int32_t> numberPhases;
// };
// /// \brief Conversion from a given ChargingSchedulePeriod \p k to a given json object \p j
// void to_json(json& j, const ChargingSchedulePeriod& k);
// /// \brief Conversion from a given json object \p j to a given ChargingSchedulePeriod \p k
// void from_json(const json& j, ChargingSchedulePeriod& k);

// // /// \brief Enhances ChargingSchedule by containing std::vector<EnhancedChargingSchedulePeriods> instead of
// // /// std::vector<ChargingSchedulePeriod>
// struct ChargingSchedule {
//     ChargingRateUnitEnum chargingRateUnit;
//     std::vector<ChargingSchedulePeriod> chargingSchedulePeriod;
//     std::optional<int32_t> duration;
//     std::optional<ocpp::DateTime> startSchedule;
//     std::optional<float> minChargingRate;
// };
// /// \brief Conversion from a given ChargingSchedule \p k to a given json object \p j
// void to_json(json& j, const ChargingSchedule& k);
// /// \brief Conversion from a given json object \p j to a given ChargingSchedule \p k
// void from_json(const json& j, ChargingSchedule& k);

} // namespace v201
} // namespace ocpp

#endif
